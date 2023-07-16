#ifndef BUTTONHANDLER_H
#define BUTTONHANDLER_H

#include <QThread>
#include <QFile>
#include <QException>
#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>

#include <string>
#include <iostream>
#include <unordered_map>
#include <ext/stdio_filebuf.h>
#include <linux/input.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sstream>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <cstdlib>
#include <liboxide.h>
#include <liboxide/event_device.h>

using namespace std;
using namespace Oxide;

#define buttonHandler ButtonHandler::init()

struct PressRecord {
    bool pressed = false;
    struct timeval pressTime;
    string name = "Unknown";
    Qt::Key keyCode = Qt::Key_unknown;
    PressRecord (string name, Qt::Key keyCode) : name(name), keyCode(keyCode) {}
    PressRecord() : PressRecord("Unknown", Qt::Key_unknown){}
};

static event_device buttons(deviceSettings.getButtonsDevicePath(), O_RDWR);


class ButtonHandler : public QThread {
    Q_OBJECT

public:
    static ButtonHandler* init();

    ButtonHandler() : QThread(), filebuf(buttons.fd, ios::in), stream(&filebuf), pressed(), timer(this), m_enabled(true) {
        flood = build_flood();
        timer.setInterval(100);
        timer.setSingleShot(false);
        connect(&timer, &QTimer::timeout, this, &ButtonHandler::timeout);
        Oxide::startThreadWithPriority(this, QThread::HighestPriority);
        moveToThread(this);
    }
    void setEnabled(bool enabled){
        m_enabled = enabled;
    }
    void clear_buffer(){
        if(buttons.fd == -1){
            return;
        }
#ifdef DEBUG
        qDebug() << "Clearing event buffer on" << buttons.device.c_str();
#endif
        ::write(buttons.fd, flood, 512 * 8 * 4 * sizeof(input_event));
    }

public slots:
    void pressKey(Qt::Key);

private slots:
    void keyDown(Qt::Key key){
        if(!m_enabled){
            return;
        }
        qDebug() << "Down" << key;
        if(validKeys.contains(key) && !pressed.contains(key)){
            QElapsedTimer timer;
            timer.start();
            pressed.insert(key, timer);
        }
    }
    void keyUp(Qt::Key key){
        if(!m_enabled){
            return;
        }
        qDebug() << "Up" << key;
        if(!pressed.contains(key)){
            // This should never happen
            return;
        }
        auto value = pressed.value(key);
        pressed.remove(key);
        if(value.hasExpired(700)){
            // Held event already fired
            return;
        }
        if(key == Qt::Key_PowerOff){
            emit powerPress();
            return;
        }
//        pressKey(key);
    }
    void timeout(){
        if(!m_enabled){
            return;
        }
        for(auto key : pressed.keys()){
            // If the key has been held for a while
            if(!pressed.value(key).hasExpired(700)){
                continue;
            }
            qDebug() << "Key held" << key;
            switch(key){
                case Qt::Key_Left:
                    emit leftHeld();
                break;
                case Qt::Key_Home:
                    emit homeHeld();
                break;
                case Qt::Key_Right:
                    emit rightHeld();
                break;
                case Qt::Key_PowerOff:
                    emit powerHeld();
                break;
                default:
                    continue;
            }
            pressed.remove(key);
        }
    }

signals:
    void leftHeld();
    void homeHeld();
    void rightHeld();
    void powerHeld();
    void powerPress();
    void activity();
    void rawEvent(const input_event&);

protected:
    void run();
    __gnu_cxx::stdio_filebuf<char> filebuf;
    istream stream;
    QMap<Qt::Key, QElapsedTimer> pressed;
    QTimer timer;
    const QSet<Qt::Key> validKeys { Qt::Key_Left, Qt::Key_Home, Qt::Key_Right, Qt::Key_PowerOff };
    bool m_enabled;
    input_event* flood;
    input_event* build_flood(){
        auto n = 512 * 8;
        auto num_inst = 4;
        input_event* ev = (input_event *)malloc(sizeof(struct input_event) * n * num_inst);
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
    static inline input_event createEvent(ushort type, ushort code, int value){
        struct input_event event;
        event.type = type;
        event.code = code;
        event.value = value;
        return event;
    }
};

#endif // BUTTONHANDLER_H
