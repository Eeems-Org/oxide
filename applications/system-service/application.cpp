#include <signal.h>
#include <QTimer>

#include "application.h"
#include "appsapi.h"

void Application::launch(){
    if(m_process->processId()){
        resume();
    }else{
        auto api = (AppsAPI*)parent();
        api->pauseAll();
        m_process->start();
        m_process->waitForStarted();
    }
}
void Application::pause(){
    if(m_process->processId()){
        switch(m_type){
            case AppsAPI::Background:
            case AppsAPI::Backgroundable:
                kill(m_process->processId(), SIGUSR2);
                // TODO give 1 second for process to ack back the signal, otherwise fall through
                break;
            case AppsAPI::Foreground:
            default:
                kill(m_process->processId(), SIGSTOP);
        }
        auto api = (AppsAPI*)parent();
        api->resumeIfNone();
        emit paused();
        emit api->applicationPaused(qPath());
    }
}
void Application::resume(){
    if(m_process->processId()){
        auto api = (AppsAPI*)parent();
        api->pauseAll();
        switch(m_type){
            case AppsAPI::Background:
            case AppsAPI::Backgroundable:
                kill(m_process->processId(), SIGUSR1);
                // TODO give 1 second for process to ack back the signal, otherwise fall through
                break;
            case AppsAPI::Foreground:
            default:
                kill(m_process->processId(), SIGCONT);
        }
        emit resumed();
        emit api->applicationResumed(qPath());
    }
}
void Application::signal(int signal){
    if(m_process->processId()){
        emit signaled(signal);
        kill(m_process->processId(), signal);
    }
}
void Application::unregister(){
    emit unregistered();
    auto api = (AppsAPI*)parent();
    api->unregisterApplication(this);
}

void Application::load(
    QString name, QString description, QString call, QString term,
    int type, bool autostart
){
    m_name = name;
    m_description = description;
    m_call = call;
    m_term = term;
    m_type = (int)type;
    if(type == AppsAPI::Foreground){
        m_autoStart = false;
    }else{
        m_autoStart = autostart;
    }
    m_process->setProgram(call);
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
