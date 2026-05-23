#ifndef SCREENSHOTAPI_H
#define SCREENSHOTAPI_H

#include <fcntl.h>
#include <liboxide.h>
#include <liboxide/devicesettings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <QDBusObjectPath>
#include <QDebug>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QPainter>

#include "apibase.h"
#include "screenshot.h"

#define DISPLAYWIDTH (deviceSettings.getScreenWidth())
#define DISPLAYHEIGHT (deviceSettings.getScreenHeight())
#define TEMP_USE_REMARKABLE_DRAW 0x0018
#define remarkable_color uint16_t
#define DISPLAYSIZE DISPLAYWIDTH * DISPLAYHEIGHT * sizeof(remarkable_color)

#define screenAPI ScreenAPI::singleton()

class ScreenAPI : public APIBase
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_SCREEN_INTERFACE)
    Q_PROPERTY(bool enabled READ enabled)
    Q_PROPERTY(QList<QDBusObjectPath> screenshots READ screenshots)

  public:
    static ScreenAPI* singleton(ScreenAPI* self = nullptr);
    ScreenAPI(QObject* parent);
    void setEnabled(bool enabled);
    bool enabled();
    QList<QDBusObjectPath> screenshots();
    Q_INVOKABLE QDBusObjectPath screenshot();

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
    QString getTimestamp();
    QString getNextPath();
};

#endif // SCREENSHOTAPI_H
