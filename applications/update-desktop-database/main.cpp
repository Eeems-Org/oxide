#include <QCommandLineParser>

#include <liboxide.h>

using namespace codes::eeems::oxide1;
using namespace Oxide::Sentry;
using namespace Oxide::JSON;

#define LOG_VERBOSE(msg) if(!parser.isSet(quietOption) && parser.isSet(verboseOption)){qDebug() << msg;}
#define LOG(msg) if(!parser.isSet(quietOption)){qDebug() << msg;}

int qExit(int ret){
    QTimer::singleShot(0, [ret](){
        qApp->exit(ret);
    });
    return qApp->exec();
}

QList<QString> configDirectoryPaths = { "/opt/etc/draft", "/etc/draft", "/home/root/.config/draft" };

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("update-desktop-database", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("update-desktop-database");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("A program to send desktop notifications");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addPositionalArgument("directory", "NOT IMPLEMENTED", "[DIRECTORY...]");
    QCommandLineOption versionOption("version", "Display the version and exit");
    parser.addOption(versionOption);
    QCommandLineOption quietOption(
        {"q", "quiet"},
        "Do not display any information about processing and updating progress."
    );
    parser.addOption(quietOption);
    QCommandLineOption verboseOption(
        {"v", "verbose"},
        "Display more information about processing and upating progress"
    );
    parser.addOption(verboseOption);
    parser.process(app);
    if(parser.isSet(versionOption)){
        parser.showVersion();
    }
    auto bus = QDBusConnection::systemBus();
    General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    LOG_VERBOSE("Requesting apps API");
    QDBusObjectPath path = api.requestAPI("apps");
    if(path.path() == "/"){
        LOG("Unable to get apps API");
        return qExit(EXIT_FAILURE);
    }
    Apps apps(OXIDE_SERVICE, path.path(), bus);
    LOG("Reloading applications");
    apps.reload().waitForFinished();
    LOG_VERBOSE("Finished reloading applications");
    LOG("Importing Draft Applications");
    for(auto configDirectoryPath : configDirectoryPaths){
        QDir configDirectory(configDirectoryPath);
        configDirectory.setFilter( QDir::Files | QDir::NoSymLinks | QDir::NoDot | QDir::NoDotDot);
        auto images = configDirectory.entryInfoList(QDir::NoFilter,QDir::SortFlag::Name);
        for(QFileInfo fi : images){
            if(fi.fileName() != "conf"){
                auto f = fi.absoluteFilePath();
                LOG_VERBOSE("parsing file " << f);
                QFile file(fi.absoluteFilePath());
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    if(!parser.isSet(quietOption)){
                        qCritical() << "Couldn't find the file " << f;
                    }
                    continue;
                }
                QTextStream in(&file);
                QVariantMap properties;
                while (!in.atEnd()) {
                    QString line = in.readLine();
                    if(line.startsWith("#") || line.trimmed().isEmpty()){
                        continue;
                    }
                    QStringList parts = line.split("=");
                    if(parts.length() != 2){
                        if(!parser.isSet(quietOption)){
                            O_WARNING("wrong format on " << line);
                        }
                        continue;
                    }
                    QString lhs = parts.at(0);
                    QString rhs = parts.at(1);
                    if(rhs != ":" && rhs != ""){
                        if(lhs == "name"){
                            properties.insert("name", rhs);
                        }else if(lhs == "desc"){
                            properties.insert("description", rhs);
                        }else if(lhs == "imgFile"){
                            auto icon = configDirectoryPath + "/icons/" + rhs + ".png";
                            if(icon.startsWith("qrc:")){
                                icon = "";
                            }
                            properties.insert("icon", icon);
                        }else if(lhs == "call"){
                            properties.insert("bin", rhs);
                        }else if(lhs == "term"){
                            properties.insert("onStop", rhs.trimmed());
                        }
                    }
                }
                file.close();
                auto name = properties["name"].toString();
                path = apps.getApplicationPath(name);
                if(path.path() != "/"){
                    LOG_VERBOSE("Already exists" << name);
                    auto icon = properties["icon"].toString();
                    if(icon.isEmpty()){
                        continue;
                    }
                    Application application(OXIDE_SERVICE, path.path(), bus);
                    if(application.icon().isEmpty()){
                        application.setIcon(icon);
                    }
                    continue;
                }
                LOG_VERBOSE("Not found, creating...");
                properties.insert("displayName", name);
                path = apps.registerApplication(properties);
                if(path.path() == "/"){
                    LOG("Failed to import" << name);
                }
            }
        }
    }
    LOG_VERBOSE("Finished Importing Draft Applications");
    return qExit(EXIT_SUCCESS);
}
