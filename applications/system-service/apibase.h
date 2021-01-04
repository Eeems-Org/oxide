#ifndef APIBASE_H
#define APIBASE_H

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

#include <unistd.h>

#include "dbussettings.h"

class APIBase : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
public:
    APIBase(QObject* parent) : QObject(parent) {}
    virtual void setEnabled(bool enabled) = 0;
    int hasPermission(QString permission);

protected:
    int getSenderPid(){
        if(!calledFromDBus()){
            return getpid();
        }
        return connection().interface()->servicePid(message().service());
    }
    int getSenderPgid(){ return getpgid(getSenderPid()); }
};

#endif // APIBASE_H
