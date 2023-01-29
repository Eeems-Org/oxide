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

#include "mxcfb.h"
#include "screenapi.h"
#include "fifohandler.h"
#include "buttonhandler.h"

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/meta.h"

#define DEFAULT_PATH "/opt/bin:/opt/sbin:/opt/usr/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin"

class SandBoxProcess : public QProcess{
    Q_OBJECT

public:
    SandBoxProcess(QObject* parent = nullptr)
    : QProcess(parent), m_gid(0), m_uid(0), m_chroot(""), m_mask(0) {}

    bool setUser(const QString& name){
        try{
            m_uid = Oxide::getUID(name);
            return true;
        }
        catch(const std::runtime_error&){
            return false;
        }
    }
    bool setGroup(const QString& name){
        try{
            m_gid = Oxide::getGID(name);
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
    Q_PROPERTY(QByteArray screenCapture READ screenCapture)

public:
    Application(QDBusObjectPath path, QObject* parent) : Application(path.path(), parent) {}
    Application(QString path, QObject* parent) : QObject(parent), m_path(path), m_backgrounded(false), fifos() {
        m_process = new SandBoxProcess(this);
        connect(m_process, &SandBoxProcess::started, this, &Application::started);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&SandBoxProcess::finished), [=](int exitCode, QProcess::ExitStatus status){
            Q_UNUSED(status);
            finished(exitCode);
        });
        connect(m_process, &SandBoxProcess::readyReadStandardError, this, &Application::readyReadStandardError);
        connect(m_process, &SandBoxProcess::readyReadStandardOutput, this, &Application::readyReadStandardOutput);
        connect(m_process, &SandBoxProcess::stateChanged, this, &Application::stateChanged);
        connect(m_process, &SandBoxProcess::errorOccurred, this, &Application::errorOccurred);
    }
    ~Application() {
        stopNoSecurityCheck();
        unregisterPath();
        if(m_screenCapture != nullptr){
            delete m_screenCapture;
        }
        umountAll();
        if(p_stdout != nullptr){
            delete p_stdout;
        }
        if(p_stderr != nullptr){
            delete p_stderr;
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
    QByteArray screenCapture(){
        if(!hasPermission("permissions")){
            return QByteArray();
        }
        return screenCaptureNoSecurityCheck();
    }
    QByteArray screenCaptureNoSecurityCheck(){ return qUncompress(*m_screenCapture); }

    const QVariantMap& getConfig(){ return m_config; }
    void setConfig(const QVariantMap& config);
    void saveScreen(){
        if(m_screenCapture != nullptr){
            return;
        }
        Oxide::Sentry::sentry_transaction("application", "saveScreen", [this](Oxide::Sentry::Transaction* t){
            qDebug() << "Saving screen...";
            QByteArray bytes;
            Oxide::Sentry::sentry_span(t, "save", "Save the framebuffer", [&bytes]{
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                if(!EPFrameBuffer::framebuffer()->save(&buffer, "JPG", 100)){
                    O_WARNING("Failed to save buffer");
                }
            });
            qDebug() << "Compressing data...";
            Oxide::Sentry::sentry_span(t, "compress", "Compress the framebuffer", [this, bytes]{
                m_screenCapture = new QByteArray(qCompress(bytes));
            });
            qDebug() << "Screen saved " << m_screenCapture->size() << "bytes";
        });
    }
    void recallScreen(){
        if(m_screenCapture == nullptr){
            return;
        }
        Oxide::Sentry::sentry_transaction("application", "recallScreen", [this](Oxide::Sentry::Transaction* t){
            qDebug() << "Uncompressing screen...";
            QImage img;
            Oxide::Sentry::sentry_span(t, "decompress", "Decompress the framebuffer", [this, &img]{
                img = QImage::fromData(screenCaptureNoSecurityCheck(), "JPG");
            });
            if(img.isNull()){
                qDebug() << "Screen capture was corrupt";
                qDebug() << m_screenCapture->size();
                delete m_screenCapture;
                return;
            }
            qDebug() << "Recalling screen...";
            Oxide::Sentry::sentry_span(t, "recall", "Recall the screen", [this, img]{
                auto size = EPFrameBuffer::framebuffer()->size();
                QRect rect(0, 0, size.width(), size.height());
                QPainter painter(EPFrameBuffer::framebuffer());
                painter.drawImage(rect, img);
                painter.end();
                EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::HighQualityGrayscale, EPFrameBuffer::FullUpdate, true);
                EPFrameBuffer::waitForLastUpdate();

                delete m_screenCapture;
                m_screenCapture = nullptr;
            });
            qDebug() << "Screen recalled.";
        });
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
    void showSplashScreen();
    void started();
    void finished(int exitCode);
    void readyReadStandardError(){
        QString error = m_process->readAllStandardError();
        if(p_stderr != nullptr){
            *p_stderr << error.toStdString().c_str();
            p_stderr->flush();
        }else{
            const char* prefix = ("[" + name() + " " + QString::number(m_process->processId()) + "]").toUtf8();
            for(QString line : error.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts)){
                if(!line.isEmpty()){
                    sd_journal_print(LOG_ERR, "%s %s", prefix, (const char*)line.toUtf8());
                }
            }
        }
    }
    void readyReadStandardOutput(){
        QString output = m_process->readAllStandardOutput();
        if(p_stdout != nullptr){
            *p_stdout << output.toStdString().c_str();
            p_stdout->flush();
        }else{
            const char* prefix = ("[" + name() + " " + QString::number(m_process->processId()) + "]").toUtf8();
            for(QString line : output.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts)){
                if(!line.isEmpty()){
                    sd_journal_print(LOG_INFO, "%s %s", prefix, (const char*)line.toUtf8());
                }
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
                if(sharedSettings.applicationUsage()){
                    if(span != nullptr){
                        Oxide::Sentry::stop_span(span);
                        delete span;
                        span = nullptr;
                    }
                    if(transaction != nullptr){
                        Oxide::Sentry::stop_transaction(transaction);
                        delete transaction;
                        transaction = nullptr;
                    }
                }
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
    QByteArray* m_screenCapture = nullptr;
    QElapsedTimer timer;
    QMap<QString, FifoHandler*> fifos;
    Oxide::Sentry::Transaction* transaction = nullptr;
    Oxide::Sentry::Span* span = nullptr;
    QTextStream* p_stdout = nullptr;
    QTextStream* p_stderr = nullptr;

    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
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
            O_WARNING("Failed to create bindmount: " << ::strerror(errno));
            return;
        }
        if(!readOnly){
            return;
        }
        if(mount(csource.c_str(), ctarget.c_str(), NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL)){
            O_WARNING("Failed to remount bindmount read only: " << ::strerror(errno));
        }
        qDebug() << "mount ro" << source << target;
    }
    void sysfs(const QString& path){
        mkdirs(path, 744);
        umount(path);
        qDebug() << "sysfs" << path;
        if(mount("none", path.toStdString().c_str(), "sysfs", 0, "")){
            O_WARNING("Failed to mount sysfs: " << ::strerror(errno));
        }
    }
    void ramdisk(const QString& path){
        mkdirs(path, 744);
        umount(path);
        qDebug() << "ramdisk" << path;
        if(mount("tmpfs", path.toStdString().c_str(), "tmpfs", 0, "size=249m,mode=755")){
            O_WARNING("Failed to create ramdisk: " << ::strerror(errno));
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
            O_WARNING(target << "Already mounted");
            return fifos.contains(name) ? fifos[name] : nullptr;
        }
        auto source = resourcePath() + "/" + name;
        if(!QFile::exists(source)){
            if(::mkfifo(source.toStdString().c_str(), 0644)){
                O_WARNING("Failed to create " << name << " fifo: " << ::strerror(errno));
            }
        }
        if(!QFile::exists(source)){
            O_WARNING("No fifo for " << name);
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
            O_WARNING("Failed to create symlink: " << ::strerror(errno));
            return;
        }
    }
    const QString resourcePath() { return "/tmp/tarnish-chroot/" + name(); }
    const QString chrootPath() { return resourcePath() + "/chroot"; }
    void mountAll(){
        Oxide::Sentry::sentry_transaction("application", "mount", [this](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
            if(t != nullptr){
                sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
            }
#endif
            auto path = chrootPath();
            qDebug() << "Setting up chroot" << path;
            Oxide::Sentry::sentry_span(t, "bind", "Bind directories", [this, path]{
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
            });
            Oxide::Sentry::sentry_span(t, "ramdisk", "Create ramdisks", [this, path]{
                // tmpfs folders
                mkdirs(path + "/tmp", 744);
                if(!QFile::exists(path + "/run")){
                    ramdisk(path + "/run");
                }
                if(!QFile::exists(path + "/var/volatile")){
                    ramdisk(path + "/var/volatile");
                }
            });
            Oxide::Sentry::sentry_span(t, "configured", "Bind configured directories", [this, path]{
                // Configured folders
                for(auto directory : directories()){
                    bind(directory, path + directory);
                }
            });
            Oxide::Sentry::sentry_span(t, "fifo", "Create fifos", [this, path]{
                // Fake sys devices
                auto fifo = mkfifo("powerState", path + "/sys/power/state");
                connect(fifo, &FifoHandler::dataRecieved, this, &Application::powerStateDataRecieved);
            });
            Oxide::Sentry::sentry_span(t, "symlink", "Create symlinks", [this, path]{
                // Missing symlinks
                symlink(path + "/var/run", "../run");
                symlink(path + "/var/lock", "../run/lock");
                symlink(path + "/var/tmp", "volatile/tmp");
            });
        });
    }
    void umountAll(){
        Oxide::Sentry::sentry_transaction("application", "umount", [this](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
            if(t != nullptr){
                sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
            }
#endif
            auto path = chrootPath();
            Oxide::Sentry::sentry_span(t, "fifos", "Remove fifos", [this]{
                for(auto name : fifos.keys()){
                    auto fifo = fifos.take(name);
                    fifo->quit();
                    fifo->deleteLater();
                }
            });
            QDir dir(path);
            if(!dir.exists()){
                return;
            }
            qDebug() << "Tearing down chroot" << path;
            Oxide::Sentry::sentry_span(t, "dirs", "Remove directories", [dir]{
                for(auto file : dir.entryList(QDir::Files)){
                    QFile::remove(file);
                }
            });
            Oxide::Sentry::sentry_span(t, "umount", "Unmount all mounts", [this]{
                for(auto mount : getActiveApplicationMounts()){
                    umount(mount);
                }
            });
            if(!getActiveApplicationMounts().isEmpty()){
                qDebug() << "Some items are still mounted in chroot" << path;
                return;
            }
            Oxide::Sentry::sentry_span(t, "rm", "Remove final folder", [&dir]{
                dir.removeRecursively();
            });
        });
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
    void startSpan(std::string operation, std::string description);
};

#endif // APPLICATION_H
