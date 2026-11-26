#include "terminalview.h"

#include <ghostty/vt.h>

#include <QDebug>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QQuickWindow>
#include <QSocketNotifier>

#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <libblight.h>
#include <libblight/clock.h>
#include <libblight/connection.h>
#include <liboxide/oxideqml.h>

TerminalView::TerminalView(QQuickItem* parent)
  : QQuickPaintedItem(parent) {
  setAcceptedMouseButtons(Qt::AllButtons);
  setFlag(ItemAcceptsInputMethod, true);
  setFlag(ItemIsFocusScope, true);
  setFocusPolicy(Qt::StrongFocus);
  setFocus(true);
  m_title = "Terminal";
  GhosttyTerminal terminal = nullptr;
  ghostty_terminal_new(nullptr, &terminal, 80, 24);
  ghostty_terminal_set(terminal, GHOSTTY_TERMINAL_OPT_USERDATA, this);
  GhosttyTerminalTitleChangedFn titleCb =
    [](GhosttyTerminal t, void* userdata) {
      GhosttyString title = {nullptr, 0};
      ghostty_terminal_get(t, GHOSTTY_TERMINAL_DATA_TITLE, &title);
      TerminalView* self = static_cast<TerminalView*>(userdata);
      if (title.ptr && title.len > 0) {
        QString qtitle = QString::fromUtf8(
          reinterpret_cast<const char*>(title.ptr), static_cast<int>(title.len)
        );
        QMetaObject::invokeMethod(self, "setTitle", Q_ARG(QString, qtitle));
      }
    };
  ghostty_terminal_set(terminal, GHOSTTY_TERMINAL_OPT_TITLE_CHANGED, &titleCb);
  GhosttyColorRgb fg = {0, 0, 0};
  GhosttyColorRgb bg = {255, 255, 255};
  ghostty_terminal_set(terminal, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND, &fg);
  ghostty_terminal_set(terminal, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND, &bg);
  m_terminal = terminal;

  GhosttyRenderState rs = nullptr;
  ghostty_render_state_new(nullptr, &rs);
  m_renderState = rs;

  GhosttyRenderStateRowIterator ri = nullptr;
  ghostty_render_state_row_iterator_new(nullptr, &ri);
  m_rowIter = ri;

  GhosttyRenderStateRowCells cells = nullptr;
  ghostty_render_state_row_cells_new(nullptr, &cells);
  m_cells = cells;

  m_font = QFont("monospace");
  m_font.setStyleHint(QFont::Monospace);
  m_font.setPointSize(12);
  QFontMetricsF fm(m_font);
  m_charWidth = fm.horizontalAdvance(QChar('M'));
  if (m_charWidth <= 0) {
    m_charWidth = 8;
  }
  m_charHeight = fm.height();
  if (m_charHeight <= 0) {
    m_charHeight = 14;
  }

  QSize gridSize(m_cols * m_charWidth, m_rows * m_charHeight);
  m_drawn = QImage(gridSize, QImage::Format_ARGB32_Premultiplied);
  m_drawn.fill(Qt::black);
  if (m_terminal != nullptr) {
    ghostty_terminal_resize(
      reinterpret_cast<GhosttyTerminal>(m_terminal),
      80,
      24,
      static_cast<uint32_t>(m_charWidth),
      static_cast<uint32_t>(m_charHeight)
    );
  }
  m_cellCache.resize(m_cols * m_rows);
  m_lastPaint.reset();
  m_paintTimer = new QTimer(this);
  m_paintTimer->setSingleShot(true);
  connect(m_paintTimer, &QTimer::timeout, this, &TerminalView::applyPending);
  openPty();
}

TerminalView::~TerminalView() {
  if (m_paintTimer != nullptr) {
    m_paintTimer->stop();
    delete m_paintTimer;
    m_paintTimer = nullptr;
  }
  if (m_notifier != nullptr) {
    delete m_notifier;
    m_notifier = nullptr;
  }
  if (m_notifierException != nullptr) {
    delete m_notifierException;
    m_notifierException = nullptr;
  }
  if (m_masterFd >= 0) {
    close(m_masterFd);
    m_masterFd = -1;
  }
  if (m_child > 0) {
    kill(m_child, SIGHUP);
    waitpid(m_child, nullptr, 0);
    m_child = -1;
  }
  if (m_renderState != nullptr) {
    ghostty_render_state_free(
      reinterpret_cast<GhosttyRenderState>(m_renderState)
    );
    m_renderState = nullptr;
  }
  if (m_rowIter != nullptr) {
    ghostty_render_state_row_iterator_free(
      reinterpret_cast<GhosttyRenderStateRowIterator>(m_rowIter)
    );
    m_rowIter = nullptr;
  }
  if (m_cells != nullptr) {
    ghostty_render_state_row_cells_free(
      reinterpret_cast<GhosttyRenderStateRowCells>(m_cells)
    );
    m_cells = nullptr;
  }
  if (m_terminal != nullptr) {
    ghostty_terminal_free(reinterpret_cast<GhosttyTerminal>(m_terminal));
    m_terminal = nullptr;
  }
}

void
TerminalView::paint(QPainter* painter) {
  if (!m_drawn.isNull()) {
    painter->drawImage(boundingRect(), m_drawn);
  }
}

void
TerminalView::geometryChange(
  const QRectF& newGeometry,
  const QRectF& oldGeometry
) {
  QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
  auto size = newGeometry.size().toSize();
  if (
    size.isEmpty() || m_charWidth <= 0 || m_charHeight <= 0 ||
    window() == nullptr
  ) {
    return;
  }
  auto* contentItem = window()->contentItem();
  if (contentItem == nullptr) {
    return;
  }
  int cols = static_cast<int>(width() / m_charWidth);
  int rows = static_cast<int>(height() / m_charHeight);
  if (cols < 2) {
    cols = 2;
  }
  if (rows < 2) {
    rows = 2;
  }
  if (cols == m_cols && rows == m_rows) {
    return;
  }
  qDebug() << "New size" << rows << cols;
  m_cols = cols;
  m_rows = rows;
  m_cellCache.resize(m_cols * m_rows);
  if (m_terminal != nullptr) {
    ghostty_terminal_resize(
      reinterpret_cast<GhosttyTerminal>(m_terminal),
      static_cast<uint16_t>(m_cols),
      static_cast<uint16_t>(m_rows),
      static_cast<uint32_t>(m_charWidth),
      static_cast<uint32_t>(m_charHeight)
    );
  }
  if (m_masterFd >= 0) {
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_row = static_cast<unsigned short>(m_rows);
    ws.ws_col = static_cast<unsigned short>(m_cols);
    ioctl(m_masterFd, TIOCSWINSZ, &ws);
  }
  m_drawn = QImage(
    QSize(m_cols * m_charWidth, m_rows * m_charHeight),
    QImage::Format_ARGB32_Premultiplied
  );
  m_drawn.fill(Qt::black);
  m_pending = QRegion();
  renderAllCells();
  renderCursor();
  update();
}

void
TerminalView::keyPressEvent(QKeyEvent* event) {
  QByteArray data = keyToBytes(event);
  if (!data.isEmpty() && m_masterFd >= 0) {
    // TODO properly handle errors and continuing writing partial data
    ::write(m_masterFd, data.constData(), data.size());
  }
  event->accept();
}

void
TerminalView::setTitle(const QString& title) {
  if (m_title != title) {
    m_title = title;
    emit titleChanged(m_title);
  }
}

bool
TerminalView::inputMethodVisible() const {
  return true;
}

void
TerminalView::renderAllCells() {
  if (m_terminal == nullptr || m_renderState == nullptr || m_drawn.isNull()) {
    return;
  }
  QPainter painter(&m_drawn);
  painter.setFont(m_font);

  GhosttyTerminal terminal = reinterpret_cast<GhosttyTerminal>(m_terminal);
  GhosttyRenderState renderState =
    reinterpret_cast<GhosttyRenderState>(m_renderState);
  GhosttyRenderStateRowIterator rowIter =
    reinterpret_cast<GhosttyRenderStateRowIterator>(m_rowIter);
  GhosttyRenderStateRowCells cells =
    reinterpret_cast<GhosttyRenderStateRowCells>(m_cells);

  ghostty_render_state_update(renderState, terminal);
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &rowIter
  );
  GhosttyColorRgb fg = {200, 200, 200};
  GhosttyColorRgb bg = {0, 0, 0};
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_COLOR_FOREGROUND, &fg
  );
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_COLOR_BACKGROUND, &bg
  );

  uint16_t y = 0;
  while (ghostty_render_state_row_iterator_next(rowIter)) {
    ghostty_render_state_row_get(
      rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells
    );
    int x = 0;
    while (ghostty_render_state_row_cells_next(cells)) {
      GhosttyStyle style = {};
      ghostty_render_state_row_cells_get(
        cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style
      );
      char buf[256];
      GhosttyBuffer gbuf;
      gbuf.ptr = reinterpret_cast<uint8_t*>(buf);
      gbuf.cap = sizeof(buf) - 1;
      gbuf.len = 0;
      ghostty_render_state_row_cells_get(
        cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &gbuf
      );
      std::string text(reinterpret_cast<const char*>(gbuf.ptr), gbuf.len);
      renderCell(painter, x, y, style, text, fg, bg, m_charWidth, m_charHeight);
      size_t cacheIdx = y * m_cols + x;
      if (cacheIdx < m_cellCache.size()) {
        m_cellCache[cacheIdx].style = style;
        m_cellCache[cacheIdx].text = std::move(text);
      }
      x++;
    }
    y++;
  }
  m_pending +=
    QRectF(0, 0, m_cols * m_charWidth, m_rows * m_charHeight).toRect();
  m_rowIter = rowIter;
  m_cells = cells;
  ghostty_render_state_clean(renderState);
}

void
TerminalView::renderDirtyCells() {
  if (m_terminal == nullptr || m_renderState == nullptr || m_drawn.isNull()) {
    return;
  }
  QPainter painter(&m_drawn);
  painter.setFont(m_font);

  GhosttyRenderState renderState =
    reinterpret_cast<GhosttyRenderState>(m_renderState);
  GhosttyRenderStateRowIterator rowIter =
    reinterpret_cast<GhosttyRenderStateRowIterator>(m_rowIter);
  GhosttyRenderStateRowCells cells =
    reinterpret_cast<GhosttyRenderStateRowCells>(m_cells);
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &rowIter
  );
  GhosttyColorRgb fg = {200, 200, 200};
  GhosttyColorRgb bg = {0, 0, 0};
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_COLOR_FOREGROUND, &fg
  );
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_COLOR_BACKGROUND, &bg
  );

  uint16_t y = 0;
  int changedCells = 0;
  while (ghostty_render_state_row_iterator_next_dirty(rowIter, &y)) {
    ghostty_render_state_row_get(
      rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells
    );
    int cx = 0;
    while (ghostty_render_state_row_cells_next(cells)) {
      GhosttyStyle style = {};
      ghostty_render_state_row_cells_get(
        cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style
      );

      char buf[256];
      GhosttyBuffer gbuf;
      gbuf.ptr = reinterpret_cast<uint8_t*>(buf);
      gbuf.cap = sizeof(buf) - 1;
      gbuf.len = 0;
      ghostty_render_state_row_cells_get(
        cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &gbuf
      );
      std::string text(reinterpret_cast<const char*>(gbuf.ptr), gbuf.len);
      size_t cacheIdx = y * m_cols + cx;
      if (cacheIdx < m_cellCache.size()) {
        const CachedCell& cached = m_cellCache[cacheIdx];
        if (
          cached.style.flags != style.flags ||
          cached.style.fg_color.tag != style.fg_color.tag ||
          (style.fg_color.tag == GHOSTTY_STYLE_COLOR_RGB &&
           (cached.style.fg_color.value.rgb.r != style.fg_color.value.rgb.r ||
            cached.style.fg_color.value.rgb.g != style.fg_color.value.rgb.g ||
            cached.style.fg_color.value.rgb.b != style.fg_color.value.rgb.b)) ||
          (style.fg_color.tag == GHOSTTY_STYLE_COLOR_PALETTE &&
           cached.style.fg_color.value.palette !=
             style.fg_color.value.palette) ||
          cached.style.bg_color.tag != style.bg_color.tag ||
          (style.bg_color.tag == GHOSTTY_STYLE_COLOR_RGB &&
           (cached.style.bg_color.value.rgb.r != style.bg_color.value.rgb.r ||
            cached.style.bg_color.value.rgb.g != style.bg_color.value.rgb.g ||
            cached.style.bg_color.value.rgb.b != style.bg_color.value.rgb.b)) ||
          (style.bg_color.tag == GHOSTTY_STYLE_COLOR_PALETTE &&
           cached.style.bg_color.value.palette !=
             style.bg_color.value.palette) ||
          cached.style.underline_color.tag != style.underline_color.tag ||
          (style.underline_color.tag == GHOSTTY_STYLE_COLOR_RGB &&
           (cached.style.underline_color.value.rgb.r !=
              style.underline_color.value.rgb.r ||
            cached.style.underline_color.value.rgb.g !=
              style.underline_color.value.rgb.g ||
            cached.style.underline_color.value.rgb.b !=
              style.underline_color.value.rgb.b)) ||
          (style.underline_color.tag == GHOSTTY_STYLE_COLOR_PALETTE &&
           cached.style.underline_color.value.palette !=
             style.underline_color.value.palette) ||
          cached.text != text
        ) {
          renderCell(
            painter, cx, y, style, text, fg, bg, m_charWidth, m_charHeight
          );
          m_pending +=
            QRectF(
              cx * m_charWidth, y * m_charHeight, m_charWidth, m_charHeight
            )
              .toRect();
          changedCells++;
        }
      }
      if (cacheIdx < m_cellCache.size()) {
        m_cellCache[cacheIdx].style = style;
        m_cellCache[cacheIdx].text = std::move(text);
      }
      cx++;
    }
  }
  m_rowIter = rowIter;
  m_cells = cells;
  ghostty_render_state_clean(renderState);
}

void
TerminalView::renderCursor() {
  QPainter painter(&m_drawn);
  painter.setFont(m_font);
  GhosttyRenderState renderState =
    reinterpret_cast<GhosttyRenderState>(m_renderState);
  GhosttyColorRgb fg = {200, 200, 200};
  GhosttyColorRgb bg = {0, 0, 0};
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_COLOR_FOREGROUND, &fg
  );
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_COLOR_BACKGROUND, &bg
  );
  bool cursorVisible = false;
  if (
    ghostty_render_state_get(
      renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE, &cursorVisible
    ) == GHOSTTY_SUCCESS
  ) {
    bool hasViewport = false;
    ghostty_render_state_get(
      renderState,
      GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE,
      &hasViewport
    );
    if (hasViewport) {
      uint16_t cx = 0, cy = 0;
      if (
        ghostty_render_state_get(
          renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X, &cx
        ) == GHOSTTY_SUCCESS &&
        ghostty_render_state_get(
          renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y, &cy
        ) == GHOSTTY_SUCCESS
      ) {
        if (
          m_lastCursorX >= 0 && m_lastCursorY >= 0 &&
          (cx != static_cast<uint16_t>(m_lastCursorX) ||
           cy != static_cast<uint16_t>(m_lastCursorY))
        ) {
          renderCellAtPosition(painter, m_lastCursorX, m_lastCursorY, fg, bg);
          m_pending += QRectF(
                         m_lastCursorX * m_charWidth,
                         m_lastCursorY * m_charHeight,
                         m_charWidth,
                         m_charHeight
          )
                         .toRect();
        }
        if (cursorVisible) {
          painter.fillRect(
            QRectF(
              cx * m_charWidth, cy * m_charHeight, m_charWidth, m_charHeight
            ),
            QColor(fg.r, fg.g, fg.b)
          );
          m_pending +=
            QRectF(
              cx * m_charWidth, cy * m_charHeight, m_charWidth, m_charHeight
            )
              .toRect();
        }
        m_lastCursorX = static_cast<int>(cx);
        m_lastCursorY = static_cast<int>(cy);
      }
    } else if (m_lastCursorX >= 0 && m_lastCursorY >= 0) {
      // Clear the previous cursor
      renderCellAtPosition(painter, m_lastCursorX, m_lastCursorY, fg, bg);
      m_pending += QRectF(
                     m_lastCursorX * m_charWidth,
                     m_lastCursorY * m_charHeight,
                     m_charWidth,
                     m_charHeight
      )
                     .toRect();
      m_lastCursorX = -1;
      m_lastCursorY = -1;
    }
  } else if (m_lastCursorX >= 0 && m_lastCursorY >= 0) {
    // Clear the previous cursor
    renderCellAtPosition(painter, m_lastCursorX, m_lastCursorY, fg, bg);
    m_pending += QRectF(
                   m_lastCursorX * m_charWidth,
                   m_lastCursorY * m_charHeight,
                   m_charWidth,
                   m_charHeight
    )
                   .toRect();
    m_lastCursorX = -1;
    m_lastCursorY = -1;
  }
}

void
TerminalView::render() {
  if (!m_terminal || !m_renderState || m_drawn.isNull()) {
    return;
  }
  GhosttyTerminal terminal = reinterpret_cast<GhosttyTerminal>(m_terminal);
  GhosttyRenderState renderState =
    reinterpret_cast<GhosttyRenderState>(m_renderState);
  ghostty_render_state_update(renderState, terminal);
  GhosttyRenderStateDirty dirtyState = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirtyState
  );
  if (dirtyState == GHOSTTY_RENDER_STATE_DIRTY_FALSE) {
    ghostty_render_state_clean(renderState);
    return;
  }
  switch (dirtyState) {
    case GHOSTTY_RENDER_STATE_DIRTY_PARTIAL:
      renderDirtyCells();
      break;
    case GHOSTTY_RENDER_STATE_DIRTY_FULL:
    default:
      renderAllCells();
      break;
  }
  renderCursor();
}

void
TerminalView::renderCell(
  QPainter& painter,
  int x,
  int y,
  const GhosttyStyle& style,
  const std::string& text,
  const GhosttyColorRgb& fg,
  const GhosttyColorRgb& bg,
  qreal charWidth,
  qreal charHeight
) {
  painter.setFont(m_font);
  GhosttyColorRgb cellFg = fg;
  GhosttyColorRgb cellBg = bg;
  switch (style.fg_color.tag) {
    case GHOSTTY_STYLE_COLOR_NONE:
      cellFg = fg;
      break;
    case GHOSTTY_STYLE_COLOR_RGB:
      cellFg = style.fg_color.value.rgb;
      break;
    case GHOSTTY_STYLE_COLOR_PALETTE:
      cellFg = paletteIndexToRgb(style.fg_color.value.palette);
      break;
  }
  switch (style.bg_color.tag) {
    case GHOSTTY_STYLE_COLOR_NONE:
      cellBg = bg;
      break;
    case GHOSTTY_STYLE_COLOR_RGB:
      cellBg = style.bg_color.value.rgb;
      break;
    case GHOSTTY_STYLE_COLOR_PALETTE:
      cellBg = paletteIndexToRgb(style.bg_color.value.palette);
      break;
  }
  painter.fillRect(
    QRectF(x * charWidth, y * charHeight, charWidth, charHeight),
    QColor(cellBg.r, cellBg.g, cellBg.b)
  );
  if (text.empty()) {
    return;
  }
  painter.setPen(QColor(cellFg.r, cellFg.g, cellFg.b));
  painter.drawText(
    QPointF(
      x * charWidth, (y + 1) * charHeight - QFontMetricsF(m_font).descent()
    ),
    QString::fromUtf8(text.c_str(), static_cast<int>(text.size()))
  );
}

GhosttyColorRgb
TerminalView::paletteIndexToRgb(uint8_t index) {
  // Standard 256-color palette
  if (index < 16) {
    // 16 base colors
    static const GhosttyColorRgb base[16] = {
      {0,   0,   0  }, // 0: black
      {128, 0,   0  }, // 1: red
      {0,   128, 0  }, // 2: green
      {128, 128, 0  }, // 3: yellow
      {0,   0,   128}, // 4: blue
      {128, 0,   128}, // 5: magenta
      {0,   128, 128}, // 6: cyan
      {192, 192, 192}, // 7: white
      {128, 128, 128}, // 8: bright black
      {255, 0,   0  }, // 9: bright red
      {0,   255, 0  }, // 10: bright green
      {255, 255, 0  }, // 11: bright yellow
      {0,   0,   255}, // 12: bright blue
      {255, 0,   255}, // 13: bright magenta
      {0,   255, 255}, // 14: bright cyan
      {255, 255, 255}  // 15: bright white
    };
    return base[index];
  } else if (index < 232) {
    // 216-color cube (6x6x6)
    uint8_t idx = index - 16;
    uint8_t r = idx / 36;
    uint8_t g = (idx % 36) / 6;
    uint8_t b = idx % 6;
    static const uint8_t cube[6] = {0, 95, 135, 175, 215, 255};
    return {cube[r], cube[g], cube[b]};
  } else {
    // 24 grayscale
    uint8_t v = 8 + (index - 232) * 10;
    return {v, v, v};
  }
}

void
TerminalView::renderCellAtPosition(
  QPainter& painter,
  int x,
  int y,
  const GhosttyColorRgb& fg,
  const GhosttyColorRgb& bg
) {
  if (!m_terminal || !m_renderState) {
    return;
  }

  painter.setFont(m_font);

  GhosttyRenderState renderState =
    reinterpret_cast<GhosttyRenderState>(m_renderState);
  GhosttyRenderStateRowIterator rowIter =
    reinterpret_cast<GhosttyRenderStateRowIterator>(m_rowIter);
  GhosttyRenderStateRowCells cells =
    reinterpret_cast<GhosttyRenderStateRowCells>(m_cells);

  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &rowIter
  );

  uint16_t row = 0;
  while (ghostty_render_state_row_iterator_next(rowIter)) {
    if (row == static_cast<uint16_t>(y)) {
      ghostty_render_state_row_get(
        rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells
      );
      int col = 0;
      while (ghostty_render_state_row_cells_next(cells)) {
        if (col == x) {
          char buf[256];
          GhosttyBuffer gbuf;
          gbuf.ptr = reinterpret_cast<uint8_t*>(buf);
          gbuf.cap = sizeof(buf) - 1;
          gbuf.len = 0;
          GhosttyResult r = ghostty_render_state_row_cells_get(
            cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &gbuf
          );
          GhosttyColorRgb cellFg = fg;
          ghostty_render_state_row_cells_get(
            cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &cellFg
          );
          GhosttyColorRgb cellBg = bg;
          ghostty_render_state_row_cells_get(
            cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &cellBg
          );
          painter.fillRect(
            QRectF(
              x * m_charWidth, y * m_charHeight, m_charWidth, m_charHeight
            ),
            QColor(cellBg.r, cellBg.g, cellBg.b)
          );
          if (r == GHOSTTY_SUCCESS && gbuf.len > 0) {
            if (gbuf.len >= sizeof(buf)) {
              gbuf.len = sizeof(buf) - 1;
            }
            buf[gbuf.len] = '\0';
            QString text = QString::fromUtf8(buf, static_cast<int>(gbuf.len));
            painter.setPen(QColor(cellFg.r, cellFg.g, cellFg.b));
            painter.drawText(
              QPointF(
                x * m_charWidth,
                (y + 1) * m_charHeight - QFontMetricsF(m_font).descent()
              ),
              text
            );
          }
          break;
        }
        col++;
      }
      break;
    }
    row++;
  }

  m_rowIter = rowIter;
  m_cells = cells;
}

void
TerminalView::applyPending() {
  if (m_pending.isEmpty()) {
    return;
  }
  auto* window = this->window();
  if (window == nullptr) {
    return;
  }
  auto* contentItem = window->contentItem();
  if (contentItem == nullptr) {
    return;
  }
  double elapsed = m_lastPaint.elapsed();
  if (elapsed <= 0.016) {
    int remainingMs = static_cast<int>((0.016 - elapsed) * 1000.0);
    if (remainingMs < 1) {
      remainingMs = 1;
    }
    m_paintTimer->start(remainingMs);
    return;
  }
  QImage windowImage = Oxide::QML::getImageForWindow(window);
  if (windowImage.isNull()) {
    return;
  }
  m_lastPaint.reset();

  QRegion pending = m_pending;
  m_pending = QRegion();

  QRect boundingRect = pending.boundingRect();
  QRectF sourceRect(
    boundingRect.x(),
    boundingRect.y(),
    boundingRect.width(),
    boundingRect.height()
  );
  QPainter painter(&windowImage);
  painter.setFont(m_font);
  QPointF topLeft = mapToScene(QPointF(0, 0));
  QPointF topRight = mapToScene(QPointF(1, 0));
  QPointF bottomLeft = mapToScene(QPointF(0, 1));
  QTransform sceneTransform;
  sceneTransform.translate(topLeft.x(), topLeft.y());
  sceneTransform.rotate(qRadiansToDegrees(
    qAtan2(topRight.y() - topLeft.y(), topRight.x() - topLeft.x())
  ));
  sceneTransform.scale(
    QLineF(topLeft, topRight).length(), QLineF(topLeft, bottomLeft).length()
  );
  painter.setTransform(sceneTransform, true);
  painter.drawImage(sourceRect, m_drawn, sourceRect);

  for (const QRect& rect : pending) {
    Oxide::QML::repaint(
      window,
      sceneTransform.mapRect(QRectF(rect)),
      Blight::WaveformMode::UI,
      Blight::ContentType::Color,
      Blight::UpdateMode::PartialUpdate,
      false
    );
  }
}
void
TerminalView::openPty() {
  int slave = -1;
  if (openpty(&m_masterFd, &slave, nullptr, nullptr, nullptr) != 0) {
    qWarning() << "openpty failed";
    return;
  }
  fcntl(m_masterFd, F_SETFL, O_NONBLOCK);

  m_child = fork();
  if (m_child < 0) {
    qWarning() << "fork failed";
    close(slave);
    return;
  }
  if (m_child == 0) {
    close(m_masterFd);
    setsid();
    dup2(slave, STDIN_FILENO);
    dup2(slave, STDOUT_FILENO);
    dup2(slave, STDERR_FILENO);
    if (slave > STDERR_FILENO) {
      close(slave);
    }
    const char* shell = getenv("SHELL");
    if (!shell || !shell[0]) {
      shell = "/usr/bin/bash";
    }
    const char* home = getenv("HOME");
    if (home && home[0]) {
      chdir(home);
    }
    setenv("TERM", "xterm-256color", 1);
    execlp(shell, shell, "-l", nullptr);
    _exit(127);
  }
  close(slave);

  m_notifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
  connect(m_notifier, &QSocketNotifier::activated, this, [this](int) {
    char buf[4096];
    for (;;) {
      ssize_t n = read(m_masterFd, buf, sizeof(buf));
      if (n == 0) {
        QCoreApplication::quit();
        break;
      }
      if (n < 0) {
        if (errno == EIO) {
          QCoreApplication::quit();
        }
        break;
      }
      ghostty_terminal_vt_write(
        reinterpret_cast<GhosttyTerminal>(m_terminal),
        reinterpret_cast<const uint8_t*>(buf),
        static_cast<size_t>(n)
      );
      render();
      applyPending();
    }
  });
  m_notifierException =
    new QSocketNotifier(m_masterFd, QSocketNotifier::Exception, this);
  connect(m_notifierException, &QSocketNotifier::activated, this, [this](int) {
    QCoreApplication::quit();
  });
}

void
TerminalView::sendText(const QString& text) {
  if (m_masterFd >= 0 && !text.isEmpty()) {
    QByteArray data = text.toUtf8();
    // TODO handle failures and finish writing partial writes
    ::write(m_masterFd, data.constData(), data.size());
  }
}

void
TerminalView::sendKey(int key, int modifiers) {
  QKeyEvent event(
    QEvent::KeyPress, key, static_cast<Qt::KeyboardModifiers>(modifiers)
  );
  QByteArray data = keyToBytes(&event);
  if (!data.isEmpty() && m_masterFd >= 0) {
    // TODO handle failures and finish writing partial writes
    ::write(m_masterFd, data.constData(), data.size());
  }
}

QByteArray
TerminalView::keyToBytes(QKeyEvent* e) {
  if (e->modifiers() & Qt::ControlModifier) {
    QString t = e->text();
    if (!t.isEmpty()) {
      char c = t.at(0).toLatin1();
      if (c >= 'a' && c <= 'z') {
        c = c - 'a' + 1;
      } else if (c >= 'A' && c <= 'Z') {
        c = c - 'A' + 1;
      }
      return QByteArray(1, c);
    }
  }
  switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
      return QByteArray("\r");
    case Qt::Key_Backspace:
      return QByteArray("\x7f");
    case Qt::Key_Tab:
      return QByteArray("\t");
    case Qt::Key_Escape:
      return QByteArray("\x1b");
    case Qt::Key_Up:
      return QByteArray("\x1b[A");
    case Qt::Key_Down:
      return QByteArray("\x1b[B");
    case Qt::Key_Right:
      return QByteArray("\x1b[C");
    case Qt::Key_Left:
      return QByteArray("\x1b[D");
    case Qt::Key_Home:
      return QByteArray("\x1b[H");
    case Qt::Key_End:
      return QByteArray("\x1b[F");
    case Qt::Key_Delete:
      return QByteArray("\x1b[3~");
    case Qt::Key_Insert:
      return QByteArray("\x1b[2~");
    case Qt::Key_PageUp:
      return QByteArray("\x1b[5~");
    case Qt::Key_PageDown:
      return QByteArray("\x1b[6~");
    case Qt::Key_Help:
      return QByteArray("\x1b[28~");
    case Qt::Key_Menu:
      return QByteArray("\x1b[29~");
    case Qt::Key_F1:
      return QByteArray("\x1bOP");
    case Qt::Key_F2:
      return QByteArray("\x1bOQ");
    case Qt::Key_F3:
      return QByteArray("\x1bOR");
    case Qt::Key_F4:
      return QByteArray("\x1bOS");
    case Qt::Key_F5:
      return QByteArray("\x1b[15~");
    case Qt::Key_F6:
      return QByteArray("\x1b[17~");
    case Qt::Key_F7:
      return QByteArray("\x1b[18~");
    case Qt::Key_F8:
      return QByteArray("\x1b[19~");
    case Qt::Key_F9:
      return QByteArray("\x1b[20~");
    case Qt::Key_F10:
      return QByteArray("\x1b[21~");
    case Qt::Key_F11:
      return QByteArray("\x1b[23~");
    case Qt::Key_F12:
      return QByteArray("\x1b[24~");
    default:
      break;
  }
  QString t = e->text();
  QByteArray result;
  if (e->modifiers() & Qt::AltModifier) {
    result.append('\x1b');
  }
  if (!t.isEmpty()) {
    result.append(t.toUtf8());
    return result;
  }
  return QByteArray();
}
