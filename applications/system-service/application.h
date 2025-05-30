#ifndef APPLICATION_H
#define APPLICATION_H

#include <QObject>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QProcess>
#include <QProcessEnvironment>
#include <QElapsedTimer>
#include <QTime>
#include <QDir>
#include <QCoreApplication>

#include <zlib.h>
#include <systemd/sd-journal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <cstdlib>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <stdexcept>
#include <sys/types.h>
#include <algorithm>
#include <liboxide.h>
#include <libblight/types.h>

class Notification;
#include "notification.h"

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/meta.h"

#define DEFAULT_PATH "/opt/bin:/opt/sbin:/opt/usr/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin"

class ApplicationProcess : public QProcess{
    Q_OBJECT

public:
    ApplicationProcess(QObject* parent = nullptr);

    bool setUser(const QString& name);
    bool setGroup(const QString& name);
    void setMask(mode_t mask);

protected:
    void setupChildProcess() override;

private:
    gid_t m_gid;
    uid_t m_uid;
    mode_t m_mask;
};

class Application : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_APPLICATION_INTERFACE)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(int processId READ processId)
    Q_PROPERTY(QStringList permissions READ permissions WRITE setPermissions NOTIFY permissionsChanged)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY displayNameChanged)
    Q_PROPERTY(QString description READ description)
    Q_PROPERTY(QString bin READ bin)
    Q_PROPERTY(QString onPause READ onPause WRITE setOnPause NOTIFY onPauseChanged)
    Q_PROPERTY(QString onResume READ onResume WRITE setOnResume NOTIFY onResumeChanged)
    Q_PROPERTY(QString onStop READ onStop WRITE setOnStop NOTIFY onStopChanged)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(int type READ type)
    Q_PROPERTY(int state READ state)
    Q_PROPERTY(bool systemApp READ systemApp)
    Q_PROPERTY(bool hidden READ hidden)
    Q_PROPERTY(bool transient READ transient)
    Q_PROPERTY(QString icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QVariantMap environment READ environment NOTIFY environmentChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(QString user READ user)
    Q_PROPERTY(QString group READ group)

public:
    Application(QDBusObjectPath path, QObject* parent) : Application(path.path(), parent) {}
    Application(QString path, QObject *parent);
    ~Application();

    QString path();
    QDBusObjectPath qPath();
    void registerPath();
    void unregisterPath();
    enum ApplicationState { Inactive, InForeground, InBackground, Paused };
    Q_ENUM(ApplicationState)

    Q_INVOKABLE void launch();
    Q_INVOKABLE void pause(bool startIfNone = true);
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void unregister();

    void launchNoSecurityCheck();
    void resumeNoSecurityCheck();
    void stopNoSecurityCheck();
    void pauseNoSecurityCheck(bool startIfNone = true);
    void unregisterNoSecurityCheck();
    QString name();
    int processId();
    QStringList permissions();
    void setPermissions(QStringList permissions);
    QString displayName();
    void setDisplayName(QString displayName);
    QString description();
    QString bin();
    QString onPause();
    void setOnPause(QString onPause);
    QString onResume();
    void setOnResume(QString onResume);
    QString onStop();
    void setOnStop(QString onStop);
    QStringList flags();
    bool autoStart();
    void setAutoStart(bool autoStart);
    bool systemApp();
    bool transient();
    bool hidden();
    int type();
    int state();
    int stateNoSecurityCheck();
    QString icon();
    void setIcon(QString icon);
    QVariantMap environment();
    Q_INVOKABLE void setEnvironment(QVariantMap environment);
    QString workingDirectory();
    void setWorkingDirectory(const QString& workingDirectory);
    QString user();
    QString group();

    const QVariantMap& getConfig();
    void setConfig(const QVariantMap& config);
    void waitForFinished();
    void signal(int signal);
    QVariant value(QString name, QVariant defaultValue = QVariant());
    void setValue(QString name, QVariant value);
    void interruptApplication();
    void uninterruptApplication();
    void waitForPause();
    void waitForResume();
    QString id();

signals:
    void launched();
    void paused();
    void resumed();
    void unregistered();
    void exited(int);
    void permissionsChanged(QStringList);
    void displayNameChanged(QString);
    void onPauseChanged(QString);
    void onResumeChanged(QString);
    void onStopChanged(QString);
    void autoStartChanged(bool);
    void iconChanged(QString);
    void environmentChanged(QVariantMap);
    void workingDirectoryChanged(QString);
    void directoriesChanged(QStringList);

public slots:
    void sigUsr1();
    void sigUsr2();

private slots:
    void started();
    void finished(int exitCode);
    void readyReadStandardError();
    void readyReadStandardOutput();
    void stateChanged(QProcess::ProcessState state);
    void errorOccurred(QProcess::ProcessError error);

private:
    QVariantMap m_config;
    QString m_path;
    ApplicationProcess* m_process;
    bool m_backgrounded;
    QByteArray* m_screenCapture = nullptr;
    QElapsedTimer timer;
    Oxide::Sentry::Transaction* transaction = nullptr;
    Oxide::Sentry::Span* span = nullptr;
    int p_stdout_fd = -1;
    QTextStream* p_stdout = nullptr;
    int p_stderr_fd = -1;
    QTextStream* p_stderr = nullptr;
    Notification* m_notification = nullptr;
    QImage* m_backup = nullptr;

    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
    void delayUpTo(int milliseconds);
    void updateEnvironment();
    void startSpan(std::string operation, std::string description);
};

#endif // APPLICATION_H
