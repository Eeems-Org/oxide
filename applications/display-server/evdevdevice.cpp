#include "evdevdevice.h"

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
}

EvDevDevice::~EvDevDevice(){ unlock(); }

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
    notifier->setEnabled(false);
    input_event event;
    while(::read(device.fd, &event, sizeof(input_event)) > 0){
        events.push_back(event);
        switch(event.type){
            case EV_SYN:
                switch(event.code){
                    case SYN_DROPPED:
                        events.clear();
                        break;
                    case SYN_REPORT:
                        emit inputEvents(events);
                        events.clear();
                        break;
                }
                break;
        }
    }
    notifier->setEnabled(true);
}
