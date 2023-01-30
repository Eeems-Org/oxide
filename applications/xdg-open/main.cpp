#include <QCommandLineParser>

#include <string>
#include <sys/stat.h>
#include <liboxide.h>
#include <liboxide/applications.h>

using namespace Oxide::Sentry;
using namespace Oxide::Applications;
using namespace codes::eeems::oxide1;

int launchOxideApp(const QString& name){
    auto bus = QDBusConnection::systemBus();
    General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    QDBusObjectPath path = api.requestAPI("apps");
    if(path.path() == "/"){
        qDebug() << "Unable to get apps API";
        return EXIT_FAILURE;
    }
    Apps apps(OXIDE_SERVICE, path.path(), bus);
    path = apps.getApplicationPath(name);
    if(path.path() == "/"){
        qDebug() << "Application does not exist:" << name.toStdString().c_str();
        return EXIT_FAILURE;
    }
    Application app(OXIDE_SERVICE, path.path(), bus);
    app.launch().waitForFinished();
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("xdg-open", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("xdg-open");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Open a file or URL.");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);
    QStringList args = parser.positionalArguments();
    if (args.isEmpty() || args.length() > 1) {
        parser.showHelp(EXIT_FAILURE);
    }
    auto path = args.first();
    auto url = QUrl::fromUserInput(path, QDir::currentPath(), QUrl::AssumeLocalFile);
    if(url.scheme().isEmpty()){
        url.setScheme("file");
    }
    if(url.isLocalFile()){
        QFileInfo info(path);
        if(info.suffix() != "oxide"){
            qDebug() << "The extension is not supported:" << path.toStdString().c_str();
            return EXIT_FAILURE;
        }
        if(info.absoluteDir().path() != OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY){
            qDebug() << "The registration file must be in " OXIDE_APPLICATION_REGISTRATIONS_DIRECTORY ":" << path.toStdString().c_str();
            return EXIT_FAILURE;
        }
        // Workaround because basename will sometimes return the suffix instead
        auto name = info.fileName().left(info.fileName().length() - 6);
        return launchOxideApp(name);
    }else if(url.scheme() == "oxide"){
        if(url.hasFragment() || url.hasQuery() || !url.userInfo().isEmpty() || !url.authority().isEmpty()){
            qDebug() << "Url must bein the format oxide://{appname} :" << path.toStdString().c_str();
            return EXIT_FAILURE;
        }
        auto name = url.path();
        return launchOxideApp(name);
    }
    qDebug() << "Operation not supported:" << path.toStdString().c_str();
    return EXIT_FAILURE;
}
