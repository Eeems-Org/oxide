#include <QCommandLineParser>
#include <QJsonValue>
#include <QJsonDocument>
#include <QSet>

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
    sentry_init("notify-send", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("notify-send");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("a program to send desktop notifications");
    parser.applicationDescription();
    parser.addVersionOption();
    parser.addPositionalArgument("summary", "Summary of the notification", "{SUMMARY}");
    parser.addPositionalArgument("body", "Body of the notification", "[BODY]");
    QCommandLineOption appNameOption(
        {"a", "app-name"},
        "Specifies the app name for the notification.",
        "APP_NAME"
    );
    parser.addOption(appNameOption);
    QCommandLineOption iconOption(
        {"i", "icon"},
        "Specifies an icon filename or stock icon to display.",
        "ICON"
    );
    parser.addOption(iconOption);
    QCommandLineOption expireOption(
        {"t", "expire-time"},
        "The duration, in milliseconds to wait for the notification. Does nothing without --wait",
        "TIME"
    );
    parser.addOption(expireOption);
    QCommandLineOption printOption(
        {"p", "print-id"},
        "Print the notification identifier."
    );
    parser.addOption(printOption);
    QCommandLineOption replaceOption(
        {"r", "replace-id"},
        "The identifier of the notification to replace.",
        "REPLACE_ID"
    );
    parser.addOption(replaceOption);
    QCommandLineOption waitOption(
        {"w", "wait"},
        "Wait for the notification to be closed before exiting. If the expire-time is set, it will be used as the maximum waiting time."
    );
    parser.addOption(waitOption);
    QCommandLineOption transientOption(
        {"e", "transient"},
        "Show a transient notification. This notification will be removed immediatly after being shown on the screen"
    );
    parser.addOption(transientOption);
    QCommandLineOption urgencyOption(
        {"u", "urgency"},
        "NOT IMPLEMENTED",
        "LEVEL"
    );
    parser.addOption(urgencyOption);
    QCommandLineOption appOption(
        {"A", "action"},
        "NOT IMPLEMENTED",
        "[NAME=]TEXT"
    );
    parser.addOption(appOption);
    QCommandLineOption categoryOption(
        {"c", "category"},
        "NOT IMPLEMENTED",
        "TYPE[,TYPE]"
    );
    parser.addOption(categoryOption);
    QCommandLineOption hintOption(
        {"h", "hint"},
        "NOT IMPLEMENTED",
        "TYPE:NAME:VALUE"
    );
    parser.addOption(hintOption);
    QCommandLineOption helpOption(
        {"?", "help"},
        "Show help and exit"
    );
    parser.addOption(helpOption);
    parser.process(app);
    if(parser.isSet(helpOption)){
        parser.showHelp();
    }
    QStringList args = parser.positionalArguments();
    if(args.isEmpty()){
#ifdef SENTRY
        sentry_breadcrumb("error", "No arguments");
#endif
        parser.showHelp(EXIT_FAILURE);
    }
    if(args.count() > 2){
#ifdef SENTRY
        sentry_breadcrumb("error", "Too many arguments");
#endif
        parser.showHelp(EXIT_FAILURE);
    }
    auto bus = QDBusConnection::systemBus();
    General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
    qDebug() << "Requesting notification API...";
    QDBusObjectPath path = api.requestAPI("notification");
    if(path.path() == "/"){
        qDebug() << "Unable to get notification API";
        return qExit(EXIT_FAILURE);
    }
    Notifications notifications(OXIDE_SERVICE, path.path(), bus);
    QString guid;
    auto appName = parser.isSet(appNameOption) ? parser.value(appNameOption) : "codes.eeems.notify-send";
    auto iconPath = parser.isSet(iconOption) ? parser.value(iconOption) : "";
    auto text = args.join("\n");
    if(parser.isSet(replaceOption)){
        guid = parser.value(replaceOption);
        path = notifications.get(guid);
        if(path.path() == "/" || !notifications.take(guid)){
            guid = QUuid::createUuid().toString();
            path = notifications.add(guid, appName, text, iconPath);
        }
        if(path.path() == "/"){
            qDebug() << "Failed to add notification";
            return qExit(EXIT_FAILURE);
        }
        Notification notification(OXIDE_SERVICE, path.path(), bus);
        notification.setApplication(appName);
        notification.setIcon(iconPath);
        notification.setText(text);
    }else{
        guid = QUuid::createUuid().toString();
        path = notifications.add(guid, appName, text, iconPath);
        if(path.path() == "/"){
            qDebug() << "Failed to add notification";
            return qExit(EXIT_FAILURE);
        }
    }
    Notification notification(OXIDE_SERVICE, path.path(), bus);
    if(parser.isSet(printOption)){
        QTextStream qStdOut(stdout, QIODevice::WriteOnly);
        qStdOut << guid << Qt::endl;
    }
    qDebug() << "Displaying notification" << guid;
    notification.display().waitForFinished();
    if(parser.isSet(waitOption)){
        qDebug() << "Waiting for notification to be closed";
        if(!Oxide::DBusConnect(
            &notification,
            "removed",
            [](QVariantList args){
                Q_UNUSED(args);
                qApp->exit(EXIT_SUCCESS);
            },
            true
        )){
            qDebug() << "Failed to wait for notification to exit";
            return qExit(EXIT_FAILURE);
        }
        if(parser.isSet(expireOption)){
            auto timeout = parser.value(expireOption);
            qDebug() << ("Timeout set to " + timeout + "ms").toStdString().c_str();
            QTimer::singleShot(timeout.toInt(), [&notification]{
                qDebug() << "Notification wait timed out";
                notification.remove().waitForFinished();
                qApp->exit(EXIT_FAILURE);
            });
        }
        return app.exec();
    }else if(parser.isSet(transientOption)){
        notification.remove();
    }
    return qExit(EXIT_SUCCESS);
}
