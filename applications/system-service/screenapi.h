#ifndef SCREENSHOTAPI_H
#define SCREENSHOTAPI_H

#include <QObject>
#include <QFile>
#include <QDBusObjectPath>
#include <QMutex>

#include "apibase.h"
#include "screenshot.h"

#define screenAPI ScreenAPI::singleton()

class ScreenAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_SCREEN_INTERFACE)
    Q_PROPERTY(bool enabled READ isEnabled)
    Q_PROPERTY(QList<QDBusObjectPath> screenshots READ screenshots)

public:
    static ScreenAPI* instance;
    static ScreenAPI* singleton(){ return instance; }
    ScreenAPI(QObject* parent);
    ~ScreenAPI(){}
    void setEnabled(bool enabled);
    bool isEnabled();
    QList<QDBusObjectPath> screenshots();
    Q_INVOKABLE bool drawFullscreenImage(QString path);
    Q_INVOKABLE QDBusObjectPath screenshot();
    QImage copy();

public slots:
    QDBusObjectPath addScreenshot(QByteArray blob);

signals:
    void screenshotAdded(QDBusObjectPath);
    void screenshotRemoved(QDBusObjectPath);
    void screenshotModified(QDBusObjectPath);

private:
    QList<Screenshot*> m_screenshots;
    bool m_enabled;
    QMutex mutex;

    Screenshot* addScreenshot(QString filePath);
    void mkdirs(const QString& path, mode_t mode = 0700);
    QString getTimestamp(){ return QDateTime::currentDateTime().toString("yyyy-MM-ddThhmmss.zzz"); }
    QString getNextPath();
};

#endif // SCREENSHOTAPI_H
