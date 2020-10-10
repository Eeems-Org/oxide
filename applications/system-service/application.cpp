#include <signal.h>
#include <QTimer>
#include <QFile>

#include "application.h"
#include "appsapi.h"
#include "buttonhandler.h"

const event_device touchScreen("/dev/input/event1", O_WRONLY);

void Application::launch(){
    if(m_process->processId()){
        resume();
    }else{
        qDebug() << "Launching " << path();
        appsAPI->pauseAll();
        m_process->start();
        m_process->waitForStarted();
    }
}

void Application::pause(bool startIfNone){
    if(
        !m_process->processId()
        || state() == Paused
        || type() == AppsAPI::Background
    ){
        return;
    }
    qDebug() << "Pausing " << path();
    interruptApplication();
    saveScreen();
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
        || state() == Paused
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
            appsAPI->connectSignals(this, 2);
            kill(m_process->processId(), SIGUSR2);
            timer.restart();
            delayUpTo(1000);
            appsAPI->disconnectSignals(this, 2);
            if(timer.isValid()){
                qDebug() << "Application took too long to background" << name();
                kill(m_process->processId(), SIGSTOP);
            }else{
                m_backgrounded = true;
            }
            break;
        case AppsAPI::Foreground:
        default:
            kill(m_process->processId(), SIGSTOP);
    }
}
void Application::resume(){
    if(
        !m_process->processId()
        || state() == InForeground
        || (type() == AppsAPI::Background && state() == InBackground)
    ){
        return;
    }
    qDebug() << "Resuming " << path();
    appsAPI->pauseAll();
    recallScreen();
    uninterruptApplication();
    emit resumed();
    emit appsAPI->applicationResumed(qPath());
    qDebug() << "Resumed " << path();
}
void Application::uninterruptApplication(){
    if(
        !m_process->processId()
        || state() == InForeground
        || (type() == AppsAPI::Background && state() == InBackground)
    ){
        return;
    }
    if(!onResume().isEmpty()){
        system(onResume().toStdString().c_str());
    }
    switch(type()){
        case AppsAPI::Background:
        case AppsAPI::Backgroundable:
            if(state() == Paused){
                inputManager->clear_touch_buffer(touchScreen.fd);
                kill(m_process->processId(), SIGCONT);
            }
            appsAPI->connectSignals(this, 1);
            kill(m_process->processId(), SIGUSR1);
            delayUpTo(1000);
            appsAPI->disconnectSignals(this, 1);
            if(timer.isValid()){
                // No need to fall through, we've just assumed it continued
                qDebug() << "Warning: application took too long to forground" << name();
            }
            m_backgrounded = false;
            break;
        case AppsAPI::Foreground:
        default:
            inputManager->clear_touch_buffer(touchScreen.fd);
            kill(m_process->processId(), SIGCONT);
    }
}
void Application::stop(){
    auto state = this->state();
    if(state == Inactive){
        return;
    }
    if(!onStop().isEmpty()){
        QProcess::execute(onStop());
    }
    if(state == Paused){
        kill(m_process->processId(), SIGCONT);
    }
    m_process->kill();
}
void Application::signal(int signal){
    if(m_process->processId()){
        kill(m_process->processId(), signal);
    }
}
void Application::unregister(){
    emit unregistered();
    appsAPI->unregisterApplication(this);
}

int Application::state(){
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
    m_config = config;
    if(type() == AppsAPI::Foreground){
        setAutoStart(false);
    }
    m_process->setProgram(bin());
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
