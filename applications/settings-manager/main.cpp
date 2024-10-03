#include <QCommandLineParser>
#include <QSet>
#include <QTextStream>

#include <liboxide.h>

using namespace codes::eeems::oxide1;
using namespace Oxide::Sentry;
using namespace Oxide::JSON;

int qExit(int ret){
    QTimer::singleShot(0, [ret](){
        qApp->exit(ret);
    });
    return qApp->exec();
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("rot", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("rot");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Oxide settings tool");
    parser.addHelpOption();
    parser.applicationDescription();
    parser.addVersionOption();
    parser.addPositionalArgument("api", "settings\nwifi\npower\napps\nsystem\nscreen\nnotification");
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
#ifdef SENTRY
        sentry_breadcrumb("error", "No arguments");
#endif
        parser.showHelp(EXIT_FAILURE);
    }
    auto apiName = args.at(0);
    if(!(QSet<QString> {"settings", "power", "wifi", "apps", "system", "screen", "notification"}).contains(apiName)){
        qDebug() << "Unknown API" << apiName;
#ifdef SENTRY
        sentry_breadcrumb("error", "Unknown API");
#endif
        return qExit(EXIT_FAILURE);
    }
    if(args.length() < 2){
        parser.showHelp(EXIT_FAILURE);
    }
    auto action = args.at(1);
    if(action == "get"){
#ifdef SENTRY
        sentry_breadcrumb("action", "get");
#endif
        parser.addPositionalArgument("property", "Property to get.");
        parser.parse(app.arguments());
        args = parser.positionalArguments();
        if(args.size() < 3){
            parser.showHelp(EXIT_FAILURE);
        }
    }else if(action == "set"){
#ifdef SENTRY
        sentry_breadcrumb("action", "set");
#endif
        parser.addPositionalArgument("property", "Property to change.");
        parser.addPositionalArgument("value", "Value to set the property to.");
        parser.parse(app.arguments());
        args = parser.positionalArguments();
        if(args.size() < 4){
#ifdef SENTRY
            sentry_breadcrumb("action", "Missing arguments");
#endif
            parser.showHelp(EXIT_FAILURE);
        }
    }else if(action == "listen"){
#ifdef SENTRY
        sentry_breadcrumb("action", "listen");
#endif
        parser.addPositionalArgument("signal", "Signal to listen to.");
        parser.parse(app.arguments());
        args = parser.positionalArguments();
        if(args.size() < 3){
#ifdef SENTRY
            sentry_breadcrumb("action", "Missing arguments");
#endif
            parser.showHelp(EXIT_FAILURE);
        }
    }else if(action == "call"){
#ifdef SENTRY
        sentry_breadcrumb("action", "call");
#endif
        parser.addPositionalArgument("method", "Method to call");
        parser.addPositionalArgument("arguments", "Arguments to pass to the method using the following format: <QVariant>:<Value>. e.g. QString:Test", "[arguments...]");
        parser.parse(app.arguments());
        args = parser.positionalArguments();
        if(args.size() < 3){
#ifdef SENTRY
            sentry_breadcrumb("action", "Missing arguments");
#endif
            parser.showHelp(EXIT_FAILURE);
        }
    }else{
#ifdef SENTRY
        sentry_breadcrumb("action", "unknown");
#endif
        parser.showHelp(EXIT_FAILURE);
    }
    auto bus = QDBusConnection::systemBus();
    if(!bus.isConnected()){
        qDebug() << "Not able to connect to dbus";
#ifdef SENTRY
        sentry_breadcrumb("error", "Unable to connect to dbus");
#endif
        return qExit(EXIT_FAILURE);
    }
    QString path = "";
    if(apiName != "settings"){
        General generalApi(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
        auto reply = generalApi.requestAPI(apiName);
        reply.waitForFinished();
        if(reply.isError()){
            qDebug() << reply.error();
#ifdef SENTRY
            sentry_breadcrumb("error", "Unable to request API");
#endif
            return qExit(EXIT_FAILURE);
        }
        path = ((QDBusObjectPath)reply).path();
        if(path == "/"){
            qDebug() << "API not available";
#ifdef SENTRY
            sentry_breadcrumb("error", "API not available");
#endif
            return qExit(EXIT_FAILURE);
        }
    }
    QObject* api;
    if(apiName == "settings"){
        if(action == "call"){
            if(args.length() == 3 && args.at(2) == "crash"){
                trigger_crash();
            }else if(args.length() == 3 && args.at(2) == "transaction"){
                sentry_transaction("Dummy rot transaction", "transaction", [](Transaction* t){
                    sentry_span(t, "span", "Transaction span", []{
                        qDebug() << "Triggered transaction";
                    });
                });
                return qExit(EXIT_SUCCESS);
            }
            qDebug() << "Call is not valid for the settings API";
#ifdef SENTRY
            sentry_breadcrumb("error", "invalid arguments");
#endif
            return qExit(EXIT_FAILURE);
        }
        if(parser.isSet("object")){
            qDebug() << "Paths are not valid for the settings API";
#ifdef SENTRY
            sentry_breadcrumb("error", "invalid arguments");
#endif
            return qExit(EXIT_FAILURE);
        }
        api = &sharedSettings;
    }else if(apiName == "power"){
#ifdef SENTRY
        sentry_breadcrumb("api", "power");
#endif
        api = new Power(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            qDebug() << "Paths are not valid for the power API";
#ifdef SENTRY
            sentry_breadcrumb("error", "invalid arguments");
#endif
            return qExit(EXIT_FAILURE);
        }
    }else if(apiName == "wifi"){
#ifdef SENTRY
        sentry_breadcrumb("api", "wifi");
#endif
        api = new Wifi(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            auto object = parser.value("object");
#ifdef SENTRY
            sentry_breadcrumb("object", object.toStdString().c_str());
#endif
            auto type = object.mid(0, object.indexOf(":"));
            auto path = object.mid(object.indexOf(":") + 1);
            path = OXIDE_SERVICE_PATH + QString("/" + path);
            if(type == "Network"){
                api = new Network(OXIDE_SERVICE, path, bus);
            }else if(type == "BSS"){
                api = new BSS(OXIDE_SERVICE, path, bus);
            }else{
                qDebug() << "Unknown object type" << type;
#ifdef SENTRY
                sentry_breadcrumb("error", "Unknown object type");
#endif
                return qExit(EXIT_FAILURE);
            }
        }
    }else if(apiName == "apps"){
#ifdef SENTRY
        sentry_breadcrumb("api", "apps");
#endif
        api = new Apps(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            auto object = parser.value("object");
#ifdef SENTRY
            sentry_breadcrumb("object", object.toStdString().c_str());
#endif
            auto type = object.mid(0, object.indexOf(":"));
            auto path = object.mid(object.indexOf(":") + 1);
            path = OXIDE_SERVICE_PATH + QString("/" + path);
            if(type == "Application"){
                api = new Application(OXIDE_SERVICE, path, bus);
            }else{
                qDebug() << "Unknown object type" << type;
#ifdef SENTRY
                sentry_breadcrumb("error", "Unknown object type");
#endif
                return qExit(EXIT_FAILURE);
            }
        }
    }else if(apiName == "system"){
#ifdef SENTRY
        sentry_breadcrumb("api", "system");
#endif
        api = new System(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            qDebug() << "Paths are not valid for the system API";
#ifdef SENTRY
            sentry_breadcrumb("error", "invalid arguments");
#endif
            return qExit(EXIT_FAILURE);
        }
    }else if(apiName == "screen"){
#ifdef SENTRY
        sentry_breadcrumb("api", "screen");
#endif
        api = new Screen(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            auto object = parser.value("object");
#ifdef SENTRY
            sentry_breadcrumb("object", object.toStdString().c_str());
#endif
            auto type = object.mid(0, object.indexOf(":"));
            auto path = object.mid(object.indexOf(":") + 1);
            path = OXIDE_SERVICE_PATH + QString("/" + path);
            if(type == "Screenshot"){
                api = new Screenshot(OXIDE_SERVICE, path, bus);
            }else{
                qDebug() << "Unknown object type" << type;
#ifdef SENTRY
                sentry_breadcrumb("error", "invalid arguments");
#endif
                return qExit(EXIT_FAILURE);
            }
        }
    }else if(apiName == "notification"){
#ifdef SENTRY
        sentry_breadcrumb("api", "notification");
#endif
        api = new Notifications(OXIDE_SERVICE, path, bus);
        if(parser.isSet("object")){
            auto object = parser.value("object");
#ifdef SENTRY
            sentry_breadcrumb("object", object.toStdString().c_str());
#endif
            auto type = object.mid(0, object.indexOf(":"));
            auto path = object.mid(object.indexOf(":") + 1);
            path = OXIDE_SERVICE_PATH + QString("/" + path);
            if(type == "Notification"){
                api = new Notification(OXIDE_SERVICE, path, bus);
            }else{
                qDebug() << "Unknown object type" << type;
#ifdef SENTRY
                sentry_breadcrumb("error", "Unknown object type");
#endif
                return qExit(EXIT_FAILURE);
            }
        }
    }else{
        qDebug() << "API not initialized? Please log a bug.";
#ifdef SENTRY
            sentry_breadcrumb("error", "Unknown API");
#endif
        return qExit(EXIT_FAILURE);
    }
    QDBusAbstractInterface* iapi = qobject_cast<QDBusAbstractInterface*>(api);
    QTextStream qStdOut(stdout, QIODevice::WriteOnly);
    if(action == "get"){
        auto value = api->property(args.at(2).toStdString().c_str());
        if(iapi != nullptr){
            auto error = iapi->lastError();
            if(error.type() != QDBusError::NoError){
                qDebug() << "Failed to get value" << iapi->lastError();
#ifdef SENTRY
                sentry_breadcrumb("error", "Failed to get value");
#endif
                return qExit(EXIT_FAILURE);
            }
        }
        qStdOut << toJson(value).toStdString().c_str() << Qt::endl;
    }else if(action == "set"){
        auto property = args.at(2).toStdString();
        if(!api->setProperty(property.c_str(), args.at(3).toStdString().c_str())){
            qDebug() << "Failed to set value";
            if(iapi != nullptr){
                qDebug() << iapi->lastError();
            }
#ifdef SENTRY
            sentry_breadcrumb("error", "Failed to get value");
#endif
            return qExit(EXIT_FAILURE);
        }
    }else if(action == "listen"){
        if(Oxide::DBusConnect(
            (QDBusAbstractInterface*)api,
            QString(args.at(2).toStdString().c_str()),
            [](QVariantList args){
                QTextStream qStdOut(stdout, QIODevice::WriteOnly);
                if(args.size() > 1){
                    qStdOut << toJson(args).toStdString().c_str() << Qt::endl;
                }else if(args.size() == 1 && !args.first().isNull()){
                    qStdOut << toJson(args.first()).toStdString().c_str() << Qt::endl;
                }else{
                    qStdOut << "undefined" << Qt::endl;
                }
            },
            [](){ qApp->exit(EXIT_SUCCESS); },
            parser.isSet("once")
        )){
            return app.exec();
        }
        qDebug() << "Unable to listen to signal";
        if(iapi != nullptr){
            qDebug() << bus.interface()->lastError();
        }
        return qExit(EXIT_FAILURE);
    }else if(action == "call"){
        auto method = args.at(2);
        args = args.mid(3);
        QVariantList arguments;
        if(args.size() > 0){
            for(auto arg : args){
                if(!arg.contains(":")){
                    qDebug() << "Arguments must be in the following format: <QVariant>:<Value>";
#ifdef SENTRY
                    sentry_breadcrumb("error", "Argument has bad format");
#endif
                    return qExit(EXIT_FAILURE);
                }
                auto type = arg.mid(0, arg.indexOf(":"));
                auto value = arg.mid(arg.indexOf(":") + 1);
                int id = QMetaType::type(type.toStdString().c_str());
                if(id == QMetaType::UnknownType){
                    qDebug() << "Unknown type" << type;
#ifdef SENTRY
                    sentry_breadcrumb("error", "Unknown type");
#endif
                    return qExit(EXIT_FAILURE);
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
#ifdef SENTRY
                    sentry_breadcrumb("error", "Unable to convert to type");
#endif
                    return qExit(EXIT_FAILURE);
                }
                variant.convert(id);
                if(!variant.isValid()){
                    qDebug() << "Failed converting to " << type;
                    qDebug() << "Value" << variant;
#ifdef SENTRY
                    sentry_breadcrumb("error", "Failed converting");
#endif
                    return qExit(EXIT_FAILURE);
                }
                arguments.append(variant);
            }
        }
        if(iapi == nullptr){
#ifdef SENTRY
            sentry_breadcrumb("error", "Cannot handle calls for non-dbus APIs");
#endif
            qDebug() << "Cannot handle calls for non-dbus APIs";
            return qExit(EXIT_SUCCESS);
        }
        QDBusMessage reply = iapi->callWithArgumentList(QDBus::Block, method, arguments);
        auto result = reply.arguments();
        if(result.size() > 1){
            qStdOut << toJson(result).toStdString().c_str() << Qt::endl;
        }else if(!result.first().isNull()){
            qStdOut << toJson(result.first()).toStdString().c_str() << Qt::endl;
        }
        if(!reply.errorName().isEmpty()){
#ifdef SENTRY
            sentry_breadcrumb("error", reply.errorMessage().toStdString().c_str());
#endif
            return qExit(EXIT_SUCCESS);
        }
        if(apiName == "system" && (method == "inhibitSleep" || method == "inhibitPowerOff")){
            return app.exec();
        }
    }
    return qExit(EXIT_SUCCESS);
}
