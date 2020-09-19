#include <signal.h>
#include <QTimer>
#include <QFile>

#include "application.h"
#include "appsapi.h"

struct event_device {
    string device;
    int fd;
    event_device(string path, int flags){
        device = path;
        fd = open(path.c_str(), flags);
    }
};
const event_device touchScreen("/dev/input/event1", O_WRONLY);

void Application::launch(){
    if(m_process->processId()){
        resume();
    }else{
        qDebug() << "Launching " << path();
        auto api = (AppsAPI*)parent();
        api->pauseAll();
        m_process->start();
        m_process->waitForStarted();
    }
}

void Application::pause(bool startIfNone){
    if(m_process->processId() && state() != Paused && state() != InBackground){
        qDebug() << "Pausing " << path();
        if(!m_onpause.isEmpty()){
            QProcess::execute(m_onpause);
        }
        switch(m_type){
            case AppsAPI::Background:
            case AppsAPI::Backgroundable:
                kill(m_process->processId(), SIGUSR2);
                // TODO give 1 second for process to ack back the signal, otherwise fall through
                m_backgrounded = true;
                break;
            case AppsAPI::Foreground:
            default:
                kill(m_process->processId(), SIGSTOP);
        }
        saveScreen();
        auto api = (AppsAPI*)parent();
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
        auto api = (AppsAPI*)parent();
        api->pauseAll();
        recallScreen();
        if(!m_onresume.isEmpty()){
            QProcess::execute(m_onresume);
        }
        switch(m_type){
            case AppsAPI::Background:
            case AppsAPI::Backgroundable:
                kill(m_process->processId(), SIGUSR1);
                // TODO give 1 second for process to ack back the signal, otherwise fall through
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
    if(!m_onstop.isEmpty()){
        QProcess::execute(m_onstop);
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
    auto api = (AppsAPI*)parent();
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
            if(m_type == AppsAPI::Backgroundable && m_backgrounded){
                return InBackground;
            }
            return InForeground;
        }break;
        case QProcess::NotRunning:
        default:
            return Inactive;
    }
}

void Application::load(
    QString name, QString displayname, QString description,
    QString bin, int type, bool autostart, bool systemApp,
    QString icon, QString onPause, QString onResume, QString onStop
){
    m_name = name;
    m_displayname = displayname;
    m_description = description;
    m_bin = bin;
    m_onpause = onPause;
    m_onresume = onResume;
    m_onstop = onStop;
    m_type = (int)type;
    if(type == AppsAPI::Foreground){
        m_autoStart = false;
    }else{
        m_autoStart = autostart;
    }
    m_systemApp = systemApp;
    m_icon = icon;
    m_process->setProgram(bin);
}
void Application::started(){
    emit launched();
    auto api = (AppsAPI*)parent();
    emit api->applicationLaunched(qPath());
}
void Application::finished(int exitCode){
    qDebug() << "Application" << name() << "exit code" << exitCode;
    emit exited(exitCode);
    auto api = (AppsAPI*)parent();
    api->resumeIfNone();
    emit api->applicationExited(qPath(), exitCode);
}
void Application::errorOccurred(QProcess::ProcessError error){
    auto api = (AppsAPI*)parent();
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
