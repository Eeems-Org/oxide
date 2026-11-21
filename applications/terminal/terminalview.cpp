#include "terminalview.h"

#include <ghostty/vt.h>

#include <QDebug>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QSocketNotifier>

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

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

  if (m_terminal) {
    ghostty_terminal_resize(
      reinterpret_cast<GhosttyTerminal>(m_terminal),
      80,
      24,
      static_cast<uint32_t>(m_charWidth),
      static_cast<uint32_t>(m_charHeight)
    );
  }
  openPty();
}

TerminalView::~TerminalView() {
  if (m_notifier) {
    delete m_notifier;
  }
  if (m_masterFd >= 0) {
    close(m_masterFd);
  }
  if (m_child > 0) {
    kill(m_child, SIGHUP);
    waitpid(m_child, nullptr, 0);
  }
  if (m_renderState) {
    ghostty_render_state_free(
      reinterpret_cast<GhosttyRenderState>(m_renderState)
    );
    m_renderState = nullptr;
  }
  if (m_rowIter) {
    ghostty_render_state_row_iterator_free(
      reinterpret_cast<GhosttyRenderStateRowIterator>(m_rowIter)
    );
    m_rowIter = nullptr;
  }
  if (m_cells) {
    ghostty_render_state_row_cells_free(
      reinterpret_cast<GhosttyRenderStateRowCells>(m_cells)
    );
    m_cells = nullptr;
  }
  if (m_terminal) {
    ghostty_terminal_free(reinterpret_cast<GhosttyTerminal>(m_terminal));
    m_terminal = nullptr;
  }
}

void
TerminalView::paint(QPainter* painter) {
  if (!m_terminal || !m_renderState) {
    return;
  }

  GhosttyTerminal terminal = reinterpret_cast<GhosttyTerminal>(m_terminal);
  GhosttyRenderState renderState =
    reinterpret_cast<GhosttyRenderState>(m_renderState);
  GhosttyRenderStateRowIterator rowIter =
    reinterpret_cast<GhosttyRenderStateRowIterator>(m_rowIter);
  GhosttyRenderStateRowCells cells =
    reinterpret_cast<GhosttyRenderStateRowCells>(m_cells);

  ghostty_render_state_update(renderState, terminal);

  uint16_t cols = 0, rows = 0;
  ghostty_render_state_get(renderState, GHOSTTY_RENDER_STATE_DATA_COLS, &cols);
  ghostty_render_state_get(renderState, GHOSTTY_RENDER_STATE_DATA_ROWS, &rows);

  GhosttyColorRgb fg = {200, 200, 200};
  GhosttyColorRgb bg = {0, 0, 0};
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_COLOR_FOREGROUND, &fg
  );
  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_COLOR_BACKGROUND, &bg
  );

  ghostty_render_state_get(
    renderState, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &rowIter
  );

  painter->setFont(m_font);
  uint16_t y = 0;
  while (ghostty_render_state_row_iterator_next(rowIter)) {
    ghostty_render_state_row_get(
      rowIter, GHOSTTY_RENDER_STATE_ROW_DATA_CELLS, &cells
    );
    int x = 0;
    while (ghostty_render_state_row_cells_next(cells)) {
      char buf[256];
      GhosttyBuffer gbuf;
      gbuf.ptr = reinterpret_cast<uint8_t*>(buf);
      gbuf.cap = sizeof(buf) - 1;
      gbuf.len = 0;
      GhosttyResult r = ghostty_render_state_row_cells_get(
        cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_UTF8, &gbuf
      );
      if (r == GHOSTTY_SUCCESS && gbuf.len > 0) {
        if (gbuf.len >= sizeof(buf)) {
          gbuf.len = sizeof(buf) - 1;
        }
        buf[gbuf.len] = '\0';
        QString text = QString::fromUtf8(buf, static_cast<int>(gbuf.len));

        GhosttyColorRgb cellFg = fg;
        ghostty_render_state_row_cells_get(
          cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &cellFg
        );
        painter->setPen(QColor(cellFg.r, cellFg.g, cellFg.b));

        GhosttyColorRgb cellBg = bg;
        if (
          ghostty_render_state_row_cells_get(
            cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &cellBg
          ) == GHOSTTY_SUCCESS
        ) {
          painter->fillRect(
            QRectF(
              x * m_charWidth, y * m_charHeight, m_charWidth, m_charHeight
            ),
            QColor(cellBg.r, cellBg.g, cellBg.b)
          );
        }
        painter->drawText(
          QPointF(
            x * m_charWidth,
            (y + 1) * m_charHeight - QFontMetricsF(m_font).descent()
          ),
          text
        );
      } else {
        GhosttyColorRgb cellBg = bg;
        if (
          ghostty_render_state_row_cells_get(
            cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &cellBg
          ) == GHOSTTY_SUCCESS
        ) {
          painter->fillRect(
            QRectF(
              x * m_charWidth, y * m_charHeight, m_charWidth, m_charHeight
            ),
            QColor(cellBg.r, cellBg.g, cellBg.b)
          );
        }
      }
      x++;
    }
    y++;
  }

  bool cursorVisible = false;
  if (
    ghostty_render_state_get(
      renderState, GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE, &cursorVisible
    ) == GHOSTTY_SUCCESS &&
    cursorVisible
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
        painter->fillRect(
          QRectF(
            cx * m_charWidth, cy * m_charHeight, m_charWidth, m_charHeight
          ),
          QColor(fg.r, fg.g, fg.b)
        );
      }
    }
  }

  ghostty_render_state_clean(renderState);
  m_rowIter = rowIter;
  m_cells = cells;
}

void
TerminalView::geometryChange(
  const QRectF& newGeometry,
  const QRectF& oldGeometry
) {
  QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
  updateSize();
}

void
TerminalView::keyPressEvent(QKeyEvent* event) {
  qDebug() << "TerminalView keyPressEvent" << event->key() << event->text();
  QByteArray data = keyToBytes(event);
  if (!data.isEmpty() && m_masterFd >= 0) {
    ::write(m_masterFd, data.constData(), data.size());
  }
  event->accept();
}

void
TerminalView::updateSize() {
  if (m_charWidth <= 0 || m_charHeight <= 0) {
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
  m_cols = cols;
  m_rows = rows;
  if (m_terminal) {
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
  update();
}

void
TerminalView::setTitle(const QString& title) {
  if (m_title != title) {
    m_title = title;
    emit titleChanged(m_title);
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
      if (n > 0) {
        ghostty_terminal_vt_write(
          reinterpret_cast<GhosttyTerminal>(m_terminal),
          reinterpret_cast<const uint8_t*>(buf),
          static_cast<size_t>(n)
        );
        update();
      } else if (n == 0) {
        QCoreApplication::quit();
        break;
      } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break;
        }
        break;
      }
    }
  });
}

void
TerminalView::sendText(const QString& text) {
  if (m_masterFd >= 0 && !text.isEmpty()) {
    QByteArray data = text.toUtf8();
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
