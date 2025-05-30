#include <QTimer>
#include <QFile>
#include <QTransform>

#include <signal.h>
#include <liboxide.h>
#include <liboxide/oxideqml.h>
#include <libblight.h>
#include <libblight/connection.h>

#include "application.h"
#include "appsapi.h"
#include "systemapi.h"
#include "notificationapi.h"
#include "screenapi.h"
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
        Oxide::Sentry::sentry_transaction("Launch Application", "launch", [this](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
            if(t != nullptr){
                sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
            }
#else
            Q_UNUSED(t);
#endif
            appsAPI->recordPreviousApplication();
            O_INFO("Launching " << path());
            appsAPI->pauseAll();
            if(!flags().contains("noannounce")){
                m_notification = notificationAPI->add(
                    QUuid::createUuid().toString(),
                    "codes.eeems.tarnish",
                    "codes.eeems.tarnish",
                    QString("Loading %1...").arg(displayName()),
                    icon()
                );
                m_notification->display();
            }
            if(flags().contains("exclusive")){
                auto compositor = getCompositorDBus();
                compositor->enterExclusiveMode().waitForFinished();
                auto frameBuffer = getFrameBuffer();
                QPainter p(frameBuffer);
                p.fillRect(frameBuffer->rect(), Qt::white);
                p.end();
                compositor->exclusiveModeRepaint().waitForFinished();
            }
            if(m_process->program() != bin()){
                m_process->setProgram(bin());
            }
            updateEnvironment();
            m_process->setWorkingDirectory(workingDirectory());
            m_process->setGroup(group());
            if(p_stdout == nullptr){
                p_stdout_fd = sd_journal_stream_fd(name().toStdString().c_str(), LOG_INFO, 1);
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
                        O_DEBUG("Opened stdout for " << name());
                    }
                }
            }
            if(p_stderr == nullptr){
                p_stderr_fd = sd_journal_stream_fd(name().toStdString().c_str(), LOG_ERR, 1);
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
                        O_DEBUG("Opened stderr for " << name());
                    }
                }
            }
            m_process->start();
            m_process->waitForStarted();
            if(type() == Background){
                startSpan("background", "Application is in the background");
            }else{
                startSpan("foreground", "Application is in the foreground");
                if(flags().contains("exclusive")){
                    auto compositor = getCompositorDBus();
                    compositor->focus(id());
                }
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
    Oxide::Sentry::sentry_transaction("Pause Application", "pause", [this, startIfNone](Oxide::Sentry::Transaction* t){
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
    O_INFO("Paused " << path());
}
void Application::interruptApplication(){
    if(
        !m_process->processId()
        || stateNoSecurityCheck() == Paused
        || type() == Background
    ){
        return;
    }
    Oxide::Sentry::sentry_transaction("Interrupt Application", "interrupt", [this](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
        if(t != nullptr){
            sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
        }
#else
        Q_UNUSED(t);
#endif
        if(flags().contains("exclusive")){
            m_backup = new QImage(getFrameBuffer()->copy());
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
                    O_INFO("Waiting for SIGUSR2 ack");
                    appsAPI->connectSignals(this, 2);
                    kill(-m_process->processId(), SIGUSR2);
                    timer.restart();
                    delayUpTo(1000);
                    appsAPI->disconnectSignals(this, 2);
                    if(stateNoSecurityCheck() == Inactive){
                        O_INFO("Application crashed while pausing");
                    }else if(timer.isValid()){
                        O_INFO("Application took too long to background" << name());
                        kill(-m_process->processId(), SIGSTOP);
                        waitForPause();
                        startSpan("stopped", "Application is stopped");
                    }else{
                        m_backgrounded = true;
                        O_INFO("SIGUSR2 ack recieved");
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
        if(flags().contains("exclusive")){
            getCompositorDBus()->exitExclusiveMode().waitForFinished();
        }
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

QString Application::id(){ return QString("connection/%1").arg(m_process->processId()); }

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
        O_DEBUG("Can't Resume" << path() << "Already running!");
        return;
    }
    Oxide::Sentry::sentry_transaction("Resume Application", "resume", [this](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
        if(t != nullptr){
            sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
        }
#else
        Q_UNUSED(t);
#endif
        appsAPI->recordPreviousApplication();
        O_INFO("Resuming " << path());
        appsAPI->pauseAll();
        uninterruptApplication();
        waitForResume();
        emit resumed();
        emit appsAPI->applicationResumed(qPath());
    });
    O_INFO("Resumed " << path());
}
void Application::uninterruptApplication(){
    if(
        !m_process->processId()
        || stateNoSecurityCheck() == InForeground
        || (type() == Background && stateNoSecurityCheck() == InBackground)
    ){
        return;
    }
    Oxide::Sentry::sentry_transaction("Uninterrupt Application", "uninterrupt", [this](Oxide::Sentry::Transaction* t){
        if(flags().contains("exclusive")){
            auto compositor = getCompositorDBus();
            compositor->enterExclusiveMode().waitForFinished();
            auto frameBuffer = getFrameBuffer();
            QPainter p(frameBuffer);
            if(m_backup == nullptr){
                p.fillRect(frameBuffer->rect(), Qt::white);
            }else{
                p.drawImage(frameBuffer->rect(), *m_backup, m_backup->rect());
                delete m_backup;
                m_backup = nullptr;
            }
            p.end();
            compositor->exclusiveModeRepaint().waitForFinished();
        }
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
                    O_INFO("Waiting for SIGUSR1 ack");
                    appsAPI->connectSignals(this, 1);
                    kill(-m_process->processId(), SIGUSR1);
                    delayUpTo(1000);
                    appsAPI->disconnectSignals(this, 1);
                    if(timer.isValid()){
                        // No need to fall through, we've just assumed it continued
                        O_INFO("Warning: application took too long to forground" << name());
                    }else{
                        O_INFO("SIGUSR1 ack recieved");
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
    auto compositor = getCompositorDBus();
    compositor->raise(id());
    compositor->focus(id());
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
    Oxide::Sentry::sentry_transaction("Stop Application", "stop", [this, state](Oxide::Sentry::Transaction* t){
#ifdef SENTRY
        if(t != nullptr){
            sentry_transaction_set_tag(t->inner, "application", name().toStdString().c_str());
        }
#else
        Q_UNUSED(t);
#endif
        O_INFO("Stopping " << path());
        if(!onStop().isEmpty()){
            Oxide::Sentry::sentry_span(t, "onStop", "Run onStop action", [this](){
                O_INFO("onStop: " << onStop());
                O_INFO("exit code: " << QProcess::execute(onStop(), QStringList()));
            });
        }
        getCompositorDBus()->lower(QString("connection/%1").arg(m_process->processId()));
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

QVariantMap Application::environment() { return value("environment", QVariantMap()).toMap(); }

void Application::setEnvironment(QVariantMap environment){
    if(!hasPermission("permissions")){
        return;
    }
    for(auto key : environment.keys()){
        auto value = environment.value(key, QVariant());
        if(!value.isValid()){
            O_INFO(key << " has invalid value: " << value);
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
    if(m_notification != nullptr){
        m_notification->remove();
        m_notification = nullptr;
    }
    emit launched();
    emit appsAPI->applicationLaunched(qPath());
}
void Application::finished(int exitCode){
    O_INFO("Application" << name() << "exit code" << exitCode);
    getCompositorDBus()->lower(QString("connection/%1").arg(m_process->processId()));
    if(
        exitCode != EXIT_SUCCESS
        && exitCode != SIGINT
        && exitCode != SIGTERM
        && exitCode != SIGKILL
        && exitCode != 128 + EXIT_SUCCESS
        && exitCode != 128 + SIGINT
        && exitCode != 128 + SIGTERM
    ){
        notificationAPI->add(
            QUuid::createUuid().toString(),
            "codes.eeems.tarnish",
            "codes.eeems.tarnish",
            QString("%1 crashed (%2)").arg(displayName()).arg(exitCode),
            icon()
        )->display();
    }
    emit exited(exitCode);
    if(flags().contains("exclusive")){
        getCompositorDBus()->exitExclusiveMode().waitForFinished();
    }
    appsAPI->resumeIfNone();
    emit appsAPI->applicationExited(qPath(), exitCode);
    if(transient()){
        unregister();
    }
    if(m_backup != nullptr){
        delete m_backup;
        m_backup = nullptr;
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
            O_INFO("Application" << name() << "is starting.");
            break;
        case QProcess::Running:
            O_INFO("Application" << name() << "is running.");
            break;
        case QProcess::NotRunning:
            O_INFO("Application" << name() << "is not running.");
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
        case QProcess::FailedToStart:{
            O_INFO("Application" << name() << "failed to start.");
            emit exited(-1);
            emit appsAPI->applicationExited(qPath(), -1);
            notificationAPI->add(
                QUuid::createUuid().toString(),
                "codes.eeems.tarnish",
                "codes.eeems.tarnish",
                QString("%1 failed to start").arg(displayName()),
                icon()
            )->display();
            if(transient()){
                unregister();
            }
            break;
        }
        case QProcess::Crashed: // Let the finished slot handle this
            O_INFO("Application" << name() << "crashed.");
            break;
        case QProcess::Timedout:
            O_INFO("Application" << name() << "timed out.");
            break;
        case QProcess::WriteError:
            O_INFO("Application" << name() << "unable to write to stdin.");
            break;
        case QProcess::ReadError:
            O_INFO("Application" << name() << "unable to read from stdout or stderr.");
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
    auto envPath = env.value("PATH", DEFAULT_PATH).split(":");
    auto defaults = QString(DEFAULT_PATH).split(":");
    for(auto item : defaults){
        if(!envPath.contains(item)){
            envPath.append(item);
        }
    }
    env.insert("PATH", envPath.join(":"));
    auto preload = env.value("LD_PRELOAD", "").split(":");
    if(!flags().contains("nopreload") && !flags().contains("nopreload.sysfs")){
        QString sysfs_preload("/opt/lib/libsysfs_preload.so");
        if(!preload.contains(sysfs_preload)){
            preload.append(sysfs_preload);
        }
    }
    if(!flags().contains("nopreload") && !flags().contains("nopreload.compositor")){
        QString blight_client("/opt/lib/libblight_client.so");
        if(!preload.contains(blight_client)){
            preload.append(blight_client);
        }
        if(deviceSettings.getDeviceType() == Oxide::DeviceSettings::RM2){
            env.insert("RM2FB_DISABLE", "1");
        }
    }
    if(flags().contains("exclusive")){
        env.insert("OXIDE_PRELOAD_EXPOSE_FB", "1");
        env.insert("OXIDE_PRELOAD_ALLOW_RM2FB", "1");
    }
    env.insert("LD_PRELOAD", preload.join(":"));
    for(auto key : environment().keys()){
        env.insert(key, environment().value(key, "").toString());
    }
    env.remove("OXIDE_PRELOAD_DISABLE_INPUT");
    m_process->setEnvironment(env.toStringList());
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
  m_backgrounded(false)
{
    m_process = new ApplicationProcess(this);
    connect(m_process, &ApplicationProcess::started, this, &Application::started);
    connect(
        m_process,
        QOverload<int, QProcess::ExitStatus>::of(&ApplicationProcess::finished),
        [this](int exitCode, QProcess::ExitStatus status) {
            Q_UNUSED(status);
            finished(exitCode);
        }
    );
    connect(
        m_process,
        &ApplicationProcess::readyReadStandardError,
        this,
        &Application::readyReadStandardError
    );
    connect(
        m_process,
        &ApplicationProcess::readyReadStandardOutput,
        this,
        &Application::readyReadStandardOutput
    );
    connect(
        m_process,
        &ApplicationProcess::stateChanged,
        this,
        &Application::stateChanged
    );
    connect(
        m_process,
        &ApplicationProcess::errorOccurred,
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
        O_INFO("Registered" << path() << OXIDE_APPLICATION_INTERFACE);
    } else {
        O_WARNING("Failed to register" << path());
    }
}
void Application::unregisterPath() {
    auto bus = QDBusConnection::systemBus();
    if (bus.objectRegisteredAt(path()) != nullptr) {
        O_INFO("Unregistered" << path());
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

ApplicationProcess::ApplicationProcess(QObject* parent)
: QProcess(parent),
  m_gid(0),
  m_uid(0),
  m_mask(0)
{}

bool ApplicationProcess::setUser(const QString& name){
    try{
        m_uid = Oxide::getUID(name);
        return true;
    }
    catch(const std::runtime_error&){
        return false;
    }
}

bool ApplicationProcess::setGroup(const QString& name){
    try{
        m_gid = Oxide::getGID(name);
        return true;
    }
    catch(const std::runtime_error&){
        return false;
    }
}

void ApplicationProcess::setMask(mode_t mask){
    m_mask = mask;
}

void ApplicationProcess::setupChildProcess() {
    // Drop all privileges in the child process
    setgroups(0, 0);
    // Change to correct user
    setresgid(m_gid, m_gid, m_gid);
    setresuid(m_uid, m_uid, m_uid);
    umask(m_mask);
    setsid();
    prctl(PR_SET_PDEATHSIG, SIGTERM);
}

#include "moc_application.cpp"
