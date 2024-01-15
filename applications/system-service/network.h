#ifndef NETWORK_H
#define NETWORK_H

#include <QDBusConnection>
#include <QMutableListIterator>
#include <QMutex>

#include <liboxide.h>

#include "supplicant.h"

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/meta.h"

class Network : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", OXIDE_NETWORK_INTERFACE)
    Q_PROPERTY(bool enabled READ enabled  WRITE setEnabled  NOTIFY stateChanged)
    Q_PROPERTY(QString ssid READ ssid CONSTANT)
    Q_PROPERTY(QList<QDBusObjectPath> bSSs READ bSSs)
    Q_PROPERTY(QString password READ getNull WRITE setPassword)
    Q_PROPERTY(QString protocol READ protocol WRITE setProtocol)
    Q_PROPERTY(QVariantMap properties READ properties WRITE setProperties NOTIFY propertiesChanged)

public:
    Network(QString path, QString ssid, QVariantMap properties, QObject* parent);
    Network(QString path, QVariantMap properties, QObject* parent);

    ~Network();
    QString path();
    void registerPath();
    void unregisterPath();

    bool enabled();
    void setEnabled(bool enabled);

    QString ssid();

    QList<QDBusObjectPath> bSSs();

    QString getNull();

    QString password();
    void setPassword(QString password);

    QString protocol();
    void setProtocol(QString protocol);

    QVariantMap properties();
    void setProperties(QVariantMap properties);
    QList<QString> paths();
    void addNetwork(const QString& path, Interface* interface);
    void removeNetwork(const QString& path);
    void removeInterface(Interface* interface);
    void registerNetwork();
    Q_INVOKABLE void connect();
    Q_INVOKABLE void remove();

signals:
    void stateChanged(bool);
    void propertiesChanged(QVariantMap);
    void connected();
    void disconnected();
    void removed();


private slots:
    void PropertiesChanged(const QVariantMap& properties);

private:
    QString m_path;
    QList<INetwork*> networks;
    QVariantMap m_properties;
    QString m_ssid;
    QString m_protocol;
    bool m_enabled = false;

    bool hasPermission(QString permission, const char* sender = __builtin_FUNCTION());

    QVariantMap realProps();
    QString passwordField();
};

#endif // NETWORK_H
