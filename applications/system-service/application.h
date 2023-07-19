#ifndef APPLICATION_H
#define APPLICATION_H

#include <QObject>
#include <QDebug>
#include <QDBusObjectPath>
#include <QProcess>
#include <QElapsedTimer>

#include "apibase.h"
#include "fifohandler.h"

class Window;

class SandBoxProcess : public QProcess{
    Q_OBJECT

public:
    SandBoxProcess(QObject* parent = nullptr);

    bool setUser(const QString& name);
    bool setGroup(const QString& name);
    bool setChroot(const QString& path);
    void setMask(mode_t mask);

protected:
    void setupChildProcess() override;

private:
    gid_t m_gid;
    uid_t m_uid;
    QString m_chroot;
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
    Q_PROPERTY(QString splash READ splash WRITE setSplash NOTIFY splashChanged)
    Q_PROPERTY(QVariantMap environment READ environment NOTIFY environmentChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(bool chroot READ chroot)
    Q_PROPERTY(QString user READ user)
    Q_PROPERTY(QString group READ group)
    Q_PROPERTY(QStringList directories READ directories WRITE setDirectories NOTIFY directoriesChanged)
    Q_PROPERTY(QByteArray screenCapture READ screenCapture)

public:
    static Window* _window();
    static void shutdown();

    Application(QDBusObjectPath path, QObject* parent);
    Application(QString path, QObject* parent);
    ~Application();

    QString path() { return m_path; }
    QDBusObjectPath qPath(){ return QDBusObjectPath(path()); }
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
    QString splash();
    void setSplash(QString splash);
    QVariantMap environment();
    Q_INVOKABLE void setEnvironment(QVariantMap environment);
    QString workingDirectory();
    void setWorkingDirectory(const QString& workingDirectory);
    bool chroot();
    QString user();
    QString group();
    QStringList directories();
    void setDirectories(QStringList directories);
    QByteArray screenCapture();
    QByteArray screenCaptureNoSecurityCheck();

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
    void splashChanged(QString);
    void environmentChanged(QVariantMap);
    void workingDirectoryChanged(QString);
    void directoriesChanged(QStringList);

public slots:
    void sigUsr1(){
        timer.invalidate();
    }
    void sigUsr2(){
        timer.invalidate();
    }

private slots:
    void showSplashScreen();
    void started();
    void finished(int exitCode);
    void readyReadStandardError();
    void readyReadStandardOutput();
    void stateChanged(QProcess::ProcessState state);
    void errorOccurred(QProcess::ProcessError error);
    void powerStateDataRecieved(FifoHandler* handler, const QString& data);

private:
    QVariantMap m_config;
    QString m_path;
    SandBoxProcess* m_process;
    bool m_backgrounded;
    QByteArray* m_screenCapture = nullptr;
    QElapsedTimer timer;
    QMap<QString, FifoHandler*> fifos;
    Oxide::Sentry::Transaction* transaction = nullptr;
    Oxide::Sentry::Span* span = nullptr;
    int p_stdout_fd = -1;
    QTextStream* p_stdout = nullptr;
    int p_stderr_fd = -1;
    QTextStream* p_stderr = nullptr;
    pid_t m_pid;

    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
    void delayUpTo(int milliseconds);
    void updateEnvironment();
    void mkdirs(const QString& path, mode_t mode = 0700);
    void bind(const QString& source, const QString& target, bool readOnly = false);
    void sysfs(const QString& path);
    void ramdisk(const QString& path);
    void umount(const QString& path);
    FifoHandler* mkfifo(const QString& name, const QString& target);
    void symlink(const QString& source, const QString& target);
    const QString resourcePath();
    const QString chrootPath();
    void mountAll();
    void umountAll();
    bool isMounted(const QString& path);
    QStringList getActiveApplicationMounts();
    QStringList getActiveMounts();
    void startSpan(std::string operation, std::string description);
    void saveScreen();
    void recallScreen();
};

#endif // APPLICATION_H
