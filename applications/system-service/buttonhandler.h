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

    ButtonHandler();
    void setEnabled(bool enabled);

public slots:
    void pressKey(Qt::Key);

private slots:
    void keyDown(Qt::Key key);
    void keyUp(Qt::Key key);
    void timeout();

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
};

#endif // BUTTONHANDLER_H
