#ifndef EVENTFILTER_H
#define EVENTFILTER_H

#include <QObject>
#include <QEvent>
#include <QTimer>
#include <QGuiApplication>
#include <QQuickItem>

class EventFilter : public QObject
{
    Q_OBJECT
public:
    QQuickItem* root;
    explicit EventFilter(QObject* parent = nullptr);
protected:
  bool eventFilter(QObject* obj, QEvent* ev);
};

#endif // EVENTFILTER_H
