#include <QTimer>
#include <QFile>

#include <signal.h>

#include "application.h"
#include "appsapi.h"
#include "systemapi.h"
#include "buttonhandler.h"
#include "digitizerhandler.h"
#include "devicesettings.h"

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
        }else{
            m_process->setChroot("");
        }
        m_process->setWorkingDirectory(workingDirectory());
        m_process->setUser(user());
        m_process->setGroup(group());
        m_process->start();
        m_process->waitForStarted();
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
        || type() == AppsAPI::Background
    ){
        return;
    }
    qDebug() << "Pausing " << path();
    interruptApplication();
    if(!flags().contains("nosavescreen")){
        saveScreen();
    }
    if(startIfNone){
        appsAPI->resumeIfNone();
    }
    emit paused();
    emit appsAPI->applicationPaused(qPath());
    qDebug() << "Paused " << path();
}
void Application::interruptApplication(){
    if(
        !m_process->processId()
        || stateNoSecurityCheck() == Paused
        || type() == AppsAPI::Background
    ){
        return;
    }
    if(!onPause().isEmpty()){
        system(onPause().toStdString().c_str());
    }
    switch(type()){
        case AppsAPI::Background:
            // Already in the background. How did we get here?
            return;
        case AppsAPI::Backgroundable:
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
            }else{
                m_backgrounded = true;
                qDebug() << "SIGUSR2 ack recieved";
            }
            break;
        case AppsAPI::Foreground:
        default:
            kill(-m_process->processId(), SIGSTOP);
            waitForPause();
    }
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
        || (type() == AppsAPI::Background && stateNoSecurityCheck() == InBackground)
    ){
        qDebug() << "Can't Resume" << path() << "Already running!";
        return;
    }
    appsAPI->recordPreviousApplication();
    qDebug() << "Resuming " << path();
    appsAPI->pauseAll();
    if(!flags().contains("nosavescreen") && (type() != AppsAPI::Backgroundable || stateNoSecurityCheck() == Paused)){
        recallScreen();
    }
    uninterruptApplication();
    waitForResume();
    emit resumed();
    emit appsAPI->applicationResumed(qPath());
    qDebug() << "Resumed " << path();
}
void Application::uninterruptApplication(){
    if(
        !m_process->processId()
        || stateNoSecurityCheck() == InForeground
        || (type() == AppsAPI::Background && stateNoSecurityCheck() == InBackground)
    ){
        return;
    }
    if(!onResume().isEmpty()){
        system(onResume().toStdString().c_str());
    }
    switch(type()){
        case AppsAPI::Background:
        case AppsAPI::Backgroundable:
            if(stateNoSecurityCheck() == Paused){
                touchHandler->clear_buffer();
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
            break;
        case AppsAPI::Foreground:
        default:
            touchHandler->clear_buffer();
            kill(-m_process->processId(), SIGCONT);
    }
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
    qDebug() << "Stopping " << path();
    if(!onStop().isEmpty()){
        QProcess::execute(onStop());
    }
    Application* pausedApplication = nullptr;
    if(state == Paused){
        touchHandler->clear_buffer();
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
    }
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
}
void Application::signal(int signal){
    if(m_process->processId()){
        kill(-m_process->processId(), signal);
    }
}
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
            if(type() == AppsAPI::Background || (type() == AppsAPI::Backgroundable && m_backgrounded)){
                return InBackground;
            }
            return InForeground;
        }break;
        case QProcess::NotRunning:
        default:
            return Inactive;
    }
}
void Application::setConfig(const QVariantMap& config){
    auto oldBin = bin();
    m_config = config;
    if(type() == AppsAPI::Foreground){
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
}
void Application::errorOccurred(QProcess::ProcessError error){
    switch(error){
        case QProcess::FailedToStart:
            qDebug() << "Application" << name() << "failed to start.";
            emit exited(-1);
            emit appsAPI->applicationExited(qPath(), -1);
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
void Application::showSplashScreen(){
    auto frameBuffer = EPFrameBuffer::framebuffer();
    qDebug() << "Waiting for other painting to finish...";
    while(frameBuffer->paintingActive()){
        EPFrameBuffer::waitForLastUpdate();
    }
    qDebug() << "Displaying splashscreen for" << name();
    QPainter painter(frameBuffer);
    auto fm = painter.fontMetrics();
    auto size = frameBuffer->size();
    painter.fillRect(frameBuffer->rect(), Qt::white);
    QString splashPath = splash();
    if(splashPath.isEmpty() || !QFile::exists(splashPath)){
        splashPath = icon();
    }
    if(!splashPath.isEmpty() && QFile::exists(splashPath)){
        qDebug() << "Using image" << splashPath;
        int splashWidth = size.width() / 2;
        QSize splashSize(splashWidth, splashWidth);
        QImage splash = QImage(splashPath).scaled(splashSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QRect splashRect(
            QPoint(
                (size.width() / 2) - (splashWidth / 2),
                (size.height() / 2) - (splashWidth / 2)
            ),
            splashSize
        );
        painter.drawImage(splashRect, splash, splash.rect());
        EPFrameBuffer::sendUpdate(frameBuffer->rect(), EPFrameBuffer::HighQualityGrayscale, EPFrameBuffer::FullUpdate, true);
    }
    painter.setPen(Qt::black);
    auto text = "Loading " + displayName() + "...";
    int padding = 10;
    int textHeight = fm.height() + padding;
    QRect textRect(
        QPoint(0 + padding, size.height() - textHeight),
        QSize(size.width() - padding * 2, textHeight)
    );
    painter.drawText(
        textRect,
        Qt::AlignVCenter | Qt::AlignRight,
        text
    );
    EPFrameBuffer::sendUpdate(textRect, EPFrameBuffer::Grayscale, EPFrameBuffer::PartialUpdate, true);
    painter.end();
    qDebug() << "Waitng for screen to finish...";
    EPFrameBuffer::waitForLastUpdate();
    qDebug() << "Finished paining splash screen for" << name();
}
void Application::powerStateDataRecieved(FifoHandler* handler, const QString& data){
    Q_UNUSED(handler);
    if(!permissions().contains("power")){
        qWarning() << "Denied powerState request";
        return;
    }
    if((QStringList() << "mem" << "freeze" << "standby").contains(data)){
        systemAPI->suspend();
    }else{
        qWarning() << "Unknown power state call: " << data;
    }
}
