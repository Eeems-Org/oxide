#ifndef APP_H
#define APP_H
#include <QObject>

#include "inputmanager.h"
#include "application_interface.h"

#ifndef OXIDE_SERVICE
#define OXIDE_SERVICE "codes.eeems.oxide1"
#define OXIDE_SERVICE_PATH "/codes/eeems/oxide1"
#endif

using namespace codes::eeems::oxide1;

class AppItem : public QObject {
    Q_OBJECT
public:
    InputManager* inputManager;

    AppItem(QObject* parent) : QObject(parent){}

    ~AppItem(){
        if(app != nullptr){
            delete app;
        }
    }

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
    void exited(int);

private:
    Application* app = nullptr;
    QString _name;
    QString _displayName;
    QString _desc;
    QString _call;
    QString _imgFile = "qrc:/img/icon.png";
    bool _running = false;
    char* frameBuffer;
    int frameBufferHandle;
};
#endif // APP_H
