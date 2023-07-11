#include <QTimer>
#include <QFile>

#include <signal.h>
#include <liboxide.h>

#include "application.h"
#include "appsapi.h"
#include "systemapi.h"
#include "guiapi.h"
#include "buttonhandler.h"
#include "digitizerhandler.h"

using namespace Oxide::Applications;

const event_device touchScreen(deviceSettings.getTouchDevicePath(), O_WRONLY);

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
            if(flags().contains("nolog")){
                m_process->closeReadChannel(QProcess::StandardError);
                m_process->closeReadChannel(QProcess::StandardOutput);
            }else{
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
        if(!flags().contains("nosavescreen")){
            saveScreen();
        }
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
        if(!flags().contains("nosavescreen") && (type() != Backgroundable || stateNoSecurityCheck() == Paused)){
            recallScreen();
        }
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
    m_pid = processId();
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
    if(m_pid != -1){
        guiAPI->closeWindows(m_pid);
    }
    m_pid = -1;
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
void Application::showSplashScreen(){
    auto frameBuffer = EPFrameBuffer::framebuffer();
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
                    EPFrameBuffer::waitForLastUpdate();
                }
            });
        });
        qDebug() << "Displaying splashscreen for" << name();
        Oxide::Sentry::sentry_span(t, "paint", "Draw splash screen", [this, frameBuffer](){
            dispatchToMainThread([this, frameBuffer]{
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
            });
        });
        qDebug() << "Waitng for screen to finish...";
        Oxide::Sentry::sentry_span(t, "wait", "Wait for screen finish updating", [](){
            dispatchToMainThread([]{
                EPFrameBuffer::waitForLastUpdate();
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
