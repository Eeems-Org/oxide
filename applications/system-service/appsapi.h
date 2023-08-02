#ifndef APPSAPI_H
#define APPSAPI_H

#include <QDBusObjectPath>
#include <QSettings>

#include "apibase.h"
#include "application.h"

#define OXIDE_SETTINGS_VERSION 1

#define appsAPI AppsAPI::singleton()

using namespace Oxide;
using namespace Oxide::Applications;

class AppsAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_APPS_INTERFACE)
    Q_PROPERTY(int state READ state) // This needs to be here for the XML to generate the other properties :(
    Q_PROPERTY(QDBusObjectPath startupApplication READ startupApplication WRITE setStartupApplication)
    Q_PROPERTY(QDBusObjectPath lockscreenApplication READ lockscreenApplication WRITE setLockscreenApplication)
    Q_PROPERTY(QDBusObjectPath processManagerApplication READ processManagerApplication WRITE setProcessManagerApplication)
    Q_PROPERTY(QDBusObjectPath taskSwitcherApplication READ taskSwitcherApplication WRITE setTaskSwitcherApplication)
    Q_PROPERTY(QVariantMap applications READ getApplications)
    Q_PROPERTY(QStringList previousApplications READ getPreviousApplications)
    Q_PROPERTY(QDBusObjectPath currentApplication READ currentApplication)
    Q_PROPERTY(QVariantMap runningApplications READ runningApplications)
    Q_PROPERTY(QVariantMap pausedApplications READ pausedApplications)

public:
    static Window* _window();
    static AppsAPI* singleton(AppsAPI* self = nullptr);
    AppsAPI(QObject* parent);
    ~AppsAPI();
    void startup();
    void shutdown();
    int state() { return 0; } // Ignore this, it's a kludge to get the xml to generate

    void setEnabled(bool enabled);
    bool isEnabled();

    Q_INVOKABLE QDBusObjectPath registerApplication(QVariantMap properties);
    QDBusObjectPath registerApplicationNoSecurityCheck(QVariantMap properties);
    Q_INVOKABLE bool unregisterApplication(QDBusObjectPath path);

    Q_INVOKABLE void reload();

    QDBusObjectPath startupApplication();
    void setStartupApplication(QDBusObjectPath path);
    QDBusObjectPath lockscreenApplication();
    void setLockscreenApplication(QDBusObjectPath path);
    QDBusObjectPath processManagerApplication();
    void setProcessManagerApplication(QDBusObjectPath path);
    QDBusObjectPath taskSwitcherApplication();
    void setTaskSwitcherApplication(QDBusObjectPath path);

    QVariantMap getApplications();

    QDBusObjectPath currentApplication();
    QDBusObjectPath currentApplicationNoSecurityCheck();

    QVariantMap runningApplications();
    QVariantMap runningApplicationsNoSecurityCheck();
    QVariantMap pausedApplications();

    void unregisterApplication(Application* app);
    void pauseAll();
    void resumeIfNone();
    Application* getApplication(QDBusObjectPath path);
    QStringList getPreviousApplications();
    Q_INVOKABLE QDBusObjectPath getApplicationPath(const QString& name);
    Application* getApplication(const QString& name);
    Application* getApplication(const pid_t pgid);
    void connectSignals(Application* app, int signal);
    void disconnectSignals(Application* app, int signal);
    Q_INVOKABLE bool previousApplication();
    bool previousApplicationNoSecurityCheck();
    void forceRecordPreviousApplication();
    void recordPreviousApplication();
    void removeFromPreviousApplications(QString name);
    bool stopping();

signals:
    void applicationRegistered(QDBusObjectPath);
    void applicationLaunched(QDBusObjectPath);
    void applicationUnregistered(QDBusObjectPath);
    void applicationPaused(QDBusObjectPath);
    void applicationResumed(QDBusObjectPath);
    void applicationSignaled(QDBusObjectPath);
    void applicationExited(QDBusObjectPath, int);

public slots:
    void openDefaultApplication();
    void openTaskManager();
    void openLockScreen();
    void openTaskSwitcher();

private:
    bool m_stopping;
    bool m_starting;
    bool m_enabled;
    QMap<QString, Application*> applications;
    QStringList previousApplications;
    QSettings settings;
    QDBusObjectPath m_startupApplication;
    QDBusObjectPath m_lockscreenApplication;
    QDBusObjectPath m_processManagerApplication;
    QDBusObjectPath m_taskSwitcherApplication;
    bool m_sleeping;
    Application* resumeApp = nullptr;
    QString getPath(QString name);
    QString _noApplicationsMessage = "No applications have been found. This is the result of invalid configuration. Open an issue on\nhttps://github.com/Eeems-Org/oxide\nto get support resolving this.";
    QString _noForegroundAppMessage = "No foreground application currently running. Open an issue on\nhttps://github.com/Eeems-Org/oxide\nto get support resolving this.";

    void writeApplications();
    void readApplications();
    static void migrate(QSettings* settings, int fromVersion);
    bool locked();
    void ensureForegroundApp();
};
#endif // APPSAPI_H
