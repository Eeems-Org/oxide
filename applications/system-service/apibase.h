#ifndef APIBASE_H
#define APIBASE_H

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QImage>

#include <unistd.h>
#include <liboxide.h>
#include <liboxide/dbus.h>
#include <libblight.h>

#ifdef Q_MOC_RUN
#include "../../shared/liboxide/meta.h"
#endif

using namespace codes::eeems::blight1;


class APIBase : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
public:
    APIBase(QObject* parent) : QObject(parent) {}
    virtual void setEnabled(bool enabled) = 0;
    int hasPermission(QString permission, const char* sender = __builtin_FUNCTION());

  protected:
    int getSenderPid();
    int getSenderPgid();
};
QImage* getFrameBuffer();
Compositor* getCompositorDBus();
Blight::shared_buf_t createBuffer(const QRect& rect, unsigned int stride, Blight::Format format);
Blight::shared_buf_t createBuffer();
void addSystemBuffer(Blight::shared_buf_t buffer);

#endif // APIBASE_H
