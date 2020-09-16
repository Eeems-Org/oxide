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
        delete app;
    }

    Q_PROPERTY(QString name MEMBER _name NOTIFY nameChanged)
    Q_PROPERTY(QString desc MEMBER _desc NOTIFY descChanged)
    Q_PROPERTY(QString call MEMBER _call NOTIFY callChanged)
    Q_PROPERTY(QString term MEMBER _term NOTIFY termChanged)
    Q_PROPERTY(QString imgFile MEMBER _imgFile NOTIFY imgFileChanged)

    bool ok();

    Q_INVOKABLE void execute();
signals:
    void nameChanged();
    void descChanged();
    void callChanged();
    void termChanged();
    void imgFileChanged();

private slots:
    void exited(int);

private:
    Application* app;
    QString _name;
    QString _desc;
    QString _call;
    QString _term;
    QString _imgFile = "qrc:/img/icon.png";
    char* frameBuffer;
    int frameBufferHandle;
};
#endif // APP_H
