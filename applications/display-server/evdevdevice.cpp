#include "evdevdevice.h"
#include "evdevhandler.h"

#include <QFileInfo>

EvDevDevice::EvDevDevice(QThread* handler, event_device device)
: QObject(handler),
  device(device),
  sys("/sys/class/input/" + devName() + "/device/")
{
    _name = sys.strProperty("name").c_str();
    notifier = new QSocketNotifier(device.fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &EvDevDevice::readEvents);
    notifier->setEnabled(true);
}

EvDevDevice::~EvDevDevice(){ unlock(); }

QString EvDevDevice::devName(){ return QFileInfo(device.device.c_str()).baseName(); }

QString EvDevDevice::name(){ return _name; }

QString EvDevDevice::path(){ return device.device.c_str(); }

QString EvDevDevice::id(){
    return QString("%1:%2").arg(
        sys.strProperty("id/vendor").c_str(),
        sys.strProperty("id/product").c_str()
    );
}

bool EvDevDevice::exists(){ return QFile::exists(path()); }

void EvDevDevice::lock(){ exists() && device.lock(); }

void EvDevDevice::unlock(){ exists() && device.locked && device.unlock(); }


void EvDevDevice::readEvents(){
    notifier->setEnabled(false);
    auto handler = static_cast<EvDevHandler*>(parent());
    input_event event;
    while(::read(device.fd, &event, sizeof(input_event)) > 0){
        handler->writeEvent(&event);
    }
    notifier->setEnabled(true);
}
