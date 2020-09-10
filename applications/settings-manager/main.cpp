#include <QCommandLineParser>
#include <QSet>

#include "dbusservice_interface.h"
#include "powerapi_interface.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

class SlotHandler : public QObject {
public:
    SlotHandler(QStringList parameters) : QObject(0), parameters(parameters){
        watcher = new QDBusServiceWatcher(OXIDE_SERVICE, QDBusConnection::systemBus(), QDBusServiceWatcher::WatchForUnregistration, this);
        QObject::connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &SlotHandler::serviceUnregistered);
    }
    ~SlotHandler() {};
    int qt_metacall(QMetaObject::Call call, int id, void **arguments){
        id = QObject::qt_metacall(call, id, arguments);
        if (id < 0 || call != QMetaObject::InvokeMetaMethod)
            return id;
        Q_ASSERT(id < 1);

        handleSlot(sender(), arguments);
        return -1;
    }
    bool connect(QObject* sender, int methodId){
        return QMetaObject::connect(sender, methodId, this, this->metaObject()->methodCount());
    }
public slots:
    void serviceUnregistered(const QString& name){
        Q_UNUSED(name);
        qDebug() << QDBusError(QDBusError::ServiceUnknown, "The name " OXIDE_SERVICE " is no longer registered");
        qApp->exit();
    }
private:
    QStringList parameters;
    QDBusServiceWatcher* watcher;

    void handleSlot(QObject* api, void** arguments){
        Q_UNUSED(api);
        QVariantList args;
        for(int i = 0; i < parameters.length(); i++){
            auto typeId = QMetaType::type(parameters[i].toStdString().c_str());
            QMetaType type(typeId);
            void* ptr = reinterpret_cast<void*>(arguments[i + 1]);
            args << QVariant(typeId, ptr);
        }
        qDebug() << args;
    };
};

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    app.setApplicationName("rot");
    app.setApplicationVersion("1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Oxide settings tool");
    parser.addHelpOption();
    parser.applicationDescription();
    parser.addVersionOption();
    parser.addPositionalArgument("api", "API to work with");
    parser.addPositionalArgument("action", "get, set, listen");
    parser.addPositionalArgument("propertyOrSignal", "Property or signal to interact with");
    parser.addPositionalArgument("value", "Value to set the property to");

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if(args.length() < 3){
        parser.showHelp(EXIT_FAILURE);
    }
    auto action = args.at(1);
    if(!(QSet<QString> {"get", "set", "listen"}).contains(action)){
        parser.showHelp(EXIT_FAILURE);
    }
    if((QSet<QString> {"set"}).contains(action) && args.length() < 4){
        parser.showHelp(EXIT_FAILURE);
    }

    auto bus = QDBusConnection::systemBus();
    codes::eeems::oxide1::General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);

    auto name = args.at(0);
    auto property = args.at(2).toStdString();
    if(name == "power"){
        auto reply = api.requestAPI("power");
        reply.waitForFinished();
        if(reply.isError()){
            qDebug() << reply.error();
            return EXIT_FAILURE;
        }
        auto path = ((QDBusObjectPath)reply).path();
        if(path == "/"){
            qDebug() << "API not available";
            return EXIT_FAILURE;
        }
        codes::eeems::oxide1::Power powerApi(OXIDE_SERVICE, path, bus);
        if(action == "get"){
            qDebug() << powerApi.property(property.c_str());
        }else if(action == "set"){
            if(!powerApi.setProperty(property.c_str(), args.at(3).toStdString().c_str())){
                qDebug() << "Failed to set value";
                return EXIT_FAILURE;
            }
            qDebug() << powerApi.property(property.c_str());
        }else if(action == "listen"){
            auto metaObject = powerApi.metaObject();
            auto name = QString(property.c_str());
            for(int methodId = 0; methodId < metaObject->methodCount(); methodId++){
                auto method = metaObject->method(methodId);
                if(method.methodType() == QMetaMethod::Signal && method.name() == name){
                    QByteArray slotName = method.name().prepend("on").append("(");
                    QStringList parameters;
                    for(int i = 0, j = method.parameterCount(); i < j; ++i){
                        parameters << QMetaType::typeName(method.parameterType(i));
                    }
                    slotName.append(parameters.join(",")).append(")");
                    QByteArray theSignal = QMetaObject::normalizedSignature(method.methodSignature().constData());
                    QByteArray theSlot = QMetaObject::normalizedSignature(slotName);
                    if(!QMetaObject::checkConnectArgs(theSignal, theSlot)){
                        continue;
                    }
                    auto slotHandler = new SlotHandler(parameters);
                    if(slotHandler->connect(&powerApi, methodId)){
                        return app.exec();

                    }
                }
            }
            qDebug() << "Unable to listen to signal";
            return EXIT_FAILURE;
        }
    }else{
        qDebug() << "Unable to work with " + name;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
