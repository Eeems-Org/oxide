#include <QCommandLineParser>
#include <QJsonValue>
#include <QJsonDocument>
#include <QSet>
#include <QMutableListIterator>

#include "dbussettings.h"

#include "dbusservice_interface.h"
#include "powerapi_interface.h"
#include "wifiapi_interface.h"
#include "network_interface.h"
#include "bss_interface.h"
#include "appsapi_interface.h"
#include "application_interface.h"
#include "systemapi_interface.h"
#include "screenapi_interface.h"
#include "screenshot_interface.h"
#include "notificationapi_interface.h"
#include "notification_interface.h"

using namespace codes::eeems::oxide1;

static QTextStream qStdOut(stdout, QIODevice::WriteOnly);

QVariant sanitizeForJson(QVariant value);
QVariant decodeDBusArgument(const QDBusArgument& arg){
    auto type = arg.currentType();
    if(type == QDBusArgument::BasicType || type == QDBusArgument::VariantType){
        return sanitizeForJson(arg.asVariant());
    }
    if(type == QDBusArgument::ArrayType){
        QVariantList list;
        arg.beginArray();
        while(!arg.atEnd()){
            list.append(decodeDBusArgument(arg));
        }
        arg.endArray();
        return sanitizeForJson(list);
    }
    if(type == QDBusArgument::MapType){
        qDebug() << "Map Type";
        QMap<QVariant, QVariant> map;
        arg.beginMap();
        while(!arg.atEnd()){
            arg.beginMapEntry();
            auto key = decodeDBusArgument(arg);
            auto value = decodeDBusArgument(arg);
            arg.endMapEntry();
            map.insert(sanitizeForJson(key), sanitizeForJson(value));
        }
        arg.endMap();
        return sanitizeForJson(QVariant::fromValue(map));
    }
    qDebug() << "Unable to sanitize QDBusArgument as it is an unknown type";
    return QVariant();
}
QVariant sanitizeForJson(QVariant value){
    auto userType = value.userType();
    if(userType == QMetaType::type("QDBusObjectPath")){
        return value.value<QDBusObjectPath>().path();
    }
    if(userType == QMetaType::type("QDBusSignature")){
        return value.value<QDBusSignature>().signature();
    }
    if(userType == QMetaType::type("QDBusVariant")){
        return value.value<QDBusVariant>().variant();
    }
    if(userType == QMetaType::type("QDBusArgument")){
        return decodeDBusArgument(value.value<QDBusArgument>());
    }
    if(userType == QMetaType::type("QList<QDBusVariant>")){
        QVariantList list;
        for(auto value : value.value<QList<QDBusVariant>>()){
            list.append(sanitizeForJson(value.variant()));
        }
        return list;
    }
    if(userType == QMetaType::type("QList<QDBusSignature>")){
        QStringList list;
        for(auto value : value.value<QList<QDBusSignature>>()){
            list.append(value.signature());
        }
        return list;
    }
    if(userType == QMetaType::type("QList<QDBusObjectPath>")){
        QStringList list;
        for(auto value : value.value<QList<QDBusObjectPath>>()){
            list.append(value.path());
        }
        return list;
    }
    if(userType == QMetaType::QByteArray){
        auto byteArray = value.toByteArray();
        QVariantList list;
        for(auto byte : byteArray){
            list.append(byte);
        }
        return list;
    }
    if(userType == QMetaType::QVariantMap){
        QVariantMap map;
        auto input = value.toMap();
        for(auto key : input.keys()){
            map.insert(key, sanitizeForJson(input[key]));
        }
        return map;
    }
    if(userType == QMetaType::QVariantList){
        QVariantList list = value.toList();
        QMutableListIterator<QVariant> i(list);
        while(i.hasNext()){
            i.setValue(sanitizeForJson(i.next()));
        }
        return list;
    }
    return value;
}
QString toJson(QVariant value){
    if(value.isNull()){
        return "null";
    }
    auto jsonVariant = QJsonValue::fromVariant(sanitizeForJson(value));
    if(jsonVariant.isNull()){
        return "null";
    }
    if(jsonVariant.isUndefined()){
        return "undefined";
    }
    QJsonArray jsonArray;
    jsonArray.append(jsonVariant);
    QJsonDocument doc(jsonArray);
    auto json = doc.toJson(QJsonDocument::Compact);
    return json.mid(1, json.length() - 2);
}
QVariant fromJson(QByteArray json){
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson("[" + json + "]", &error);
    if(error.error != QJsonParseError::NoError){
        qDebug() << "Unable to read json value" << error.errorString();
        qDebug() << "Value to parse" << json;
    }
    return doc.array().first().toVariant();
}

class SlotHandler : public QObject {
public:
    SlotHandler(QStringList parameters, bool once) : QObject(0), parameters(parameters), once(once){
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
    bool once;
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
        if(args.size() > 1){
            qStdOut << toJson(args).toStdString().c_str() << endl;
        }else if(args.size() == 1 && !args.first().isNull()){
            qStdOut << toJson(args.first()).toStdString().c_str() << endl;
        }else{
            qStdOut << "undefined" << endl;
        }
        if(once){
            qApp->quit();
        }
    };
};

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("rot");
    app.setApplicationVersion(OXIDE_INTERFACE_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Oxide settings tool");
    parser.addHelpOption();
    parser.applicationDescription();
    parser.addVersionOption();
    parser.addPositionalArgument("api", "wifi\npower\napps\nsystem\nscreen\nnotification");
    parser.addPositionalArgument("action","get\nset\nlisten\ncall");
    QCommandLineOption objectOption(
        {"o", "object"},
        "Object to act on, e.g. Network:network/94d5caa2d4345ab7be5254dfb9678cd7",
        "object"
    );
    parser.addOption(objectOption);
    QCommandLineOption onceOption(
        "once",
        "Exit on the first signal when listening."
    );
    parser.addOption(onceOption);

    parser.process(app);

    QStringList args = parser.positionalArguments();
    if(args.isEmpty()){
        parser.showHelp(EXIT_FAILURE);
    }
    auto apiName = args.at(0);
    if(!(QSet<QString> {"power", "wifi", "apps", "system", "screen", "notification"}).contains(apiName)){
        qDebug() << "Unknown API" << apiName;
        return EXIT_FAILURE;
    }
    if(args.length() < 2){
        parser.showHelp(EXIT_FAILURE);
    }
    auto action = args.at(1);
    if(action == "get"){
        parser.addPositionalArgument("property", "Property to get.");
        parser.parse(app.arguments());
        args = parser.positionalArguments();
        if(args.size() < 3){
            parser.showHelp(EXIT_FAILURE);
        }
    }else if(action == "set"){
        parser.addPositionalArgument("property", "Property to change.");
        parser.addPositionalArgument("value", "Value to set the property to.");
        parser.parse(app.arguments());
        args = parser.positionalArguments();
        if(args.size() < 4){
            parser.showHelp(EXIT_FAILURE);
        }
    }else if(action == "listen"){
        parser.addPositionalArgument("signal", "Signal to listen to.");
        parser.parse(app.arguments());
        args = parser.positionalArguments();
        if(args.size() < 3){
            parser.showHelp(EXIT_FAILURE);
        }
    }else if(action == "call"){
        parser.addPositionalArgument("method", "Method to call");
        parser.addPositionalArgument("arguments", "Arguments to pass to the method using the following format: <QVariant>:<Value>. e.g. QString:Test", "[arguments...]");
        parser.parse(app.arguments());
        args = parser.positionalArguments();
        if(args.size() < 3){
            parser.showHelp(EXIT_FAILURE);
        }
    }else{
        parser.showHelp(EXIT_FAILURE);
    }
    auto bus = QDBusConnection::systemBus();
    if(!bus.isConnected()){
        qDebug() << "Not able to connect to dbus";
        return EXIT_FAILURE;
    }
    General generalApi(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    auto reply = generalApi.requestAPI(apiName);
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
    QDBusAbstractInterface* api;
    if(apiName == "power"){
        api = new Power(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            qDebug() << "Paths are not valid for the power API";
            return EXIT_FAILURE;
        }
    }else if(apiName == "wifi"){
        api = new Wifi(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            auto object = parser.value("object");
            auto type = object.mid(0, object.indexOf(":"));
            auto path = object.mid(object.indexOf(":") + 1);
            path = OXIDE_SERVICE_PATH + QString("/" + path);
            if(type == "Network"){
                api = new Network(OXIDE_SERVICE, path, bus);
            }else if(type == "BSS"){
                api = new BSS(OXIDE_SERVICE, path, bus);
            }else{
                qDebug() << "Unknown object type" << type;
                return EXIT_FAILURE;
            }
        }
    }else if(apiName == "apps"){
        api = new Apps(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            auto object = parser.value("object");
            auto type = object.mid(0, object.indexOf(":"));
            auto path = object.mid(object.indexOf(":") + 1);
            path = OXIDE_SERVICE_PATH + QString("/" + path);
            if(type == "Application"){
                api = new Application(OXIDE_SERVICE, path, bus);
            }else{
                qDebug() << "Unknown object type" << type;
                return EXIT_FAILURE;
            }
        }
    }else if(apiName == "system"){
        api = new System(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            qDebug() << "Paths are not valid for the system API";
            return EXIT_FAILURE;
        }
    }else if(apiName == "screen"){
        api = new Screen(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            auto object = parser.value("object");
            auto type = object.mid(0, object.indexOf(":"));
            auto path = object.mid(object.indexOf(":") + 1);
            path = OXIDE_SERVICE_PATH + QString("/" + path);
            if(type == "Screenshot"){
                api = new Screenshot(OXIDE_SERVICE, path, bus);
            }else{
                qDebug() << "Unknown object type" << type;
                return EXIT_FAILURE;
            }
        }
    }else if(apiName == "notification"){
        api = new Notifications(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            auto object = parser.value("object");
            auto type = object.mid(0, object.indexOf(":"));
            auto path = object.mid(object.indexOf(":") + 1);
            path = OXIDE_SERVICE_PATH + QString("/" + path);
            if(type == "Notification"){
                api = new Notification(OXIDE_SERVICE, path, bus);
            }else{
                qDebug() << "Unknown object type" << type;
                return EXIT_FAILURE;
            }
        }
    }else{
        qDebug() << "API not initialized? Please log a bug.";
        return EXIT_FAILURE;
    }
    if(action == "get"){
        auto value = api->property(args.at(2).toStdString().c_str());
        auto error = api->lastError();
        if(error.type() != QDBusError::NoError){
            qDebug() << "Failed to get value" << api->lastError();
            return EXIT_FAILURE;
        }
        qStdOut << toJson(value).toStdString().c_str() << endl;
    }else if(action == "set"){
        auto property = args.at(2).toStdString();
        if(!api->setProperty(property.c_str(), args.at(3).toStdString().c_str())){
            qDebug() << "Failed to set value" << api->lastError();
            return EXIT_FAILURE;
        }
    }else if(action == "listen"){
        auto metaObject = api->metaObject();
        auto name = QString(args.at(2).toStdString().c_str());
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
                auto slotHandler = new SlotHandler(parameters, parser.isSet("once"));
                if(slotHandler->connect(api, methodId)){
                    return app.exec();
                }
            }
        }
        qDebug() << "Unable to listen to signal" << bus.interface()->lastError();
        return EXIT_FAILURE;
    }else if(action == "call"){
        auto method = args.at(2);
        args = args.mid(3);
        QVariantList arguments;
        if(args.size() > 0){
            for(auto arg : args){
                if(!arg.contains(":")){
                    qDebug() << "Arguments must be in the following format: <QVariant>:<Value>";
                    return EXIT_FAILURE;
                }
                auto type = arg.mid(0, arg.indexOf(":"));
                auto value = arg.mid(arg.indexOf(":") + 1);
                int id = QMetaType::type(type.toStdString().c_str());
                if(id == QMetaType::UnknownType){
                    qDebug() << "Unknown type" << type;
                    return EXIT_FAILURE;
                }
                QVariant variant = fromJson(value.toUtf8());
                if(type == "QDBusObjectPath"){
                    variant = QVariant::fromValue(QDBusObjectPath(variant.toString()));
                }else if(type == "QDBusSignature"){
                    variant = QVariant::fromValue(QDBusSignature(variant.toString()));
                }
                if(!variant.canConvert(id)){
                    qDebug() << "Unable to convert to type" << type;
                    qDebug() << "Value" << variant;
                    return EXIT_FAILURE;
                }
                variant.convert(id);
                if(!variant.isValid()){
                    qDebug() << "Failed converting to " << type;
                    qDebug() << "Value" << variant;
                    return EXIT_FAILURE;
                }
                arguments.append(variant);
            }
        }
        QDBusMessage reply = api->callWithArgumentList(QDBus::Block, method, arguments);
        auto result = reply.arguments();
        if(result.size() > 1){
            qStdOut << toJson(result).toStdString().c_str() << endl;
        }else if(!result.first().isNull()){
            qStdOut << toJson(result.first()).toStdString().c_str() << endl;
        }
        if(!reply.errorName().isEmpty()){
            return EXIT_FAILURE;
        }
        if(apiName == "system" && (method == "inhibitSleep" || method == "inhibitPowerOff")){
            return app.exec();
        }
    }
    return EXIT_SUCCESS;
}
