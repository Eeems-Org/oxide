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

#include "inputmanager.h"
#include "event_device.h"
#include "settings.h"

using namespace std;

//#define PNG_PATH "/tmp/fb.png"
#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872
#define TEMP_USE_REMARKABLE_DRAW 0x0018
#define DISPLAYSIZE DISPLAYWIDTH * DISPLAYHEIGHT * sizeof(uint16_t)

#define buttonHandler ButtonHandler::init()

struct PressRecord {
    bool pressed = false;
    struct timeval pressTime;
    string name = "Unknown";
    Qt::Key keyCode = Qt::Key_unknown;
    PressRecord (string name, Qt::Key keyCode) : name(name), keyCode(keyCode) {}
    PressRecord() : PressRecord("Unknown", Qt::Key_unknown){}
};

const event_device buttons(Settings::instance().getButtonsDevicePath(), O_RDWR);

class ButtonHandler : public QThread {
    Q_OBJECT

public:
    static ButtonHandler* init();
    static string exec(const char* cmd);
    static vector<std::string> split_string_by_newline(const std::string& str);
    static int is_uint(string input);

    ButtonHandler() : QThread(), filebuf(buttons.fd, ios::in), stream(&filebuf), pressed(), timer(this), m_enabled(true) {
        timer.setInterval(100);
        timer.setSingleShot(false);
        connect(&timer, &QTimer::timeout, this, &ButtonHandler::timeout);
        timer.start();
    }
    void setEnabled(bool enabled){
        m_enabled = enabled;
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
        pressKey(key);
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

protected:
    void run();
    __gnu_cxx::stdio_filebuf<char> filebuf;
    istream stream;
    QMap<Qt::Key, QElapsedTimer> pressed;
    QTimer timer;
    const QSet<Qt::Key> validKeys { Qt::Key_Left, Qt::Key_Home, Qt::Key_Right, Qt::Key_PowerOff };
    bool m_enabled;
};

#endif // BUTTONHANDLER_H
