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
#include <sys/prctl.h>
#include <stdexcept>
#include <sys/types.h>
#include <algorithm>

#include "dbussettings.h"
#include "mxcfb.h"
#include "screenapi.h"
#include "fifohandler.h"
#include "buttonhandler.h"

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
        catch(const std::runtime_error&){
            return false;
        }
    }
    bool setGroup(const QString& name){
        try{
            m_gid = getGID(name);
            return true;
        }
        catch(const std::runtime_error&){
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
        setsid();
        prctl(PR_SET_PDEATHSIG, SIGTERM);
    }
private:
    gid_t m_gid;
    uid_t m_uid;
    QString m_chroot;
    mode_t m_mask;

    uid_t getUID(const QString& name){
        auto user = getpwnam(name.toStdString().c_str());
        if(user == NULL){
            throw std::runtime_error("Invalid user name: " + name.toStdString());
        }
        return user->pw_uid;
    }
    gid_t getGID(const QString& name){
        auto group = getgrnam(name.toStdString().c_str());
        if(group == NULL){
            throw std::runtime_error("Invalid group name: " + name.toStdString());
        }
        return group->gr_gid;
    }
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
    Q_PROPERTY(QString icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QString splash READ splash WRITE setSplash NOTIFY splashChanged)
    Q_PROPERTY(QVariantMap environment READ environment NOTIFY environmentChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(bool chroot READ chroot)
    Q_PROPERTY(QString user READ user)
    Q_PROPERTY(QString group READ group)
    Q_PROPERTY(QStringList directories READ directories WRITE setDirectories NOTIFY directoriesChanged)
public:
    Application(QDBusObjectPath path, QObject* parent) : Application(path.path(), parent) {}
    Application(QString path, QObject* parent) : QObject(parent), m_path(path), m_backgrounded(false), fifos() {
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
        if(screenCapture != nullptr){
            delete screenCapture;
        }
        umountAll();
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

    void launchNoSecurityCheck();
    void resumeNoSecurityCheck();
    void stopNoSecurityCheck();
    void pauseNoSecurityCheck(bool startIfNone = true);
    void unregisterNoSecurityCheck();
    QString name() { return value("name").toString(); }
    int processId() { return m_process->processId(); }
    QStringList permissions() { return value("permissions", QStringList()).toStringList(); }
    void setPermissions(QStringList permissions){
        if(!hasPermission("permissions")){
            return;
        }
        setValue("permissions", permissions);
        emit permissionsChanged(permissions);
    }
    QString displayName() { return value("displayName", name()).toString(); }
    void setDisplayName(QString displayName){
        if(!hasPermission("permissions")){
            return;
        }
        setValue("displayName", displayName);
        emit displayNameChanged(displayName);
    }
    QString description() { return value("description", displayName()).toString(); }
    QString bin() { return value("bin").toString(); }
    QString onPause() { return value("onPause", "").toString(); }
    void setOnPause(QString onPause){
        if(!hasPermission("permissions")){
            return;
        }
        setValue("onPause", onPause);
        emit onPauseChanged(onPause);
    }
    QString onResume() { return value("onResume", "").toString(); }
    void setOnResume(QString onResume){
        if(!hasPermission("permissions")){
            return;
        }
        setValue("onResume", onResume);
        emit onResumeChanged(onResume);
    }
    QString onStop() { return value("onStop", "").toString(); }
    void setOnStop(QString onStop){
        if(!hasPermission("permissions")){
            return;
        }
        setValue("onStop", onStop);
        emit onStopChanged(onStop);
    }
    QStringList flags() { return value("flags", QStringList()).toStringList(); }
    bool autoStart() { return flags().contains("autoStart"); }
    void setAutoStart(bool autoStart) {
        if(!hasPermission("permissions")){
            return;
        }
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
    int state(){
        if(!hasPermission("apps")){
            return Inactive;
        }
        return stateNoSecurityCheck();
    }
    int stateNoSecurityCheck();
    QString icon() { return value("icon", "").toString(); }
    void setIcon(QString icon){
        if(!hasPermission("permissions")){
            return;
        }
        setValue("icon", icon);
        emit iconChanged(icon);
    }
    QString splash() { return value("splash", "").toString(); }
    void setSplash(QString splash){
        if(!hasPermission("permissions")){
            return;
        }
        setValue("splash", splash);
        emit splashChanged(splash);
    }
    QVariantMap environment() { return value("environment", QVariantMap()).toMap(); }
    Q_INVOKABLE void setEnvironment(QVariantMap environment){
        if(!hasPermission("permissions")){
            return;
        }
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
        if(!hasPermission("permissions")){
            return;
        }
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
    QStringList directories() { return value("directories", QStringList()).toStringList(); }
    void setDirectories(QStringList directories){
        if(!hasPermission("permissions")){
            return;
        }
        setValue("directories", directories);
        emit directoriesChanged(directories);
    }
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
    void powerStateDataRecieved(FifoHandler* handler, const QString& data);
private:
    QVariantMap m_config;
    QString m_path;
    SandBoxProcess* m_process;
    bool m_backgrounded;
    QByteArray* screenCapture = nullptr;
    size_t screenCaptureSize;
    QElapsedTimer timer;
    QMap<QString, FifoHandler*> fifos;

    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
    void showSplashScreen();
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
        if(QFileInfo(source).isDir()){
            mkdirs(target, 744);
        }else{
            mkdirs(QFileInfo(target).dir().path(), 744);
        }
        auto ctarget = target.toStdString();
        auto csource = source.toStdString();
        qDebug() << "mount" << source << target;
        if(mount(csource.c_str(), ctarget.c_str(), NULL, MS_BIND, NULL)){
            qWarning() << "Failed to create bindmount: " << ::strerror(errno);
            return;
        }
        if(!readOnly){
            return;
        }
        if(mount(csource.c_str(), ctarget.c_str(), NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL)){
            qWarning() << "Failed to remount bindmount read only: " << ::strerror(errno);
        }
        qDebug() << "mount ro" << source << target;
    }
    void sysfs(const QString& path){
        mkdirs(path, 744);
        umount(path);
        qDebug() << "sysfs" << path;
        if(mount("none", path.toStdString().c_str(), "sysfs", 0, "")){
            qWarning() << "Failed to mount sysfs: " << ::strerror(errno);
        }
    }
    void ramdisk(const QString& path){
        mkdirs(path, 744);
        umount(path);
        qDebug() << "ramdisk" << path;
        if(mount("tmpfs", path.toStdString().c_str(), "tmpfs", 0, "size=249m,mode=755")){
            qWarning() << "Failed to create ramdisk: " << ::strerror(errno);
        }
    }
    void umount(const QString& path){
        if(!isMounted(path)){
            return;
        }
        auto cpath = path.toStdString();
        ::umount(cpath.c_str());
        if(isMounted(path)){
            qDebug() << "umount failed" << path;
            return;
        }
        QDir dir(path);
        if(dir.exists()){
            rmdir(cpath.c_str());
        }
        qDebug() << "umount" << path;
    }
    FifoHandler* mkfifo(const QString& name, const QString& target){
        if(isMounted(target)){
            qWarning() << target << "Already mounted";
            return fifos.contains(name) ? fifos[name] : nullptr;
        }
        auto source = resourcePath() + "/" + name;
        if(!QFile::exists(source)){
            if(::mkfifo(source.toStdString().c_str(), 0644)){
                qWarning() << "Failed to create " << name << " fifo: " << ::strerror(errno);
            }
        }
        if(!QFile::exists(source)){
            qWarning() << "No fifo for " << name;
            return fifos.contains(name) ? fifos[name] : nullptr;
        }
        bind(source, target);
        if(!fifos.contains(name)){
            qDebug() << "Creating fifo thread for" << source;
            auto handler = new FifoHandler(name, source.toStdString().c_str(), this);
            qDebug() << "Connecting fifo thread events for" << source;
            connect(handler, &FifoHandler::finished, [this, name]{
                if(fifos.contains(name)){
                    fifos.take(name);
                }
            });
            fifos[name] = handler;
            qDebug() << "Starting fifo thread for" << source;
            handler->start();
            qDebug() << "Fifo thread for " << source << "started";
        }
        return fifos[name];
    }
    void symlink(const QString& source, const QString& target){
        if(QFile::exists(source)){
            return;
        }
        qDebug() << "symlink" << source << target;
        if(::symlink(target.toStdString().c_str(), source.toStdString().c_str())){
            qWarning() << "Failed to create symlink: " << ::strerror(errno);
            return;
        }
    }
    const QString resourcePath() { return "/tmp/tarnish-chroot/" + name(); }
    const QString chrootPath() { return resourcePath() + "/chroot"; }
    void mountAll(){
        auto path = chrootPath();
        qDebug() << "Setting up chroot" << path;
        // System tmpfs folders
        bind("/dev", path + "/dev");
        bind("/proc", path + "/proc");
        sysfs(path + "/sys");
        // Folders required to run things
        bind("/bin", path + "/bin", true);
        bind("/sbin", path + "/sbin", true);
        bind("/lib", path + "/lib", true);
        bind("/usr/lib", path + "/usr/lib", true);
        bind("/usr/bin", path + "/usr/bin", true);
        bind("/usr/sbin", path + "/usr/sbin", true);
        bind("/opt/bin", path + "/opt/bin", true);
        bind("/opt/lib", path + "/opt/lib", true);
        bind("/opt/usr/bin", path + "/opt/usr/bin", true);
        bind("/opt/usr/lib", path + "/opt/usr/lib", true);
        // tmpfs folders
        mkdirs(path + "/tmp", 744);
        if(!QFile::exists(path + "/run")){
            ramdisk(path + "/run");
        }
        if(!QFile::exists(path + "/var/volatile")){
            ramdisk(path + "/var/volatile");
        }
        // Configured folders
        for(auto directory : directories()){
            bind(directory, path + directory);
        }
        // Fake sys devices
        auto fifo = mkfifo("powerState", path + "/sys/power/state");
        connect(fifo, &FifoHandler::dataRecieved, this, &Application::powerStateDataRecieved);
        // Missing symlinks
        symlink(path + "/var/run", "../run");
        symlink(path + "/var/lock", "../run/lock");
        symlink(path + "/var/tmp", "volatile/tmp");
    }
    void umountAll(){
        auto path = chrootPath();
        for(auto name : fifos.keys()){
            auto fifo = fifos.take(name);
            fifo->quit();
            fifo->deleteLater();
        }
        QDir dir(path);
        if(!dir.exists()){
            return;
        }
        qDebug() << "Tearing down chroot" << path;
        for(auto file : dir.entryList(QDir::Files)){
            QFile::remove(file);
        }
        for(auto mount : getActiveApplicationMounts()){
            umount(mount);
        }
        if(!getActiveApplicationMounts().isEmpty()){
            qDebug() << "Some items are still mounted in chroot" << path;
            return;
        }
        dir.removeRecursively();
    }
    bool isMounted(const QString& path){ return getActiveMounts().contains(path); }
    QStringList getActiveApplicationMounts(){
        auto path = chrootPath() + "/";
        QStringList activeMounts = getActiveMounts().filter(QRegularExpression("^" + QRegularExpression::escape(path) + ".*"));
        activeMounts.sort(Qt::CaseSensitive);
        std::reverse(std::begin(activeMounts), std::end(activeMounts));
        return activeMounts;
    }
    QStringList getActiveMounts(){
        QFile mounts("/proc/mounts");
        if(!mounts.open(QIODevice::ReadOnly)){
            qDebug() << "Unable to open /proc/mounts";
            return QStringList();
        }
        QString line;
        QStringList activeMounts;
        while(!(line = mounts.readLine()).isEmpty()){
            auto mount = line.section(' ', 1, 1);
            if(mount.startsWith("/")){
                activeMounts.append(mount);
            }

        }
        mounts.close();
        activeMounts.sort(Qt::CaseSensitive);
        std::reverse(std::begin(activeMounts), std::end(activeMounts));
        return activeMounts;
    }
};

#endif // APPLICATION_H
