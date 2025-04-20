#include "evdevdevice.h"

#include <liboxide/debug.h>
#include <liboxide/threading.h>
#include <libevdev/libevdev.h>
#include <QFileInfo>
#include <QThread>

EvDevDevice::EvDevDevice(QThread* handler, const event_device& device)
: QObject(handler),
  device(device),
  sys("/sys/class/input/" + devName() + "/device/")
{
    int flags = fcntl(this->device.fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(this->device.fd, F_SETFL, flags);
    O_DEBUG(device.device.c_str() << this->device.fd);
    _name = sys.strProperty("name").c_str();
    notifier = new QSocketNotifier(this->device.fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &EvDevDevice::readEvents);
    notifier->setEnabled(true);
    libevdev_new_from_fd(this->device.fd, &dev);
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

void EvDevDevice::clear_buffer(){
    auto flood = build_flood();
    ::write(device.fd, flood, 512 * 8 * 4 * sizeof(input_event));
    delete flood;
}

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
            if(res < 0){
                break;
            }
            sync = res == LIBEVDEV_READ_STATUS_SYNC;
            events.push_back(event);
            if(event.type == EV_SYN && event.code == SYN_REPORT){
                emit inputEvents(events);
                events.clear();
            }
        }while(libevdev_has_event_pending(dev));
        emitSomeEvents();
        if(res == -ENODEV){
            // Device went away
            O_WARNING("Device disapeared while reading events");
            return;
        }
        if(res != -EAGAIN){
            O_WARNING("Failed to read input:" << strerror(errno));
        }
        notifier->setEnabled(true);
    });
}

void EvDevDevice::emitSomeEvents(){
    if(events.empty()){
        return;
    }
    for(auto i = events.rbegin();  i != events.rend(); ++i){
        auto& event = *i;
        if(event.type == EV_SYN && event.code == SYN_REPORT){
            auto index = std::next(i-1).base();
            auto some = std::vector(events.begin(), index);
            emit inputEvents(some);
            events.erase(events.begin(), index);
            break;
        }
    }
}

input_event EvDevDevice::createEvent(ushort type, ushort code, int value){
    struct input_event event;
    event.type = type;
    event.code = code;
    event.value = value;
    return event;
}

input_event* EvDevDevice::build_flood(){
    auto n = 512 * 8;
    auto num_inst = 4;
    input_event* ev = (input_event*)malloc(sizeof(struct input_event) * n * num_inst);
    memset(ev, 0, sizeof(input_event) * n * num_inst);
    auto i = 0;
    while (i < n) {
        ev[i++] = createEvent(EV_ABS, ABS_DISTANCE, 1);
        ev[i++] = createEvent(EV_SYN, 0, 0);
        ev[i++] = createEvent(EV_ABS, ABS_DISTANCE, 2);
        ev[i++] = createEvent(EV_SYN, 0, 0);
    }
    return ev;
}
