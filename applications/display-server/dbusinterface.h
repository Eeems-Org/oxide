#pragma once

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

#include <liboxide/meta.h>

#define BLIGHT_SERVICE "codes.eeems.blight1"
#define BLIGHT_SERVICE_PATH "/codes/eeems/blight1"
#define BLIGHT_INTERFACE BLIGHT_SERVICE ".General"

class DbusInterface : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
    Q_CLASSINFO("D-Bus Interface", BLIGHT_INTERFACE)

public:
    DbusInterface(QObject* parent);

private slots:
    void serviceOwnerChanged(const QString& name, const QString& oldOwner, const QString& newOwner);
};
