#ifndef APIBASE_H
#define APIBASE_H

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

#include <liboxide.h>
#include <unistd.h>

// Must be included so that generate_xml.sh will work
#include "../../shared/liboxide/meta.h"

class APIBase : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
public:
    APIBase(QObject* parent);
    virtual void setEnabled(bool enabled) = 0;
    int hasPermission(QString permission, const char* sender = __builtin_FUNCTION());
    int getSenderPgid(){ return getpgid(getSenderPid()); }

protected:
    int getSenderPid(){
        if(!calledFromDBus()){
            return getpid();
        }
        return connection().interface()->servicePid(message().service());
    }
    static void shutdown();
};

#endif // APIBASE_H
