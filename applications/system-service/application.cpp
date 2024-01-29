#include <QTimer>
#include <QFile>
#include <QTransform>

#include <signal.h>
#include <liboxide.h>
#include <liboxide/oxideqml.h>

#include "application.h"
#include "appsapi.h"
#include "systemapi.h"
#include "digitizerhandler.h"
#include "notificationapi.h"
#include "screenapi.h"
#include "buttonhandler.h"
#include "apibase.h"

using namespace Oxide::Applications;

const event_device touchScreen(deviceSettings.getTouchDevicePath(), O_WRONLY);

void Application::launch(){
    if(!hasPermission("apps")){
        return;
    }
    launchNoSecurityCheck();
}
void Application::launchNoSecurityCheck(){
    if(m_process->processId()){
        resumeNoSecurityCheck();
    }else{
        if(sharedSettings.applicationUsage()){
            transaction = Oxide::Sentry::start_transaction("application", "run");
#ifdef SENTRY
            if(transaction != nullptr){
                sentry_transaction_set_tag(transaction->inner, "application", name().toStdString().c_str());
            }
#endif
            startSpan("starting", "Application is starting");
        }
        Oxide::Sentry::sentry_transaction("application", "launch", [this](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
            if(t != nullptr){
                sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
            }
#else
            Q_UNUSED(t);
#endif
            appsAPI->recordPreviousApplication();
            qDebug() << "Launching " << path();
            appsAPI->pauseAll();
            if(!flags().contains("nosplash")){
                showSplashScreen();
            }
            if(m_process->program() != bin()){
                m_process->setProgram(bin());
            }
            updateEnvironment();
            umountAll();
            if(chroot()){
                mountAll();
                m_process->setChroot(chrootPath());
                m_process->setWorkingDirectory(chrootPath() + "/" + workingDirectory());
            }else{
                m_process->setChroot("");
                m_process->setWorkingDirectory(workingDirectory());
            }
            m_process->setUser(user());
            m_process->setGroup(group());
            if(p_stdout == nullptr){
                p_stdout_fd = sd_journal_stream_fd(name().toStdString().c_str(), LOG_INFO, 1);
                if (p_stdout_fd < 0) {
                    errno = -p_stdout_fd;
                    qDebug() << "Failed to create stdout fd:" << -p_stdout_fd;
                }else{
                    FILE* log = fdopen(p_stdout_fd, "w");
                    if(!log){
                        qDebug() << "Failed to create stdout FILE:" << errno;
                        close(p_stdout_fd);
                    }else{
                        p_stdout = new QTextStream(log);
                        qDebug() << "Opened stdout for " << name();
                    }
                }
            }
            if(p_stderr == nullptr){
                p_stderr_fd = sd_journal_stream_fd(name().toStdString().c_str(), LOG_ERR, 1);
                if (p_stderr_fd < 0) {
                    errno = -p_stderr_fd;
                    qDebug() << "Failed to create sterr fd:" << -p_stderr_fd;
                }else{
                    FILE* log = fdopen(p_stderr_fd, "w");
                    if(!log){
                        qDebug() << "Failed to create stderr FILE:" << errno;
                        close(p_stderr_fd);
                    }else{
                        p_stderr = new QTextStream(log);
                        qDebug() << "Opened stderr for " << name();
                    }
                }
            }
            m_process->start();
            m_process->waitForStarted();
            if(type() == Background){
                startSpan("background", "Application is in the background");
            }else{
                startSpan("foreground", "Application is in the foreground");
            }
        });
    }
}
void Application::pause(bool startIfNone){
    if(!hasPermission("apps")){
        return;
    }
    pauseNoSecurityCheck(startIfNone);
}
void Application::pauseNoSecurityCheck(bool startIfNone){
    if(
        !m_process->processId()
        || stateNoSecurityCheck() == Paused
        || type() == Background
    ){
        return;
    }
    qDebug() << "Pausing " << path();
    Oxide::Sentry::sentry_transaction("application", "pause", [this, startIfNone](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
        if(t != nullptr){
            sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
        }
#else
        Q_UNUSED(t);
#endif
        interruptApplication();
        if(startIfNone){
            appsAPI->resumeIfNone();
        }
        emit paused();
        emit appsAPI->applicationPaused(qPath());
    });
    qDebug() << "Paused " << path();
}
void Application::interruptApplication(){
    if(
        !m_process->processId()
        || stateNoSecurityCheck() == Paused
        || type() == Background
    ){
        return;
    }
    Oxide::Sentry::sentry_transaction("application", "interrupt", [this](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
        if(t != nullptr){
            sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
        }
#else
        Q_UNUSED(t);
#endif
        if(!onPause().isEmpty()){
            Oxide::Sentry::sentry_span(t, "onPause", "Run onPause action", [this](){
                system(onPause().toStdString().c_str());
            });
        }
        Oxide::Sentry::sentry_span(t, "background", "Background application", [this](){
            switch(type()){
                case Background:
                    // Already in the background. How did we get here?
                    startSpan("background", "Application is in the background");
                    return;
                case Backgroundable:
                    qDebug() << "Waiting for SIGUSR2 ack";
                    appsAPI->connectSignals(this, 2);
                    kill(-m_process->processId(), SIGUSR2);
                    timer.restart();
                    delayUpTo(1000);
                    appsAPI->disconnectSignals(this, 2);
                    if(stateNoSecurityCheck() == Inactive){
                        qDebug() << "Application crashed while pausing";
                    }else if(timer.isValid()){
                        qDebug() << "Application took too long to background" << name();
                        kill(-m_process->processId(), SIGSTOP);
                        waitForPause();
                        startSpan("stopped", "Application is stopped");
                    }else{
                        m_backgrounded = true;
                        qDebug() << "SIGUSR2 ack recieved";
                        startSpan("background", "Application is in the background");
                    }
                    break;
                case Foreground:
                default:
                    kill(-m_process->processId(), SIGSTOP);
                    waitForPause();
                    startSpan("stopped", "Application is stopped");
            }
        });
    });
}
void Application::waitForPause(){
    if(stateNoSecurityCheck() == Paused){
        return;
    }
    siginfo_t info;
    waitid(P_PID, m_process->processId(), &info, WSTOPPED);
}
void Application::waitForResume(){
    if(stateNoSecurityCheck() != Paused){
        return;
    }
    siginfo_t info;
    waitid(P_PID, m_process->processId(), &info, WCONTINUED);
}

void Application::sigUsr1(){
    timer.invalidate();
}

void Application::sigUsr2(){
    timer.invalidate();
}
void Application::resume(){
    if(!hasPermission("apps")){
        return;
    }
    resumeNoSecurityCheck();
}
void Application::resumeNoSecurityCheck(){
    if(
        !m_process->processId()
        || stateNoSecurityCheck() == InForeground
        || (type() == Background && stateNoSecurityCheck() == InBackground)
    ){
        qDebug() << "Can't Resume" << path() << "Already running!";
        return;
    }
    Oxide::Sentry::sentry_transaction("application", "resume", [this](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
        if(t != nullptr){
            sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
        }
#else
        Q_UNUSED(t);
#endif
        appsAPI->recordPreviousApplication();
        qDebug() << "Resuming " << path();
        appsAPI->pauseAll();
        uninterruptApplication();
        waitForResume();
        emit resumed();
        emit appsAPI->applicationResumed(qPath());
    });
    qDebug() << "Resumed " << path();
}
void Application::uninterruptApplication(){
    if(
        !m_process->processId()
        || stateNoSecurityCheck() == InForeground
        || (type() == Background && stateNoSecurityCheck() == InBackground)
    ){
        return;
    }
    Oxide::Sentry::sentry_transaction("application", "uninterrupt", [this](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
        if(t != nullptr){
            sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
        }
#else
        Q_UNUSED(t);
#endif
        if(!onResume().isEmpty()){
            Oxide::Sentry::sentry_span(t, "onResume", "Run onResume action", [this](){
                system(onResume().toStdString().c_str());
            });
        }
        Oxide::Sentry::sentry_span(t, "foreground", "Foreground application", [this](){
            switch(type()){
                case Background:
                case Backgroundable:
                    if(stateNoSecurityCheck() == Paused){
                        kill(-m_process->processId(), SIGCONT);
                    }
                    qDebug() << "Waiting for SIGUSR1 ack";
                    appsAPI->connectSignals(this, 1);
                    kill(-m_process->processId(), SIGUSR1);
                    delayUpTo(1000);
                    appsAPI->disconnectSignals(this, 1);
                    if(timer.isValid()){
                        // No need to fall through, we've just assumed it continued
                        qDebug() << "Warning: application took too long to forground" << name();
                    }else{
                        qDebug() << "SIGUSR1 ack recieved";
                    }
                    m_backgrounded = false;
                    startSpan("background", "Application is in the background");
                    break;
                case Foreground:
                default:
                    kill(-m_process->processId(), SIGCONT);
                    startSpan("foreground", "Application is in the foreground");
            }
        });
    });
}
void Application::stop(){
    if(!hasPermission("apps")){
        return;
    }
    stopNoSecurityCheck();
}
void Application::stopNoSecurityCheck(){
    auto state = this->stateNoSecurityCheck();
    if(state == Inactive){
        return;
    }
    Oxide::Sentry::sentry_transaction("application", "stop", [this, state](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
        if(t != nullptr){
            sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
        }
#else
        Q_UNUSED(t);
#endif
        qDebug() << "Stopping " << path();
        if(!onStop().isEmpty()){
            Oxide::Sentry::sentry_span(t, "onStop", "Run onStop action", [this](){
                qDebug() << "onStop: " << onStop();
                qDebug() << "exit code: " << QProcess::execute(onStop(), QStringList());
            });
        }
        Application* pausedApplication = nullptr;
        if(state == Paused){
            Oxide::Sentry::sentry_span(t, "resume", "Resume paused application", [this, &pausedApplication](){
                auto currentApplication = appsAPI->currentApplicationNoSecurityCheck();
                if(currentApplication.path() != path()){
                    pausedApplication = appsAPI->getApplication(currentApplication);
                    if(pausedApplication != nullptr){
                        if(pausedApplication->stateNoSecurityCheck() == Paused){
                            pausedApplication = nullptr;
                        }else{
                            appsAPI->forceRecordPreviousApplication();
                            pausedApplication->interruptApplication();
                        }
                    }
                }
                kill(-m_process->processId(), SIGCONT);
            });
        }
        Oxide::Sentry::sentry_span(t, "stop", "Stop application", [this](){
            kill(-m_process->processId(), SIGTERM);
            // Try to wait for the application to stop normally before killing it
            int tries = 0;
            while(this->stateNoSecurityCheck() != Inactive){
                m_process->waitForFinished(100);
                if(++tries == 5){
                    kill(-m_process->processId(), SIGKILL);
                    break;
                }
            }
        });
        appsAPI->removeFromPreviousApplications(name());
        if(p_stdout != nullptr){
            delete p_stdout;
            p_stdout = nullptr;
        }
        if(p_stderr != nullptr){
            delete p_stderr;
            p_stderr = nullptr;
        }
    });
}
void Application::signal(int signal){
    if(m_process->processId()){
        kill(-m_process->processId(), signal);
    }
}

QVariant Application::value(QString name, QVariant defaultValue){ return m_config.value(name, defaultValue); }

void Application::setValue(QString name, QVariant value){ m_config[name] = value; }
void Application::unregister(){
    if(!hasPermission("apps")){
        return;
    }
    unregisterNoSecurityCheck();
}
void Application::unregisterNoSecurityCheck(){
    emit unregistered();
    appsAPI->unregisterApplication(this);
}

int Application::stateNoSecurityCheck(){
    switch(m_process->state()){
        case QProcess::Starting:
        case QProcess::Running:{
            QFile stat("/proc/" + QString::number(m_process->processId()) + "/stat");
            if(stat.open(QIODevice::ReadOnly)){
                auto status = stat.readLine().split(' ');
                stat.close();
                if(status.length() > 2 && status[2] == "T"){
                    return Paused;
                }
            }
            if(type() == Background || (type() == Backgroundable && m_backgrounded)){
                return InBackground;
            }
            return InForeground;
        }break;
        case QProcess::NotRunning:
        default:
            return Inactive;
    }
}

QString Application::icon(){
    auto _icon = value("icon", "").toString();
    if(_icon.isEmpty() || !_icon.contains("-") || QFile::exists(_icon)){
        return _icon;
    }
    auto path = Oxide::Applications::iconPath(_icon);
    if(path.isEmpty()){
        return _icon;
    }
    return path;
}

void Application::setIcon(QString icon){
    if(!hasPermission("permissions")){
        return;
    }
    setValue("icon", icon);
    emit iconChanged(this->icon());
}

QString Application::splash(){
    auto _splash = value("splash", "").toString();
    if(_splash.isEmpty() || !_splash.contains("-") || QFile::exists(_splash)){
        return _splash;
    }
    auto path = Oxide::Applications::iconPath(_splash);
    if(path.isEmpty()){
        return _splash;
    }
    return path;
}

void Application::setSplash(QString splash){
    if(!hasPermission("permissions")){
        return;
    }
    setValue("splash", splash);
    emit splashChanged(splash);
}

QVariantMap Application::environment() { return value("environment", QVariantMap()).toMap(); }

void Application::setEnvironment(QVariantMap environment){
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

QString Application::workingDirectory() { return value("workingDirectory", "/").toString(); }

void Application::setWorkingDirectory(const QString& workingDirectory){
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

bool Application::chroot(){ return flags().contains("chroot"); }

QString Application::user(){ return value("user", getuid()).toString(); }

QString Application::group(){ return value("group", getgid()).toString(); }

QStringList Application::directories() { return value("directories", QStringList()).toStringList(); }

void Application::setDirectories(QStringList directories){
    if(!hasPermission("permissions")){
        return;
    }
    setValue("directories", directories);
    emit directoriesChanged(directories);
}

QByteArray Application::screenCapture(){
    if(!hasPermission("permissions")){
        return QByteArray();
    }
    return screenCaptureNoSecurityCheck();
}

QByteArray Application::screenCaptureNoSecurityCheck(){ return qUncompress(*m_screenCapture); }

const QVariantMap& Application::getConfig(){ return m_config; }
void Application::setConfig(const QVariantMap& config){
    auto oldBin = bin();
    m_config = config;
    if(type() == Foreground){
        setAutoStart(false);
    }
    if(oldBin == bin()){
        return;
    }
    if(!QFile::exists(bin())){
        setValue("bin", oldBin);
    }
}
void Application::started(){
    emit launched();
    emit appsAPI->applicationLaunched(qPath());
}
void Application::finished(int exitCode){
    qDebug() << "Application" << name() << "exit code" << exitCode;
    emit exited(exitCode);
    appsAPI->resumeIfNone();
    emit appsAPI->applicationExited(qPath(), exitCode);
    umountAll();
    if(transient()){
        unregister();
    }
}

void Application::readyReadStandardError(){
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

void Application::readyReadStandardOutput(){
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

void Application::stateChanged(QProcess::ProcessState state){
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
void Application::errorOccurred(QProcess::ProcessError error){
    switch(error){
        case QProcess::FailedToStart:
            qDebug() << "Application" << name() << "failed to start.";
            emit exited(-1);
            emit appsAPI->applicationExited(qPath(), -1);
            if(transient()){
                unregister();
            }
            break;
        case QProcess::Crashed:
            qDebug() << "Application" << name() << "crashed.";
            break;
        case QProcess::Timedout:
            qDebug() << "Application" << name() << "timed out.";
            break;
        case QProcess::WriteError:
            qDebug() << "Application" << name() << "unable to write to stdin.";
            break;
        case QProcess::ReadError:
            qDebug() << "Application" << name() << "unable to read from stdout or stderr.";
            break;
        case QProcess::UnknownError:
        default:
            qDebug() << "Application" << name() << "unknown error.";
    }
}
bool Application::hasPermission(QString permission, const char* sender){ return appsAPI->hasPermission(permission, sender); }

void Application::delayUpTo(int milliseconds){
    timer.invalidate();
    timer.start();
    while(timer.isValid() && !timer.hasExpired(milliseconds)){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
}

void Application::updateEnvironment(){
    auto env = QProcessEnvironment::systemEnvironment();
    auto envPath = env.value("PATH", DEFAULT_PATH).split(":");
    auto defaults = QString(DEFAULT_PATH).split(":");
    for(auto item : defaults){
        if(!envPath.contains(item)){
            envPath.append(item);
        }
    }
    env.insert("PATH", envPath.join(":"));
    auto preload = env.value("LD_PRELOAD", "").split(":");
    QString sysfs_preload("/opt/lib/libsysfs_preload.so");
    if(!preload.contains(sysfs_preload)){
        preload.append(sysfs_preload);
    }
    if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM2){
        QString rm2fb_client("/opt/lib/librm2fb_client.so");
        if(!preload.contains(rm2fb_client)){
            preload.append(rm2fb_client);
        }
    }
    env.insert("LD_PRELOAD", preload.join(":"));
    for(auto key : environment().keys()){
        env.insert(key, environment().value(key, "").toString());
    }
    m_process->setEnvironment(env.toStringList());
}

void Application::mkdirs(const QString& path, mode_t mode){
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

void Application::bind(const QString& source, const QString& target, bool readOnly){
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

void Application::sysfs(const QString& path){
    mkdirs(path, 744);
    umount(path);
    qDebug() << "sysfs" << path;
    if(mount("none", path.toStdString().c_str(), "sysfs", 0, "")){
        O_WARNING("Failed to mount sysfs: " << ::strerror(errno));
    }
}

void Application::ramdisk(const QString& path){
    mkdirs(path, 744);
    umount(path);
    qDebug() << "ramdisk" << path;
    if(mount("tmpfs", path.toStdString().c_str(), "tmpfs", 0, "size=249m,mode=755")){
        O_WARNING("Failed to create ramdisk: " << ::strerror(errno));
    }
}

void Application::umount(const QString& path){
    if(!isMounted(path)){
        return;
    }
    auto cpath = path.toStdString();
    auto ret = ::umount2(cpath.c_str(), MNT_DETACH);
    if((ret && ret != EINVAL && ret != ENOENT) || isMounted(path)){
        qDebug() << "umount failed" << path;
        return;
    }
    QDir dir(path);
    if(dir.exists()){
        rmdir(cpath.c_str());
    }
    qDebug() << "umount" << path;
}

FifoHandler* Application::mkfifo(const QString& name, const QString& target){
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

void Application::symlink(const QString& source, const QString& target){
    if(QFile::exists(source)){
        return;
    }
    qDebug() << "symlink" << source << target;
    if(::symlink(target.toStdString().c_str(), source.toStdString().c_str())){
        O_WARNING("Failed to create symlink: " << ::strerror(errno));
        return;
    }
}

const QString Application::resourcePath() { return "/tmp/tarnish-chroot/" + name(); }

const QString Application::chrootPath() { return resourcePath() + "/chroot"; }

void Application::mountAll(){
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

void Application::umountAll(){
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

bool Application::isMounted(const QString& path){ return getActiveMounts().contains(path); }

QStringList Application::getActiveApplicationMounts(){
    auto path = chrootPath() + "/";
    QStringList activeMounts = getActiveMounts().filter(QRegularExpression("^" + QRegularExpression::escape(path) + ".*"));
    activeMounts.sort(Qt::CaseSensitive);
    std::reverse(std::begin(activeMounts), std::end(activeMounts));
    return activeMounts;
}

QStringList Application::getActiveMounts(){
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
void Application::showSplashScreen(){
    auto frameBuffer = getFrameBuffer();
    qDebug() << "Waiting for other painting to finish...";
    Oxide::Sentry::sentry_transaction("application", "showSplashScreen", [this, frameBuffer](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
        if(t != nullptr){
            sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
        }
#else
            Q_UNUSED(t);
#endif
        Oxide::Sentry::sentry_span(t, "wait", "Wait for screen to be ready", [frameBuffer](){
            dispatchToMainThread([frameBuffer]{
                while(frameBuffer->paintingActive()){
                    // TODO - don't spinlock
                }
            });
        });
        qDebug() << "Displaying splashscreen for" << name();
        Oxide::Sentry::sentry_span(t, "paint", "Draw splash screen", [this, frameBuffer](){
            dispatchToMainThread([this, frameBuffer]{
                QPainter painter(frameBuffer);
                auto size = frameBuffer->size();
                auto rect = frameBuffer->rect();
                auto fm = painter.fontMetrics();
                painter.fillRect(rect, Qt::white);
                QString splashPath = splash();
                if(splashPath.isEmpty() || !QFile::exists(splashPath)){
                    splashPath = icon();
                }
                if(!splashPath.isEmpty() && QFile::exists(splashPath)){
                    qDebug() << "Using image" << splashPath;
                    int splashWidth = size.width() / 2;
                    QSize splashSize(splashWidth, splashWidth);
                    QImage splash = QImage(splashPath);
                    if(systemAPI->landscape()){
                        splash = splash.transformed(QTransform().rotate(90.0));
                    }
                    splash = splash.scaled(splashSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    QRect splashRect(
                        QPoint(
                            (size.width() / 2) - (splashWidth / 2),
                            (size.height() / 2) - (splashWidth / 2)
                            ),
                        splashSize
                        );
                    painter.drawImage(splashRect, splash, splash.rect());
                    Oxide::QML::repaint(getFrameBufferWindow(), frameBuffer->rect(), Blight::HighQualityGrayscale, true);
                }
                painter.end();
                notificationAPI->drawNotificationText("Loading " + displayName() + "...");
            });
        });
    });
    qDebug() << "Finished painting splash screen for" << name();
}
void Application::powerStateDataRecieved(FifoHandler* handler, const QString& data){
    Q_UNUSED(handler);
    if(!permissions().contains("power")){
        O_WARNING("Denied powerState request");
        return;
    }
    if((QStringList() << "mem" << "freeze" << "standby").contains(data)){
        systemAPI->suspend();
    }else{
        O_WARNING("Unknown power state call: " << data);
    }
}
void Application::startSpan(std::string operation, std::string description){
    if(!sharedSettings.applicationUsage()){
        return;
    }
    if(span != nullptr){
        Oxide::Sentry::stop_span(span);
        delete span;
    }
    if(transaction == nullptr){
        span = nullptr;
        return;
    }
    span = Oxide::Sentry::start_span(transaction, operation, description);
}

void Application::waitForFinished(){
    if(m_process->processId()){
        m_process->waitForFinished();
    }
}
Application::Application(QString path, QObject *parent)
: QObject(parent),
  m_path(path),
  m_backgrounded(false),
  fifos()
{
    m_process = new SandBoxProcess(this);
    connect(m_process, &SandBoxProcess::started, this, &Application::started);
    connect(
        m_process,
        QOverload<int, QProcess::ExitStatus>::of(&SandBoxProcess::finished),
        [this](int exitCode, QProcess::ExitStatus status) {
            Q_UNUSED(status);
            finished(exitCode);
        }
    );
    connect(
        m_process,
        &SandBoxProcess::readyReadStandardError,
        this,
        &Application::readyReadStandardError
    );
    connect(
        m_process,
        &SandBoxProcess::readyReadStandardOutput,
        this,
        &Application::readyReadStandardOutput
    );
    connect(
        m_process,
        &SandBoxProcess::stateChanged,
        this,
        &Application::stateChanged
    );
    connect(
        m_process,
        &SandBoxProcess::errorOccurred,
        this,
        &Application::errorOccurred
    );
}
Application::~Application() {
    stopNoSecurityCheck();
    unregisterPath();
    if (m_screenCapture != nullptr) {
        delete m_screenCapture;
    }
    umountAll();
    if (p_stdout != nullptr) {
        p_stdout->flush();
        delete p_stdout;
    }
    if (p_stdout_fd > 0) {
        close(p_stdout_fd);
        p_stdout_fd = -1;
    }
    if (p_stderr != nullptr) {
        p_stderr->flush();
        delete p_stderr;
    }
    if (p_stderr_fd > 0) {
        close(p_stderr_fd);
        p_stderr_fd = -1;
    }
}
void Application::registerPath() {
    auto bus = QDBusConnection::systemBus();
    bus.unregisterObject(path(), QDBusConnection::UnregisterTree);
    if (bus.registerObject(path(), this, QDBusConnection::ExportAllContents)) {
        qDebug() << "Registered" << path() << OXIDE_APPLICATION_INTERFACE;
    } else {
        qDebug() << "Failed to register" << path();
    }
}
void Application::unregisterPath() {
    auto bus = QDBusConnection::systemBus();
    if (bus.objectRegisteredAt(path()) != nullptr) {
        qDebug() << "Unregistered" << path();
        bus.unregisterObject(path());
    }
}
QDBusObjectPath Application::qPath() { return QDBusObjectPath(path()); }
QString Application::path() { return m_path; }
QString Application::name() { return value("name").toString(); }
int Application::processId() { return m_process->processId(); }
QStringList Application::permissions() {
    return value("permissions", QStringList()).toStringList();
}
void Application::setPermissions(QStringList permissions) {
    if (!hasPermission("permissions")) {
        return;
    }
    setValue("permissions", permissions);
    emit permissionsChanged(permissions);
}
QString Application::displayName() {
    return value("displayName", name()).toString();
}
void Application::setDisplayName(QString displayName) {
    if (!hasPermission("permissions")) {
        return;
    }
    setValue("displayName", displayName);
    emit displayNameChanged(displayName);
}
QString Application::description() {
    return value("description", displayName()).toString();
}

QString Application::bin() { return value("bin").toString(); }
QString Application::onPause() { return value("onPause", "").toString(); }
void Application::setOnPause(QString onPause) {
    if (!hasPermission("permissions")) {
        return;
    }
    setValue("onPause", onPause);
    emit onPauseChanged(onPause);
}
QString Application::onResume() { return value("onResume", "").toString(); }
void Application::setOnResume(QString onResume) {
    if (!hasPermission("permissions")) {
        return;
    }
    setValue("onResume", onResume);
    emit onResumeChanged(onResume);
}
QString Application::onStop() { return value("onStop", "").toString(); }
void Application::setOnStop(QString onStop) {
    if (!hasPermission("permissions")) {
        return;
    }
    setValue("onStop", onStop);
    emit onStopChanged(onStop);
}

QStringList Application::flags() { return value("flags", QStringList()).toStringList(); }

bool Application::autoStart() { return flags().contains("autoStart"); }

void Application::setAutoStart(bool autoStart) {
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

bool Application::systemApp() { return flags().contains("system"); }

bool Application::transient() { return flags().contains("transient"); }

bool Application::hidden() { return flags().contains("hidden"); }

int Application::type() { return (int)value("type", 0).toInt(); }

int Application::state(){
    if(!hasPermission("apps")){
        return Inactive;
    }
    return stateNoSecurityCheck();
}

bool SandBoxProcess::setUser(const QString& name){
    try{
        m_uid = Oxide::getUID(name);
        return true;
    }
    catch(const std::runtime_error&){
        return false;
    }
}

bool SandBoxProcess::setGroup(const QString& name){
    try{
        m_gid = Oxide::getGID(name);
        return true;
    }
    catch(const std::runtime_error&){
        return false;
    }
}

bool SandBoxProcess::setChroot(const QString& path){
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

void SandBoxProcess::setMask(mode_t mask){
    m_mask = mask;
}

void SandBoxProcess::setupChildProcess() {
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

#include "moc_application.cpp"
