#ifndef SLOTHANDLER_H
#define SLOTHANDLER_H
#include <QObject>
#include <QDBusServiceWatcher>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>

#include "json.h"

class SlotHandler : public QObject {
public:
    SlotHandler(const QString& serviceName, QStringList parameters, bool once, std::function<void()> callback)
        : QObject(0),
          serviceName(serviceName),
          parameters(parameters),
          once(once),
          m_disconnected(false),
          qStdOut(stdout, QIODevice::WriteOnly),
          callback(callback)
    {
        watcher = new QDBusServiceWatcher(serviceName, QDBusConnection::systemBus(), QDBusServiceWatcher::WatchForUnregistration, this);
        QObject::connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [=](const QString& name){
            Q_UNUSED(name);
            if(!m_disconnected){
                qDebug() << QDBusError(QDBusError::ServiceUnknown, "The name " + serviceName + " is no longer registered");
                m_disconnected = true;
                callback();
            }
        });
    }
    ~SlotHandler() {};
    int qt_metacall(QMetaObject::Call call, int id, void **arguments){
        id = QObject::qt_metacall(call, id, arguments);
        if (id < 0 || call != QMetaObject::InvokeMetaMethod)
            return id;
        Q_ASSERT(id < 1);
        if(!m_disconnected){
            handleSlot(sender(), arguments);
        }
        return -1;
    }
    bool connect(QObject* sender, int methodId){
        return QMetaObject::connect(sender, methodId, this, this->metaObject()->methodCount());
    }

private:
    QString serviceName;
    QStringList parameters;
    bool once;
    bool m_disconnected;
    QDBusServiceWatcher* watcher;
    QTextStream qStdOut;
    std::function<void()> callback;

    void handleSlot(QObject* api, void** arguments){
        Q_UNUSED(api);
        QVariantList args;
        for(int i = 0; i < parameters.length(); i++){
            auto typeId = QMetaType::type(parameters[i].toStdString().c_str());
            QMetaType type(typeId);
            void* ptr = reinterpret_cast<void*>(arguments[i + 1]);
            args << QVariant(typeId, ptr);
        }
        if(args.size() > 1){
            qStdOut << toJson(args).toStdString().c_str() << Qt::endl;
        }else if(args.size() == 1 && !args.first().isNull()){
            qStdOut << toJson(args.first()).toStdString().c_str() << Qt::endl;
        }else{
            qStdOut << "undefined" << Qt::endl;
        }
        if(once){
            m_disconnected = true;
            callback();
        }
    }
};

#endif // SLOTHANDLER_H
