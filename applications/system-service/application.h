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
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include "dbussettings.h"
#include "mxcfb.h"
#include "screenapi.h"
#include "inputmanager.h"

#define DEFAULT_PATH "/opt/bin:/opt/sbin:/opt/usr/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin"

class SandBoxProcess : public QProcess{
    Q_OBJECT
public:
    SandBoxProcess(QObject* parent = nullptr)
    : QProcess(parent), m_gid(0), m_uid(0), m_chroot(""), m_mask(0) {}

    bool setUser(const QString& name){
        try{
            m_uid = getUID(name);
            return true;
        }
        catch(const runtime_error&){
            return false;
        }
    }
    bool setGroup(const QString& name){
        try{
            m_gid = getGID(name);
            return true;
        }
        catch(const runtime_error&){
            return false;
        }
    }
    bool setChroot(const QString& path){
        if(path.isEmpty() || path == "/"){
            m_chroot = "";
            return true;
        }
        QDir dir(path);
        if(dir.exists()){
            m_chroot = path;
            return true;
        }
        return false;
    }
    void setMask(mode_t mask){
        m_mask = mask;
    }
protected:
    void setupChildProcess() override {
        // Drop all privileges in the child process
        setgroups(0, 0);
        if(!m_chroot.isEmpty()){
            // enter a chroot jail.
            chroot(m_chroot.toStdString().c_str());
        }
        // Change to correct user
        setresgid(m_gid, m_gid, m_gid);
        setresuid(m_uid, m_uid, m_uid);
        umask(m_mask);
        setpgid(getpid(), 0);
    }
private:
    gid_t m_gid;
    uid_t m_uid;
    QString m_chroot;
    mode_t m_mask;

    uid_t getUID(const QString& name){
        auto user = getpwnam(name.toStdString().c_str());
        if(user == NULL){
            throw runtime_error("Invalid user name: " + name.toStdString());
        }
        return user->pw_uid;
    }
    gid_t getGID(const QString& name){
        auto group = getgrnam(name.toStdString().c_str());
        if(group == NULL){
            throw runtime_error("Invalid group name: " + name.toStdString());
        }
        return group->gr_gid;
    }
};

class Application : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_APPLICATION_INTERFACE)
    Q_PROPERTY(QString name READ name)
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
    Q_PROPERTY(QString icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QVariantMap environment READ environment NOTIFY environmentChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(bool chroot READ chroot)
    Q_PROPERTY(QString user READ user)
    Q_PROPERTY(QString group READ group)
public:
    Application(QDBusObjectPath path, QObject* parent) : Application(path.path(), parent) {}
    Application(QString path, QObject* parent) : QObject(parent), m_path(path), m_backgrounded(false) {
        m_process = new SandBoxProcess(this);
        connect(m_process, &SandBoxProcess::started, this, &Application::started);
        connect(m_process, QOverload<int>::of(&SandBoxProcess::finished), this, &Application::finished);
        connect(m_process, &SandBoxProcess::readyReadStandardError, this, &Application::readyReadStandardError);
        connect(m_process, &SandBoxProcess::readyReadStandardOutput, this, &Application::readyReadStandardOutput);
        connect(m_process, &SandBoxProcess::stateChanged, this, &Application::stateChanged);
        connect(m_process, &SandBoxProcess::errorOccurred, this, &Application::errorOccurred);
    }
    ~Application() {
        unregisterPath();
        m_process->kill();
        m_process->deleteLater();
        if(screenCapture != nullptr){
            delete screenCapture;
        }
    }

    QString path() { return m_path; }
    QDBusObjectPath qPath(){ return QDBusObjectPath(path()); }
    void registerPath(){
        auto bus = QDBusConnection::systemBus();
        bus.unregisterObject(path(), QDBusConnection::UnregisterTree);
        if(bus.registerObject(path(), this, QDBusConnection::ExportAllContents)){
            qDebug() << "Registered" << path() << OXIDE_APPLICATION_INTERFACE;
        }else{
            qDebug() << "Failed to register" << path();
        }
    }
    void unregisterPath(){
        auto bus = QDBusConnection::systemBus();
        if(bus.objectRegisteredAt(path()) != nullptr){
            qDebug() << "Unregistered" << path();
            bus.unregisterObject(path());
        }
    }
    enum ApplicationState { Inactive, InForeground, InBackground, Paused };
    Q_ENUM(ApplicationState)

    Q_INVOKABLE void launch();
    Q_INVOKABLE void pause(bool startIfNone = true);
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void unregister();

    QString name() { return value("name").toString(); }
    QString displayName() { return value("displayName", name()).toString(); }
    void setDisplayName(QString displayName){
        setValue("displayName", displayName);
        emit displayNameChanged(displayName);
    }
    QString description() { return value("description", displayName()).toString(); }
    QString bin() { return value("bin").toString(); }
    QString onPause() { return value("onPause", "").toString(); }
    void setOnPause(QString onPause){
        setValue("onPause", onPause);
        emit onPauseChanged(onPause);
    }
    QString onResume() { return value("onResume", "").toString(); }
    void setOnResume(QString onResume){
        setValue("onResume", onResume);
        emit onResumeChanged(onResume);
    }
    QString onStop() { return value("onStop", "").toString(); }
    void setOnStop(QString onStop){
        setValue("onStop", onStop);
        emit onStopChanged(onStop);
    }
    QStringList flags() { return value("flags", QStringList()).toStringList(); }
    bool autoStart() { return flags().contains("autoStart"); }
    void setAutoStart(bool autoStart) {
        if(!autoStart){
            flags().removeAll("autoStart");
            autoStartChanged(autoStart);
        }else if(!this->autoStart()){
            flags().append("autoStart");
            autoStartChanged(autoStart);
        }
    }
    bool systemApp() { return flags().contains("system"); }
    bool hidden() { return flags().contains("hidden"); }
    int type() { return (int)value("type", 0).toInt(); }
    int state();
    QString icon() { return value("icon", "").toString(); }
    void setIcon(QString icon){
        setValue("icon", icon);
        emit iconChanged(icon);
    }
    QVariantMap environment() { return value("environment", QVariantMap()).toMap(); }
    Q_INVOKABLE void setEnvironment(QVariantMap environment){
        for(auto key : environment.keys()){
            auto value = environment.value(key, QVariant());
            if(!value.isValid()){
                qDebug() << key << " has invalid value: " << value;
                return;
            }
        }
        setValue("environment", environment);
        updateEnvironment();
        emit environmentChanged(environment);
    }
    QString workingDirectory() { return value("workingDirectory", "/").toString(); }
    void setWorkingDirectory(const QString& workingDirectory){
        QDir dir(workingDirectory);
        if(!dir.exists()){
            return;
        }
        setValue("workingDirectory", workingDirectory);
        emit workingDirectoryChanged(workingDirectory);
    }
    bool chroot(){ return flags().contains("chroot"); }
    QString user(){ return value("user", getuid()).toString(); }
    QString group(){ return value("group", getgid()).toString(); }
    const QVariantMap& getConfig(){ return m_config; }
    void setConfig(const QVariantMap& config);
    void saveScreen(){
        if(screenCapture != nullptr){
            return;
        }
        qDebug() << "Saving screen...";
        int frameBufferHandle = open("/dev/fb0", O_RDWR);
        char* frameBuffer = (char*)mmap(0, DISPLAYSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, frameBufferHandle, 0);
        qDebug() << "Compressing data...";
        auto compressedData = qCompress(QByteArray(frameBuffer, DISPLAYSIZE));
        close(frameBufferHandle);
        screenCapture = new QByteArray(compressedData);
        qDebug() << "Screen saved.";
    }
    void recallScreen(){
        if(screenCapture == nullptr){
            return;
        }
        qDebug() << "Uncompressing screen...";
        auto uncompressedData = qUncompress(*screenCapture);
        if(!uncompressedData.size()){
            qDebug() << "Screen capture was corrupt";
            qDebug() << screenCapture->size();
            delete screenCapture;
            return;
        }
        qDebug() << "Recalling screen...";
        int frameBufferHandle = open("/dev/fb0", O_RDWR);
        auto frameBuffer = (char*)mmap(0, DISPLAYSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, frameBufferHandle, 0);
        memcpy(frameBuffer, uncompressedData, DISPLAYSIZE);

        mxcfb_update_data update_data;
        mxcfb_rect update_rect;
        update_rect.top = 0;
        update_rect.left = 0;
        update_rect.width = DISPLAYWIDTH;
        update_rect.height = DISPLAYHEIGHT;
        update_data.update_marker = 0;
        update_data.update_region = update_rect;
        update_data.waveform_mode = WAVEFORM_MODE_AUTO;
        update_data.update_mode = UPDATE_MODE_FULL;
        update_data.dither_mode = EPDC_FLAG_USE_DITHERING_MAX;
        update_data.temp = TEMP_USE_REMARKABLE_DRAW;
        update_data.flags = 0;
        ioctl(frameBufferHandle, MXCFB_SEND_UPDATE, &update_data);

        close(frameBufferHandle);
        delete screenCapture;
        screenCapture = nullptr;
        qDebug() << "Screen recalled.";
    }
    void waitForFinished(){
        if(m_process->processId()){
            m_process->waitForFinished();
        }
    }
    void signal(int signal);
    QVariant value(QString name, QVariant defaultValue = QVariant()){ return m_config.value(name, defaultValue); }
    void setValue(QString name, QVariant value){ m_config[name] = value; }
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
    void displayNameChanged(QString);
    void onPauseChanged(QString);
    void onResumeChanged(QString);
    void onStopChanged(QString);
    void autoStartChanged(bool);
    void iconChanged(QString);
    void environmentChanged(QVariantMap);
    void workingDirectoryChanged(QString);

public slots:
    void sigUsr1(){
        timer.invalidate();
    }
    void sigUsr2(){
        timer.invalidate();
    }

private slots:
    void started();
    void finished(int exitCode);
    void readyReadStandardError(){
        const char* prefix = ("[" + name() + " " + QString::number(m_process->processId()) + "]").toUtf8();
        QString error = m_process->readAllStandardError();
        for(QString line : error.split(QRegExp("[\r\n]"), QString::SkipEmptyParts)){
            if(!line.isEmpty()){
                sd_journal_print(LOG_ERR, "%s %s", prefix, (const char*)line.toUtf8());
            }
        }
    }
    void readyReadStandardOutput(){
        const char* prefix = ("[" + name() + " " + QString::number(m_process->processId()) + "]").toUtf8();
        QString output = m_process->readAllStandardOutput();
        for(QString line : output.split(QRegExp("[\r\n]"), QString::SkipEmptyParts)){
            if(!line.isEmpty()){
                sd_journal_print(LOG_INFO, "%s %s", prefix, (const char*)line.toUtf8());
            }
        }
    }
    void stateChanged(QProcess::ProcessState state){
        switch(state){
            case QProcess::Starting:
                qDebug() << "Application" << name() << "is starting.";
            break;
            case QProcess::Running:
                qDebug() << "Application" << name() << "is running.";
            break;
            case QProcess::NotRunning:
                qDebug() << "Application" << name() << "is not running.";
            break;
            default:
                qDebug() << "Application" << name() << "unknown state" << state;
        }
    }
    void errorOccurred(QProcess::ProcessError error);

private:
    QVariantMap m_config;
    QString m_path;
    SandBoxProcess* m_process;
    bool m_backgrounded;
    QByteArray* screenCapture = nullptr;
    size_t screenCaptureSize;
    QElapsedTimer timer;
    void delayUpTo(int milliseconds){
        timer.invalidate();
        timer.start();
        while(timer.isValid() && !timer.hasExpired(milliseconds)){
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        }
    }
    void updateEnvironment(){
        auto env = QProcessEnvironment::systemEnvironment();
        auto defaults = QString(DEFAULT_PATH).split(":");
        auto envPath = env.value("PATH", DEFAULT_PATH);
        for(auto item : defaults){
            if(!envPath.contains(item)){
                envPath.append(item);
            }
        }
        env.insert("PATH", envPath);
        for(auto key : environment().keys()){
            env.insert(key, environment().value(key, "").toString());
        }
        m_process->setEnvironment(env.toStringList());
    }
    void mkdirs(const QString& path, mode_t mode = 0700){
        QDir dir(path);
        if(!dir.exists()){
            QString subpath = "";
            for(auto part : path.split("/")){
                subpath += "/" + part;
                QDir dir(subpath);
                if(!dir.exists()){
                    mkdir(subpath.toStdString().c_str(), mode);
                }
            }
        }
    }
    void bind(const QString& source, const QString& target, bool readOnly = false){
        umount(target);
        mkdirs(target, 744);
        auto ctarget = target.toStdString();
        auto csource = source.toStdString();
        mount(csource.c_str(), ctarget.c_str(), NULL, MS_BIND, NULL);
        if(readOnly){
            mount(csource.c_str(), ctarget.c_str(), NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
            qDebug() << "mount ro" << source << target;
            return;
        }
        qDebug() << "mount rw" << source << target;
    }
    void ramdisk(const QString& path){
        mkdirs(path, 744);
        mount("tmpfs", path.toStdString().c_str(), "tmpfs", 0, "size=249m,rw,nosuid,nodev,mode=755");
        qDebug() << "ramdisk" << path;
    }
    void umount(const QString& path){
        QDir dir(path);
        if(!dir.exists()){
            return;
        }
        auto cpath = path.toStdString();
        int tries = 0;
        while(::umount(cpath.c_str()) && errno == EBUSY){
            struct timespec args{
                .tv_sec = 1,
                .tv_nsec = 0,
            }, res;
            nanosleep(&args, &res);
            if(++tries < 5){
                continue;
            }
            if(!umount2(cpath.c_str(), MNT_FORCE) || errno != EBUSY){
                break;
            }
            if(++tries >= 10){
                qDebug() << "umount failed" << path;
                return;
            }
        }
        rmdir(cpath.c_str());
        qDebug() << "umount" << path;
    }
    const QString chrootPath() { return "/tmp/tarnish-chroot/" + name(); }
    void mountAll(){
        auto path = chrootPath();
        // System tmpfs folders
        bind("/dev", path + "/dev");
        bind("/proc", path + "/proc");
        bind("/sys", path + "/sys");
        // Folders required to run things
        bind("/bin", path + "/bin", true);
        bind("/lib", path + "/lib", true);
        bind("/usr/lib", path + "/usr/lib", true);
        bind("/usr/bin", path + "/usr/bin", true);
        bind("/usr/sbin", path + "/usr/sbin", true);
        bind("/opt/bin", path + "/opt/bin", true);
        bind("/opt/lib", path + "/opt/lib", true);
        bind("/opt/usr/bin", path + "/opt/usr/bin", true);
        bind("/opt/usr/lib", path + "/opt/usr/lib", true);
        // tmpfs folders
        ramdisk(path + "/run");
        ramdisk(path + "/var/volatile");
    }
    void umountAll(){
        auto path = chrootPath();
        umount(path + "/opt/usr/bin");
        umount(path + "/opt/usr/lib");
        rmdir((path + "/opt/usr").toStdString().c_str());
        umount(path + "/usr/lib");
        umount(path + "/usr/bin");
        umount(path + "/usr/sbin");
        rmdir((path + "/usr").toStdString().c_str());
        umount(path + "/opt/bin");
        umount(path + "/opt/lib");
        rmdir((path + "/opt").toStdString().c_str());
        umount(path + "/bin");
        umount(path + "/lib");
        umount(path + "/var/volatile");
        rmdir((path + "/var").toStdString().c_str());
        umount(path + "/run");
        umount(path + "/proc");
        umount(path + "/sys");
        umount(path + "/dev");
        rmdir(path.toStdString().c_str());
    }
};

#endif // APPLICATION_H
