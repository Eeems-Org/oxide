#ifndef APP_H
#define APP_H
#include <QObject>
#include <liboxide/dbus.h>

using namespace codes::eeems::oxide1;

class AppItem : public QObject {
    Q_OBJECT

public:
    AppItem(QObject* parent) : QObject(parent){}

    ~AppItem(){
        if(app != nullptr){
            delete app;
        }
    }

    Q_PROPERTY(QString path MEMBER _path)
    Q_PROPERTY(QString name MEMBER _name NOTIFY nameChanged)
    Q_PROPERTY(QString displayName MEMBER _displayName NOTIFY displayNameChanged)
    Q_PROPERTY(QString desc MEMBER _desc NOTIFY descChanged)
    Q_PROPERTY(QString call MEMBER _call NOTIFY callChanged)
    Q_PROPERTY(QString imgFile MEMBER _imgFile NOTIFY imgFileChanged)
    Q_PROPERTY(bool running MEMBER _running NOTIFY runningChanged)

    bool ok();

    Q_INVOKABLE void execute();
    Q_INVOKABLE void stop();

signals:
    void nameChanged(QString);
    void displayNameChanged(QString);
    void descChanged(QString);
    void callChanged(QString);
    void imgFileChanged(QString);
    void runningChanged(bool);

private slots:
    void exited(int exitCode){
        qDebug() << "Application exited" << exitCode;
        _running = false;
        emit runningChanged(false);
    }
    void launched(){
        _running = true;
        emit runningChanged(true);
    }
    void onDisplayNameChanged(QString displayName){
        _displayName = displayName;
        emit displayNameChanged(displayName);
    }
    void onIconChanged(QString path){
        _imgFile = path;
        emit imgFileChanged(path);
    }

private:
    Application* app = nullptr;
    QString _path;
    QString _name;
    QString _displayName;
    QString _desc;
    QString _call;
    QString _imgFile = "qrc:/img/icon.png";
    bool _running = false;
    char* frameBuffer;
    int frameBufferHandle;

    Application* getApp();
};
#endif // APP_H
