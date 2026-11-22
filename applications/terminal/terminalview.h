#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <QImage>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QRegion>
#include <QSocketNotifier>
#include <QTimer>
#include <atomic>
#include <ghostty/vt.h>
#include <libblight/clock.h>
#include <mutex>
#include <string>
#include <vector>

struct CachedCell {
  GhosttyStyle style = {};
  std::string text;
};

class TerminalView : public QQuickPaintedItem {
  Q_OBJECT
  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
public:
  explicit TerminalView(QQuickItem* parent = nullptr);
  ~TerminalView() override;

  void paint(QPainter* painter) override;

  QString title() const { return m_title; }
  Q_INVOKABLE void setTitle(const QString& title);

  Q_INVOKABLE void sendText(const QString& text);
  Q_INVOKABLE void sendKey(int key, int modifiers);

protected:
  void
  geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
  void keyPressEvent(QKeyEvent* event) override;

signals:
  void titleChanged(const QString& title);

private:
  void updateSize();
  void openPty();
  QByteArray keyToBytes(QKeyEvent* event);
  void renderDirtyCells();
  void renderRowToPainter(
    QPainter& painter,
    GhosttyRenderStateRowIterator rowIter,
    GhosttyRenderStateRowCells cells,
    uint16_t y,
    const GhosttyColorRgb& fg,
    const GhosttyColorRgb& bg,
    qreal charWidth,
    qreal charHeight
  );
  void renderCellToPainter(
    QPainter& painter,
    int x,
    int y,
    const GhosttyStyle& style,
    const std::string& text,
    const GhosttyColorRgb& fg,
    const GhosttyColorRgb& bg,
    qreal charWidth,
    qreal charHeight
  );
  GhosttyColorRgb paletteIndexToRgb(uint8_t index);
  void renderCellAtPosition(
    QPainter& painter,
    int x,
    int y,
    const GhosttyColorRgb& fg,
    const GhosttyColorRgb& bg
  );
  void applyPending();

  QString m_title;
  void* m_terminal = nullptr;
  void* m_renderState = nullptr;
  void* m_rowIter = nullptr;
  void* m_cells = nullptr;

  QFont m_font;
  qreal m_charWidth = 8;
  qreal m_charHeight = 14;
  int m_cols = 80;
  int m_rows = 24;
  bool m_firstPaint = true;
  int m_masterFd = -1;
  int m_child = -1;
  class QSocketNotifier* m_notifier = nullptr;
  class QSocketNotifier* m_notifierException = nullptr;

  QImage m_drawn;
  QRegion m_pending;
  std::vector<CachedCell> m_cellCache;
  std::atomic<bool> m_drawing{false};
  std::mutex m_timerMutex;
  Blight::ClockWatch m_lastPaint;
  QTimer* m_paintTimer = nullptr;
  int m_lastCursorX = -1;
  int m_lastCursorY = -1;
};

#endif
