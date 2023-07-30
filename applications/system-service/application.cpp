#include <QTimer>
#include <QFile>
#include <QDBusConnection>
#include <QProcessEnvironment>
#include <QDir>
#include <QCoreApplication>

#include <signal.h>
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

#include "application.h"
#include "mxcfb.h"
#include "guiapi.h"
#include "screenapi.h"
#include "notificationapi.h"
#include "appsapi.h"
#include "systemapi.h"
#include "guiapi.h"
#include "digitizerhandler.h"
#include "window.h"

#define DEFAULT_PATH "/opt/bin:/opt/sbin:/opt/usr/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin"

using namespace Oxide::Applications;

const event_device touchScreen(deviceSettings.getTouchDevicePath(), O_WRONLY);

Application::Application(QDBusObjectPath path, QObject* parent) : Application(path.path(), parent) {}

Application::Application(QString path, QObject* parent) : QObject(parent), m_path(path), m_backgrounded(false), fifos(), m_pid{-1} {
    m_process = new Process(this);
    connect(m_process, &Process::started, this, &Application::started);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&Process::finished), [this](int exitCode, QProcess::ExitStatus status){
        Q_UNUSED(status);
        finished(exitCode);
    });
    connect(m_process, &Process::readyReadStandardError, this, &Application::readyReadStandardError);
    connect(m_process, &Process::readyReadStandardOutput, this, &Application::readyReadStandardOutput);
    connect(m_process, &Process::stateChanged, this, &Application::stateChanged);
    connect(m_process, &Process::errorOccurred, this, &Application::errorOccurred);
}

Application::~Application() {
    stopNoSecurityCheck();
    unregisterPath();
    if(m_screenCapture != nullptr){
        delete m_screenCapture;
    }
    if(p_stdout != nullptr){
        p_stdout->flush();
        delete p_stdout;
    }
    if(p_stdout_fd > 0){
        close(p_stdout_fd);
        p_stdout_fd = -1;
    }
    if(p_stderr != nullptr){
        p_stderr->flush();
        delete p_stderr;
    }
    if(p_stderr_fd > 0){
        close(p_stderr_fd);
        p_stderr_fd = -1;
    }
}

void Application::registerPath(){
    auto bus = QDBusConnection::systemBus();
    bus.unregisterObject(path(), QDBusConnection::UnregisterTree);
    if(bus.registerObject(path(), this, QDBusConnection::ExportAllContents)){
        O_DEBUG("Registered" << path() << OXIDE_APPLICATION_INTERFACE);
    }else{
        O_WARNING("Failed to register" << path());
    }
}

void Application::unregisterPath(){
    auto bus = QDBusConnection::systemBus();
    if(bus.objectRegisteredAt(path()) != nullptr){
        O_DEBUG("Unregistered" << path());
        bus.unregisterObject(path());
    }
}

void Application::launch(){
    if(!hasPermission("apps")){
        return;
    }
    launchNoSecurityCheck();
}

void Application::launchNoSecurityCheck(){
    if(appsAPI->stopping()){
        return;
    }
    if(m_process->processId()){
        resumeNoSecurityCheck();
    }else{
        auto name = this->name().toStdString();
        if(sharedSettings.applicationUsage()){
            transaction = Oxide::Sentry::start_transaction("application", "run");
            Oxide::Sentry::set_tag(transaction, "application", name);
            startSpan("starting", "Application is starting");
        }
        Oxide::Sentry::sentry_transaction("application", "launch", [this, name](Oxide::Sentry::Transaction* t){
            Oxide::Sentry::set_tag(t, "application", name);
            appsAPI->recordPreviousApplication();
            O_INFO("Launching " << path().toStdString().c_str());
            appsAPI->pauseAll();
            if(flags().contains("nosplash")){
                notificationAPI->paintNotification("Loading " + displayName() + "...", icon());
            }else{
                showSplashScreen();
            }
            if(m_process->program() != bin()){
                m_process->setProgram(bin());
            }
            updateEnvironment();
            m_process->setWorkingDirectory(workingDirectory());
            m_process->setUser(user());
            m_process->setGroup(group());
            if(flags().contains("nolog")){
                m_process->closeReadChannel(QProcess::StandardError);
                m_process->closeReadChannel(QProcess::StandardOutput);
            }else{
                if(p_stdout == nullptr){
                    p_stdout_fd = sd_journal_stream_fd(name.c_str(), LOG_INFO, 1);
                    if (p_stdout_fd < 0) {
                        errno = -p_stdout_fd;
                        O_WARNING("Failed to create stdout fd:" << -p_stdout_fd);
                    }else{
                        FILE* log = fdopen(p_stdout_fd, "w");
                        if(!log){
                            O_WARNING("Failed to create stdout FILE:" << errno);
                            close(p_stdout_fd);
                        }else{
                            p_stdout = new QTextStream(log);
                            O_DEBUG("Opened stdout for " << name.c_str());
                        }
                    }
                }
                if(p_stderr == nullptr){
                    p_stderr_fd = sd_journal_stream_fd(name.c_str(), LOG_ERR, 1);
                    if (p_stderr_fd < 0) {
                        errno = -p_stderr_fd;
                        O_WARNING("Failed to create sterr fd:" << -p_stderr_fd);
                    }else{
                        FILE* log = fdopen(p_stderr_fd, "w");
                        if(!log){
                            O_WARNING("Failed to create stderr FILE:" << errno);
                            close(p_stderr_fd);
                        }else{
                            p_stderr = new QTextStream(log);
                            O_DEBUG("Opened stderr for " << name.c_str());
                        }
                    }
                }
            }
            m_process->start();
            m_process->waitForStarted();
            if(flags().contains("nosplash")){
                NotificationAPI::_window()->_setVisible(false, false);
            }else{
                AppsAPI::_window()->_setVisible(false, false);
            }
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
    O_INFO("Pausing " << path());
    Oxide::Sentry::sentry_transaction("application", "pause", [this, startIfNone](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::set_tag(t, "application", name().toStdString());
        interruptApplication();
        if(startIfNone){
            appsAPI->resumeIfNone();
        }
        emit paused();
        emit appsAPI->applicationPaused(qPath());
    });
    O_DEBUG("Paused " << path());
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
        Oxide::Sentry::set_tag(t, "application", name().toStdString());
        if(environment().contains("OXIDE_PRELOAD_EXPOSE_FB")){
            saveScreen();
        }
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
                    O_DEBUG("Waiting for SIGUSR2 ack");
                    appsAPI->connectSignals(this, 2);
                    kill(-m_process->processId(), SIGUSR2);
                    timer.restart();
                    delayUpTo(1000);
                    appsAPI->disconnectSignals(this, 2);
                    if(stateNoSecurityCheck() == Inactive){
                        O_WARNING("Application crashed while pausing");
                    }else if(timer.isValid()){
                        O_WARNING("Application took too long to background" << name());
                        kill(-m_process->processId(), SIGSTOP);
                        waitForPause();
                        startSpan("stopped", "Application is stopped");
                    }else{
                        m_backgrounded = true;
                        O_DEBUG("SIGUSR2 ack recieved");
                        startSpan("background", "Application is in the background");
                    }
                    guiAPI->lowerWindows(m_pid);
                    break;
                case Foreground:
                default:
                    kill(-m_process->processId(), SIGSTOP);
                    waitForPause();
                    guiAPI->lowerWindows(m_pid);
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
        O_WARNING("Can't Resume" << path() << "Already running!");
        return;
    }
    Oxide::Sentry::sentry_transaction("application", "resume", [this](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::set_tag(t, "application", name().toStdString());
        appsAPI->recordPreviousApplication();
        O_INFO("Resuming " << path());
        appsAPI->pauseAll();
        uninterruptApplication();
        waitForResume();
        emit resumed();
        emit appsAPI->applicationResumed(qPath());
    });
    O_DEBUG("Resumed " << path());
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
        Oxide::Sentry::set_tag(t, "application", name().toStdString());
        if(environment().contains("OXIDE_PRELOAD_EXPOSE_FB")){
            recallScreen();
        }
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
                    O_DEBUG("Waiting for SIGUSR1 ack");
                    appsAPI->connectSignals(this, 1);
                    kill(-m_process->processId(), SIGUSR1);
                    delayUpTo(1000);
                    appsAPI->disconnectSignals(this, 1);
                    if(timer.isValid()){
                        // No need to fall through, we've just assumed it continued
                        O_WARNING("Warning: application took too long to forground" << name());
                    }else{
                        O_DEBUG("SIGUSR1 ack recieved");
                    }
                    m_backgrounded = false;
                    guiAPI->raiseWindows(m_pid);
                    startSpan("background", "Application is in the background");
                    break;
                case Foreground:
                default:
                    kill(-m_process->processId(), SIGCONT);
                    guiAPI->raiseWindows(m_pid);
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
        Oxide::Sentry::set_tag(t, "application", name().toStdString());
        O_INFO("Stopping " << path());
        if(!onStop().isEmpty()){
            Oxide::Sentry::sentry_span(t, "onStop", "Run onStop action", [this](){
                O_DEBUG("onStop: " << onStop());
                O_DEBUG("exit code: " << QProcess::execute(onStop(), QStringList()));
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

QString Application::name() { return value("name", path()).toString(); }

int Application::processId() { return m_process->processId(); }

QStringList Application::permissions() { return value("permissions", QStringList()).toStringList(); }

void Application::setPermissions(QStringList permissions){
    if(!hasPermission("permissions")){
        return;
    }
    setValue("permissions", permissions);
    emit permissionsChanged(permissions);
}

QString Application::displayName() { return value("displayName", name()).toString(); }

void Application::setDisplayName(QString displayName){
    if(!hasPermission("permissions")){
        return;
    }
    setValue("displayName", displayName);
    emit displayNameChanged(displayName);
}

QString Application::description() { return value("description", displayName()).toString(); }

QString Application::bin() { return value("bin").toString(); }

QString Application::onPause() { return value("onPause", "").toString(); }

void Application::setOnPause(QString onPause){
    if(!hasPermission("permissions")){
        return;
    }
    setValue("onPause", onPause);
    emit onPauseChanged(onPause);
}

QString Application::onResume() { return value("onResume", "").toString(); }

void Application::setOnResume(QString onResume){
    if(!hasPermission("permissions")){
        return;
    }
    setValue("onResume", onResume);
    emit onResumeChanged(onResume);
}

QString Application::onStop() { return value("onStop", "").toString(); }

void Application::setOnStop(QString onStop){
    if(!hasPermission("permissions")){
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
            O_WARNING(key << " has invalid value: " << value);
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

QString Application::user(){ return value("user", getuid()).toString(); }

QString Application::group(){ return value("group", getgid()).toString(); }

QByteArray Application::screenCapture(){
    if(!hasPermission("permissions")){
        return QByteArray();
    }
    return screenCaptureNoSecurityCheck();
}

QByteArray Application::screenCaptureNoSecurityCheck(){
    if(m_screenCapture == nullptr){
        return QByteArray();
    }
    return qUncompress(*m_screenCapture);
}

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

void Application::waitForFinished(){
    if(m_process->processId()){
        m_process->waitForFinished();
    }
}

void Application::started(){
    m_pid = processId();
    emit launched();
    emit appsAPI->applicationLaunched(qPath());
}

void Application::finished(int exitCode){
    O_DEBUG("Application" << name() << "exit code" << exitCode);
    emit exited(exitCode);
    appsAPI->resumeIfNone();
    emit appsAPI->applicationExited(qPath(), exitCode);
    if(transient()){
        unregister();
    }
    if(m_pid != -1){
        guiAPI->closeWindows(m_pid);
    }
    m_pid = -1;
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
            O_DEBUG("Application" << name() << "is starting.");
            break;
        case QProcess::Running:
            O_DEBUG("Application" << name() << "is running.");
            break;
        case QProcess::NotRunning:
            O_DEBUG("Application" << name() << "is not running.");
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
            O_WARNING("Application" << name() << "unknown state" << state);
    }
}

void Application::errorOccurred(QProcess::ProcessError error){
    switch(error){
        case QProcess::FailedToStart:
            O_WARNING("Application" << name() << "failed to start.");
            emit exited(-1);
            emit appsAPI->applicationExited(qPath(), -1);
            if(transient()){
                unregister();
            }
            break;
        case QProcess::Crashed:
            O_WARNING("Application" << name() << "crashed.");
            break;
        case QProcess::Timedout:
            O_WARNING("Application" << name() << "timed out.");
            break;
        case QProcess::WriteError:
            O_WARNING("Application" << name() << "unable to write to stdin.");
            break;
        case QProcess::ReadError:
            O_WARNING("Application" << name() << "unable to read from stdout or stderr.");
            break;
        case QProcess::UnknownError:
        default:
            O_WARNING("Application" << name() << "unknown error.");
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
    if(!flags().contains("nopreload")){
        if(flags().contains("preload.expose.fb")){
            env.insert("OXIDE_PRELOAD_EXPOSE_FB", "1");
        }
        if(flags().contains("preload.expose.input")){
            env.insert("OXIDE_PRELOAD_EXPOSE_INPUT", "1");
        }
        // TODO - detect if spaces are the seperator instead
        // TODO - strip out rm2fb
        auto preload = environment().value("LD_PRELOAD", "").toString().split(':');
        if(flags().contains("preload.qt")){
            env.insert("OXIDE_PRELOAD_NO_QAPP", "1");
            preload.prepend("/opt/lib/liboxide_qt_preload.so");
            if(flags().contains("preload.qt.tablet.nosynthesize")){
                env.insert("OXIDE_QPA_DISBLE_TABLET_SYNTHESIZE", "1");
            }
            if(flags().contains("preload.qt.qsgepaper")){
                env.insert("OXIDE_PRELOAD_FORCE_QSGEPAPER", "1");
            }
        }
        preload.prepend("/opt/lib/liboxide_preload.so");
        env.insert("LD_PRELOAD", preload.join(':'));
    }
    m_process->setEnvironment(env.toStringList());
}

void Application::showSplashScreen(){
    Oxide::Sentry::sentry_transaction("application", "showSplashScreen", [this](Oxide::Sentry::Transaction* t){
        Oxide::Sentry::set_tag(t, "application", name().toStdString());
        O_DEBUG("Displaying splashscreen for" << name());
        Oxide::Sentry::sentry_span(t, "paint", "Draw splash screen", [this](){
            auto image = AppsAPI::_window()->toImage();
            QPainter painter(&image);
            auto fm = painter.fontMetrics();
            auto geometry = AppsAPI::_window()->_geometry();
            painter.fillRect(geometry, Qt::white);
            QString splashPath = splash();
            if(splashPath.isEmpty() || !QFile::exists(splashPath)){
                splashPath = icon();
            }
            if(!splashPath.isEmpty() && QFile::exists(splashPath)){
                O_DEBUG("Using image" << splashPath);
                int splashWidth = geometry.width() / 2;
                QSize splashSize(splashWidth, splashWidth);
                QImage splash = QImage(splashPath).scaled(splashSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QRect splashRect(
                    QPoint(
                        (geometry.width() / 2) - (splashWidth / 2),
                        (geometry.height() / 2) - (splashWidth / 2)
                    ),
                    splashSize
                );
                painter.drawImage(splashRect, splash, splash.rect());
            }
            painter.setPen(Qt::black);
            auto text = "Loading " + displayName() + "...";
            int padding = 10;
            int textHeight = fm.height() + padding;
            QRect textRect(
                QPoint(0 + padding, geometry.height() - textHeight),
                QSize(geometry.width() - padding * 2, textHeight)
            );
            painter.drawText(
                textRect,
                Qt::AlignVCenter | Qt::AlignRight,
                text
            );
            painter.end();
        });
        if(!AppsAPI::_window()->_isVisible()){
            AppsAPI::_window()->_setVisible(true, false);
        }else{
            AppsAPI::_window()->_repaint(AppsAPI::_window()->_geometry(), EPFrameBuffer::HighQualityGrayscale, 0, false);
        }
    });
    O_DEBUG("Finished painting splash screen for" << name());
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

void Application::saveScreen(){
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

void Application::recallScreen(){
    if(m_screenCapture == nullptr){
        return;
    }
    Oxide::Sentry::sentry_transaction("application", "recallScreen", [this](Oxide::Sentry::Transaction* t){
        qDebug() << "Uncompressing screen...";
        QImage img;
        Oxide::Sentry::sentry_span(t, "decompress", "Decompress the framebuffer", [this, &img]{
            img = QImage::fromData(qUncompress(*m_screenCapture), "JPG");
        });
        if(img.isNull()){
            qDebug() << "Screen capture was corrupt";
            qDebug() << m_screenCapture->size();
            delete m_screenCapture;
            return;
        }
        qDebug() << "Recalling screen...";
        Oxide::Sentry::sentry_span(t, "recall", "Recall the screen", [this, img]{
            auto rect = AppsAPI::_window()->geometry();
            auto image = AppsAPI::_window()->toImage();
            QPainter painter(&image);
            painter.drawImage(rect, img);
            painter.end();
            if(AppsAPI::_window()->_isVisible()){
                AppsAPI::_window()->_repaint(rect, EPFrameBuffer::HighQualityGrayscale, 0, false);
            }else{
                AppsAPI::_window()->_setVisible(true, false);
            }
            delete m_screenCapture;
            m_screenCapture = nullptr;
        });
        qDebug() << "Screen recalled.";
    });
}

Process::Process(QObject* parent) : QProcess(parent), m_gid(0), m_uid(0), m_mask(0) {}

bool Process::setUser(const QString& name){
    try{
        m_uid = Oxide::getUID(name);
        return true;
    }
    catch(const std::runtime_error&){
        return false;
    }
}

bool Process::setGroup(const QString& name){
    try{
        m_gid = Oxide::getGID(name);
        return true;
    }
    catch(const std::runtime_error&){
        return false;
    }
}

void Process::setMask(mode_t mask){
    m_mask = mask;
}

void Process::setupChildProcess() {
    // Drop all privileges in the child process
    setgroups(0, 0);
    // Change to correct user
    setresgid(m_gid, m_gid, m_gid);
    setresuid(m_uid, m_uid, m_uid);
    umask(m_mask);
    setsid();
    prctl(PR_SET_PDEATHSIG, SIGTERM);
}
