#include "slothandler.h"
#include "meta.h"
#include "debug.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QMetaMethod>


namespace Oxide{
    bool DBusConnect(QDBusAbstractInterface* interface, const QString& slotName, std::function<void(QVariantList)> onMessage, const bool& once=false){
        return DBusConnect(interface, slotName, onMessage, []{}, once);
    }
    bool DBusConnect(QDBusAbstractInterface* interface, const QString& slotName, std::function<void(QVariantList)> onMessage, std::function<void()> callback, const bool& once){
        auto metaObject = interface->metaObject();
        for(int methodId = 0; methodId < metaObject->methodCount(); methodId++){
            auto method = metaObject->method(methodId);
            if(method.methodType() == QMetaMethod::Signal && method.name() == slotName){
                QByteArray slotName = method.name().prepend("on").append("(");
                QStringList parameters;
                for(int i = 0, j = method.parameterCount(); i < j; ++i){
                    parameters << QMetaType::typeName(method.parameterType(i));
                }
                slotName.append(parameters.join(",").toUtf8()).append(")");
                QByteArray theSignal = QMetaObject::normalizedSignature(method.methodSignature().constData());
                QByteArray theSlot = QMetaObject::normalizedSignature(slotName);
                if(!QMetaObject::checkConnectArgs(theSignal, theSlot)){
                    continue;
                }
                auto slotHandler = new Oxide::SlotHandler(OXIDE_SERVICE, parameters, once, onMessage, callback);
                return slotHandler->connect(interface, methodId);
            }
        }
        return false;
    }
    SlotHandler::SlotHandler(const QString& serviceName, QStringList parameters, bool once, std::function<void(QVariantList)> onMessage, std::function<void()> callback)
    : QObject(0),
      serviceName(serviceName),
      parameters(parameters),
      once(once),
      m_disconnected(false),
      onMessage(onMessage),
      callback(callback)
    {
        watcher = new QDBusServiceWatcher(serviceName, QDBusConnection::systemBus(), QDBusServiceWatcher::WatchForUnregistration, this);
        QObject::connect(
            watcher,
            &QDBusServiceWatcher::serviceUnregistered,
            this,
            [this, callback, serviceName](const QString& name){
                Q_UNUSED(name);
                if(!m_disconnected){
                    O_DEBUG(QDBusError(
                        QDBusError::ServiceUnknown,
                        "The name " + serviceName + " is no longer registered"
                    ));
                    m_disconnected = true;
                    callback();
                }
            }
        );
    }
    SlotHandler::~SlotHandler() {}
    int SlotHandler::qt_metacall(QMetaObject::Call call, int id, void **arguments){
        id = QObject::qt_metacall(call, id, arguments);
        if (id < 0 || call != QMetaObject::InvokeMetaMethod){
            return id;
        }
        Q_ASSERT(id < 1);
        if(!m_disconnected){
            handleSlot(sender(), arguments);
        }
        return -1;
    }
    bool SlotHandler::connect(QObject* sender, int methodId){
        return QMetaObject::connect(sender, methodId, this, this->metaObject()->methodCount());
    }
    void SlotHandler::handleSlot(QObject* api, void** arguments){
        Q_UNUSED(api);
        QVariantList args;
        for(int i = 0; i < parameters.length(); i++){
            auto typeId = QMetaType::type(parameters[i].toStdString().c_str());
            QMetaType type(typeId);
            void* ptr = reinterpret_cast<void*>(arguments[i + 1]);
            args << QVariant(typeId, ptr);
        }
        onMessage(args);
        if(once){
            m_disconnected = true;
            callback();
        }
    }
}
