#ifndef BSS_H
#define BSS_H

#include <QMutableListIterator>
#include <QMutex>

#include <liboxide.h>

#include "supplicant.h"

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/meta.h"

class BSS : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_BSS_INTERFACE)
    Q_PROPERTY(QString bssid READ bssid)
    Q_PROPERTY(QString ssid READ ssid)
    Q_PROPERTY(bool privacy READ privacy)
    Q_PROPERTY(ushort frequency READ frequency)
    Q_PROPERTY(ushort signal READ signal)
    Q_PROPERTY(QDBusObjectPath network READ network)
    Q_PROPERTY(QStringList key_mgmt READ key_mgmt)

public:
    BSS(QString path, QString bssid, QString ssid, QObject* parent);
    BSS(QString path, IBSS* bss, QObject* parent) : BSS(path, bss->bSSID(), bss->sSID(), parent) {}

    ~BSS();
    QString path();
    void registerPath();
    void unregisterPath();

    QString bssid();
    QString ssid();

    QList<QString> paths();
    void addBSS(const QString& path);
    void removeBSS(const QString& path);
    bool privacy();
    ushort frequency();
    short signal();
    QDBusObjectPath network();
    QStringList key_mgmt();
    Q_INVOKABLE QDBusObjectPath connect();

signals:
    void removed();
    void propertiesChanged(QVariantMap);

private slots:
    void PropertiesChanged(const QVariantMap& properties);

private:
    QString m_path;
    QList<IBSS*> bsss;
    QString m_bssid;
    QString m_ssid;

    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
};

#endif // BSS_H
