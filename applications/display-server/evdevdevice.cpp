#include "evdevdevice.h"

#include <liboxide/debug.h>
#include <liboxide/threading.h>
#include <libevdev/libevdev.h>
#include <QFileInfo>
#include <QThread>

EvDevDevice::EvDevDevice(QThread* handler, event_device device)
: QObject(handler),
  device(device),
  sys("/sys/class/input/" + devName() + "/device/")
{
    _name = sys.strProperty("name").c_str();
    notifier = new QSocketNotifier(device.fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &EvDevDevice::readEvents);
    notifier->setEnabled(true);
    libevdev_new_from_fd(device.fd, &dev);
}

EvDevDevice::~EvDevDevice(){
    notifier->setEnabled(false);
    notifier->deleteLater();
    unlock();
    libevdev_free(dev);
}

QString EvDevDevice::devName(){ return QFileInfo(path()).baseName(); }

QString EvDevDevice::name(){ return _name; }

QString EvDevDevice::path(){
    return QFileInfo(device.device.c_str()).canonicalFilePath();
}

QString EvDevDevice::id(){
    return QString("%1:%2").arg(
        sys.strProperty("id/vendor").c_str(),
        sys.strProperty("id/product").c_str()
    );
}

unsigned int EvDevDevice::number(){ return devName().midRef(5).toInt(); }

bool EvDevDevice::exists(){ return QFile::exists(path()); }

void EvDevDevice::lock(){ exists() && device.lock(); }

void EvDevDevice::unlock(){ exists() && device.locked && device.unlock(); }

void EvDevDevice::readEvents(){
    Oxide::dispatchToThread(thread(), [this]{
        notifier->setEnabled(false);
        int res;
        bool sync = false;
        do{
            input_event event;
            res = libevdev_next_event(
                dev,
                sync ? LIBEVDEV_READ_FLAG_SYNC : LIBEVDEV_READ_FLAG_NORMAL,
                &event
            );
            sync = res == LIBEVDEV_READ_STATUS_SYNC;
            if(res != LIBEVDEV_READ_STATUS_SUCCESS && res != LIBEVDEV_READ_STATUS_SYNC){
                continue;
            }
            events.push_back(event);
            if(event.type == EV_SYN && event.code == SYN_REPORT){
                emit inputEvents(events);
                events.clear();
            }
        }while(res == LIBEVDEV_READ_STATUS_SUCCESS);
        notifier->setEnabled(true);
    });
}
