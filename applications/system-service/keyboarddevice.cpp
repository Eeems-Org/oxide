#include "keyboarddevice.h"
#include "buttonhandler.h"
#include "keyboardhandler.h"

KeyboardDevice::KeyboardDevice(QThread* handler, event_device device)
: QObject(handler),
  device(device),
  sys("/sys/class/input/" + devName() + "/device/")
{
    _name = sys.strProperty("name").c_str();
    device.lock();
    notifier = new QSocketNotifier(device.fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &KeyboardDevice::readEvents);
    notifier->setEnabled(true);
}

KeyboardDevice::~KeyboardDevice(){
    if(exists() && device.locked){
        device.unlock();
    }
}

QString KeyboardDevice::devName(){ return QFileInfo(device.device.c_str()).baseName(); }

QString KeyboardDevice::name(){ return _name; }

QString KeyboardDevice::path(){ return device.device.c_str(); }

QString KeyboardDevice::id(){
    return QString("%1:%2")
        .arg(sys.strProperty("id/vendor").c_str())
        .arg(sys.strProperty("id/product").c_str());
}

bool KeyboardDevice::exists(){ return QFile::exists(path()); }

void KeyboardDevice::flood(){
    if(device.fd == -1 || !device.locked){
        return;
    }
    ::write(device.fd, buttonHandler->event_flood(), EVENT_FLOOD_SIZE);
}

void KeyboardDevice::readEvents(){
    notifier->setEnabled(false);
    input_event event;
    while(::read(device.fd, &event, sizeof(input_event)) > 0){
        switch(event.type){
            case EV_KEY:
                pressed[event.code] = event.value;
                break;
            case EV_SYN:
                switch(event.code){
                    case SYN_DROPPED:
                        pressed.clear();
                        break;
                    case SYN_REPORT:
                        auto handler = static_cast<KeyboardHandler*>(parent());
                        for(auto code : pressed.keys()){
                            handler->writeEvent(EV_KEY, code, pressed[code]);
                        }
                        handler->writeEvent(EV_SYN, SYN_REPORT, 0);
                        pressed.clear();
                        break;
                }
                break;
        }
    }
    notifier->setEnabled(true);
}
