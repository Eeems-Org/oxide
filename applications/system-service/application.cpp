#include <signal.h>
#include <QTimer>
#include <QFile>

#include "application.h"
#include "appsapi.h"

const event_device touchScreen("/dev/input/event1", O_WRONLY);

void Application::launch(){
    if(m_process->processId()){
        resume();
    }else{
        qDebug() << "Launching " << path();
        auto api = reinterpret_cast<AppsAPI*>(parent());
        api->pauseAll();
        m_process->start();
        m_process->waitForStarted();
    }
}

void Application::pause(bool startIfNone){
    if(m_process->processId() && state() != Paused && state() != InBackground){
        qDebug() << "Pausing " << path();
        if(!onPause().isEmpty()){
            system(onPause().toStdString().c_str());
        }
        auto api = reinterpret_cast<AppsAPI*>(parent());
        switch(type()){
            case AppsAPI::Background:
            case AppsAPI::Backgroundable:
                api->connectSignals(this, 2);
                kill(m_process->processId(), SIGUSR2);
                timer.restart();
                delayUpTo(1000);
                api->disconnectSignals(this, 2);
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
        saveScreen();
        if(startIfNone){
            api->resumeIfNone();
        }
        emit paused();
        emit api->applicationPaused(qPath());
    }
}
void Application::resume(){
    if(m_process->processId() && state() != InForeground){
        qDebug() << "Resuming " << path();
        auto api = reinterpret_cast<AppsAPI*>(parent());
        api->pauseAll();
        if(!onResume().isEmpty()){
            system(onResume().toStdString().c_str());
        }
        recallScreen();
        switch(type()){
            case AppsAPI::Background:
            case AppsAPI::Backgroundable:
                if(state() == Paused){
                    inputManager()->clear_touch_buffer(touchScreen.fd);
                    kill(m_process->processId(), SIGCONT);
                }
                api->connectSignals(this, 1);
                kill(m_process->processId(), SIGUSR1);
                delayUpTo(1000);
                api->disconnectSignals(this, 1);
                if(timer.isValid()){
                    // No need to fall through, we've just assumed it continued
                    qDebug() << "Warning: application took too long to forground" << name();
                }
                m_backgrounded = false;
                break;
            case AppsAPI::Foreground:
            default:
                inputManager()->clear_touch_buffer(touchScreen.fd);
                kill(m_process->processId(), SIGCONT);
        }
        emit resumed();
        emit api->applicationResumed(qPath());
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
    auto api = reinterpret_cast<AppsAPI*>(parent());
    api->unregisterApplication(this);
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
            if(type() == AppsAPI::Backgroundable && m_backgrounded){
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
        setValue("config", false);
    }
    m_process->setProgram(bin());
}
void Application::started(){
    emit launched();
    auto api = reinterpret_cast<AppsAPI*>(parent());
    emit api->applicationLaunched(qPath());
}
void Application::finished(int exitCode){
    qDebug() << "Application" << name() << "exit code" << exitCode;
    emit exited(exitCode);
    auto api = reinterpret_cast<AppsAPI*>(parent());
    api->resumeIfNone();
    emit api->applicationExited(qPath(), exitCode);
}
void Application::errorOccurred(QProcess::ProcessError error){
    auto api = reinterpret_cast<AppsAPI*>(parent());
    switch(error){
        case QProcess::FailedToStart:
            qDebug() << "Application" << name() << "failed to start.";
            emit exited(-1);
            emit api->applicationExited(qPath(), -1);
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
