#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <QQuickPaintedItem>

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
};

#endif
