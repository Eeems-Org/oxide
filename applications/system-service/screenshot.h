#ifndef SCREENSHOT_H
#define SCREENSHOT_H
#include <QObject>
#include <QtDBus>
#include <QMutex>

#include <sys/stat.h>
#include <sys/types.h>
#include <liboxide.h>

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/meta.h"

class Screenshot : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_SCREENSHOT_INTERFACE)
    Q_PROPERTY(QByteArray blob READ blob WRITE setBlob)
    Q_PROPERTY(QString path READ getPath)

public:
    Screenshot(QString path, QString filePath, QObject* parent);
    ~Screenshot();
    QString path();
    QDBusObjectPath qPath();
    void registerPath();
    void unregisterPath();
    QByteArray blob();
    void setBlob(QByteArray blob);
    QString getPath();

signals:
    void modified();
    void removed();

public slots:
    void remove();

private:
    QString m_path;
    QFile* m_file;
    QMutex mutex;

    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
};

#endif // SCREENSHOT_H
