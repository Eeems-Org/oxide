#ifndef EVENTFILTER_H
#define EVENTFILTER_H

#include <QObject>
#include <QEvent>
#include <QQuickItem>

namespace Oxide{
    class EventFilter : public QObject
    {
        Q_OBJECT
    public:
        QQuickItem* root;
        explicit EventFilter(QObject* parent = nullptr);
    signals:
        void suspend();
    protected:
      bool eventFilter(QObject* obj, QEvent* ev);
    };
}

#endif // EVENTFILTER_H
