#ifndef APPLICATION_H
#define APPLICATION_H

#include <QObject>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QProcess>

#include "dbussettings.h"

class Application : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_APPLICATION_INTERFACE)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString description READ description)
    Q_PROPERTY(QString call READ call)
    Q_PROPERTY(QString term READ term)
    Q_PROPERTY(bool autoStart READ autoStart)
    Q_PROPERTY(int type READ type)
    Q_PROPERTY(int state READ state)
public:
    Application(QDBusObjectPath path, QObject* parent) : Application(path.path(), parent) {}
    Application(QString path, QObject* parent) : QObject(parent), m_path(path) {
        m_process = new QProcess();
        connect(m_process, &QProcess::started, this, &Application::started);
        connect(m_process, QOverload<int>::of(&QProcess::finished), this, &Application::finished);
        connect(m_process, &QProcess::readyReadStandardError, this, &Application::readyReadStandardError);
        connect(m_process, &QProcess::readyReadStandardOutput, this, &Application::readyReadStandardOutput);
        connect(m_process, &QProcess::stateChanged, this, &Application::stateChanged);
        connect(m_process, &QProcess::errorOccurred, this, &Application::errorOccurred);
    }
    ~Application() {
        unregisterPath();
        m_process->kill();
        delete m_process;
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
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void signal(int signal);
    Q_INVOKABLE void unregister();

    QString name() { return m_name; }
    QString description() { return m_description; }
    QString call() { return m_call; }
    QString term() { return m_term; }
    bool autoStart() { return m_autoStart; }
    int type() { return (int)m_type; }
    int state(){
        switch(m_process->state()){
            case QProcess::Starting:
            case QProcess::Running:
                // TODO keep track of real state
                return InForeground;
            break;
            case QProcess::NotRunning:
            default:
                return Inactive;
        }
    }

    void load(
        QString name, QString description, QString call, QString term,
        int type, bool autostart
    );
    bool isRunning(){
        return m_process->processId() && m_process->state() != QProcess::NotRunning;
    }
signals:
    void launched();
    void paused();
    void resumed();
    void signaled(int);
    void unregistered();
    void exited(int);

private slots:
    void started();
    void finished(int exitCode);
    void readyReadStandardError(){}
    void readyReadStandardOutput(){}
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
            break;
            default:
                qDebug() << "Application" << name() << "unknown state" << state;
        }
    }
    void errorOccurred(QProcess::ProcessError error);

private:
    QString m_path;
    QString m_name;
    QString m_description;
    QString m_call;
    QString m_term;
    int m_type;
    bool m_autoStart;
    QProcess* m_process;
};

#endif // APPLICATION_H
