#include <QCommandLineParser>

#include <string>
#include <sys/stat.h>
#include <liboxide.h>
#include <liboxide/applications.h>

using namespace codes::eeems::oxide1;
using namespace Oxide::Sentry;
using namespace Oxide::JSON;

const QSet<QString> ApiNames = QSet<QString>{
    "general",
    "power",
    "wifi",
    "apps",
    "system",
    "screen",
    "notification",
    "gui",
};

QTextStream& qStdOut(){
    static QTextStream ts( stdout );
    return ts;
}

QObject* getApi(const QString& name){
    if(!ApiNames.contains(name)){
        return nullptr;
    }
    auto bus = QDBusConnection::systemBus();
    if(!bus.isConnected()){
        qDebug() << "xdg-settings: Failed to connect to DBus";
        return nullptr;
    }
    if(name == "general"){
        return new General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    }
    General generalApi(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    auto reply = generalApi.requestAPI(name);
    reply.waitForFinished();
    if(reply.isError()){
        qDebug() << "xdg-settings: Failed to connect to general API";
        return nullptr;
    }
    auto path = ((QDBusObjectPath)reply).path();
    if(path == "/"){
        qDebug() << "xdg-settings: Failed to get API path from tarnish";
        return nullptr;
    }
    if(name == "power"){
        return new Power(OXIDE_SERVICE, path, bus);
    }
    if(name == "wifi"){
        return new Wifi(OXIDE_SERVICE, path, bus);
    }
    if(name == "apps"){
        return new Apps(OXIDE_SERVICE, path, bus);
    }
    if(name == "system"){
        return new System(OXIDE_SERVICE, path, bus);
    }
    if(name == "screen"){
        return new Screen(OXIDE_SERVICE, path, bus);
    }
    if(name == "notification"){
        return new Notifications(OXIDE_SERVICE, path, bus);
    }
    if(name == "gui"){
        return new Gui(OXIDE_SERVICE, path, bus);
    }
    qDebug() << "xdg-settings: Unknown API" << name;
    return nullptr;
}

bool isValidProperty(QObject* obj, const QString& name){
    auto meta = obj->metaObject();
    for(int i = meta->propertyOffset(); i < meta->propertyCount(); ++i){
        auto property = meta->property(i);
        if(property.name() == name){
            return property.isReadable() && property.isWritable();
        }
    }
    return false;
}

QList<QString> getProperties(QObject* obj){
    QList<QString> res;
    auto meta = obj->metaObject();
    for(int i = meta->propertyOffset(); i < meta->propertyCount(); ++i){
        auto property = meta->property(i);
        auto name = property.name();
        if(!QString(name).startsWith("__META_GROUP_") && property.isReadable() && property.isWritable()){
            res.append(name);
        }
    }
    return res;
}

QStringList positionArguments(QCommandLineParser& parser, int mandatoryCount, int maxCount){
    parser.process(*qApp);
    auto args = parser.positionalArguments();
    args.removeFirst();
    auto len = args.length();
    parser.parse(args);
    if(len < mandatoryCount || len > maxCount){
        parser.showHelp(EXIT_FAILURE);
    }
    return args;
}

QObject* getObj(QStringList* args, int isGet = false){
    switch(args->length()){
        case 2:
            if(!isGet){
                return &sharedSettings;
            }
            break;
        case 1:
            return &sharedSettings;
        case 3:
        default:
            break;
    }
    auto name = args->first();
    QObject* obj = getApi(name);
    args->removeFirst();
    return obj;
}

int get(QCommandLineParser& parser){
    parser.clearPositionalArguments();
    parser.addPositionalArgument("", "", "get");
    parser.addPositionalArgument("property", "Property to get", "{property}");
    parser.addPositionalArgument("subproperty", "Subproperty to get", "[subproperty]");
    auto args = positionArguments(parser, 1, 2);

    QObject* obj = getObj(&args, true);
    if(obj == nullptr){
        parser.showHelp(EXIT_FAILURE);
    }
    auto name = args.first();
    if(!isValidProperty(obj, name)){
        qDebug() << "xdg-settings: Unknown property" << name;
        qDebug() << args;
        parser.showHelp(EXIT_FAILURE);
    }
    QVariant value = obj->property(name.toStdString().c_str());
    if(!value.isValid()){
        qDebug() << "xdg-settings: Failed to get property";
        parser.showHelp(EXIT_FAILURE);
    }
    qStdOut() << toJson(value).toStdString().c_str() << Qt::endl;
    return EXIT_SUCCESS;
}

int check(QCommandLineParser& parser){
    parser.clearPositionalArguments();
    parser.addPositionalArgument("", "", "check");
    parser.addPositionalArgument("property", "Property to check", "{property}");
    parser.addPositionalArgument("subproperty", "Subproperty to check", "[subproperty]");
    parser.addPositionalArgument("value", "Value to check if the property/subproperty is", "value");
    auto args = positionArguments(parser, 2, 3);

    QObject* obj = getObj(&args);
    if(obj == nullptr){
        parser.showHelp(EXIT_FAILURE);
    }
    auto name = args.first();
    if(!isValidProperty(obj, name)){
        qDebug() << "xdg-settings: Unknown property" << name;
        parser.showHelp(EXIT_FAILURE);
    }
    QVariant value = obj->property(name.toStdString().c_str());
    if(!value.isValid()){
        qDebug() << "xdg-settings: Failed to get property";
        parser.showHelp(EXIT_FAILURE);
    }
    qStdOut() << (args.last() == value ? "yes" : "no") << Qt::endl;
    return EXIT_SUCCESS;
}

int set(QCommandLineParser& parser){
    parser.clearPositionalArguments();
    parser.addPositionalArgument("", "", "set");
    parser.addPositionalArgument("property", "Property to set", "{property}");
    parser.addPositionalArgument("subproperty", "Subproperty to set", "[subproperty]");
    parser.addPositionalArgument("value", "Value to set the property/subproperty to", "value");
    auto args = positionArguments(parser, 2, 3);

    QObject* obj = getObj(&args);
    if(obj == nullptr){
        parser.showHelp(EXIT_FAILURE);
    }
    auto name = args.first();
    if(!isValidProperty(obj, name)){
        qDebug() << "xdg-settings: Unknown property" << name;
        parser.showHelp(EXIT_FAILURE);
    }
    QVariant value = obj->property(name.toStdString().c_str());
    if(!value.isValid()){
        qDebug() << "xdg-settings: Failed to get property";
        parser.showHelp(EXIT_FAILURE);
    }
    if(obj->setProperty(name.toStdString().c_str(), args.last())){
        return EXIT_SUCCESS;
    }
    QDBusAbstractInterface* api = qobject_cast<QDBusAbstractInterface*>(obj);
    if(api == nullptr){
        qDebug() << "xdg-settings: failed to set property";
    }else{
        qDebug() << "xdg-settings:" << api->lastError();
    }
    return EXIT_FAILURE;
}

int list(QCommandLineParser& parser){
    if(!parser.positionalArguments().isEmpty()){
        parser.showHelp(EXIT_FAILURE);
    }
    qStdOut() << "Known properties:" << Qt::endl;
    for(auto name : getProperties(&sharedSettings)){
        qStdOut() << "  " << name << Qt::endl;
    }
    for(auto name : ApiNames){
        auto api = getApi(name);
        if(api == nullptr){
            return EXIT_FAILURE;
        }
        auto props = getProperties(api);
        if(!props.isEmpty()){
            qStdOut() << "  " << name << Qt::endl;
            for(auto prop : props){
                qStdOut() << "    " << prop << Qt::endl;
            }
        }
        delete api;
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("xdg-settings", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("xdg-settings");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Get various settings from the desktop environment");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption listOption("list", "List all properties xdg-settings knows about.");
    parser.addOption(listOption);
    parser.addPositionalArgument("Commands:",
        "get   Get the value of a setting.\n"
        "check Check to see if a setting is a specific value.\n"
        "set   Set the value of a setting.\n"
    );

    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);
    parser.parse(app.arguments());
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    QStringList args = parser.positionalArguments();
    if(parser.isSet(listOption)){
        return list(parser);
    }
    if (args.isEmpty()) {
        parser.showHelp(EXIT_FAILURE);
    }


    auto command = args.first();
    if(command == "get"){
        parser.clearPositionalArguments();
        return get(parser);
    }
    if(command == "check"){
        parser.clearPositionalArguments();
        return check(parser);
    }
    if(command == "set"){
        parser.clearPositionalArguments();
        return set(parser);
    }
    parser.showHelp(EXIT_FAILURE);
}
