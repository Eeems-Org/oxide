#include <QCommandLineParser>

#include "dbusservice_interface.h"
#include "batteryapi_interface.h"

#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("rot");
    QCoreApplication::setApplicationVersion("1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Oxide settings tool");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("api", "API to work with");
    parser.addPositionalArgument("action", "get or set");
    parser.addPositionalArgument("property", "Property to interact with");
    parser.addPositionalArgument("value", "Value to set the property to");

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if(args.length() < 3){
        parser.showHelp(EXIT_FAILURE);
    }
    auto action = args.at(1);
    if(action != "get" && action != "set"){
        parser.showHelp(EXIT_FAILURE);
    }
    if(action == "set" && args.length() < 4){
        parser.showHelp(EXIT_FAILURE);
    }

    auto bus = QDBusConnection::systemBus();
    codes::eeems::oxide1::General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);

    auto name = args.at(0);
    auto property = args.at(2).toStdString();
    if(name == "battery"){
        auto reply = api.requestAPI("battery");
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
        codes::eeems::oxide1::Battery batteryApi(OXIDE_SERVICE, path, bus);
        if(action == "get"){
            qDebug() << batteryApi.property(property.c_str());
        }else if(action == "set"){
            if(!batteryApi.setProperty(property.c_str(), args.at(3).toStdString().c_str())){
                qDebug() << "Failed to set value";
                return EXIT_FAILURE;
            }
            qDebug() << batteryApi.property(property.c_str());
        }
    }else{
        qDebug() << "Unable to work with " + name;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
