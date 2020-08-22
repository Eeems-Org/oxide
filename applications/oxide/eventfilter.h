#ifndef EVENTFILTER_H
#define EVENTFILTER_H

#include <QObject>
#include <QEvent>
#include <QTimer>

class EventFilter : public QObject
{
    Q_OBJECT
public:
    QTimer* timer;
    explicit EventFilter(QObject* parent = nullptr);
signals:
    void suspend();
protected:
  bool eventFilter(QObject* obj, QEvent* ev);
};

#endif // EVENTFILTER_H
