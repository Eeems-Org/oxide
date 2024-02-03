#pragma once
#ifndef DIGITIZERHANDLER_H
#define DIGITIZERHANDLER_H

#include <QThread>
#include <QException>

#include <ext/stdio_filebuf.h>
#include <sstream>
#include <linux/input.h>
#include <iostream>
#include <string>
#include <vector>
#include <liboxide.h>
#include <liboxide/event_device.h>

using namespace std;
using namespace Oxide;

#define touchHandler DigitizerHandler::singleton_touchScreen()
#define wacomHandler DigitizerHandler::singleton_wacom()

class DigitizerHandler : public QThread {
    Q_OBJECT

public:
    static DigitizerHandler* singleton_touchScreen();
    static DigitizerHandler* singleton_wacom();

    DigitizerHandler(event_device& device);
    ~DigitizerHandler();
    void setEnabled(bool enabled);
    void grab();
    void ungrab();
    bool grabbed();
    void write(ushort type, ushort code, int value);
    void write(input_event* events, size_t size);
    void syn();
    void clear_buffer();
    static input_event createEvent(ushort type, ushort code, int value);

signals:
    void activity();
    void inputEvent(const input_event& event);

protected:
    input_event* flood;
    input_event* build_flood();
    void run();
    bool handle_events();
    void skip_event();
    __gnu_cxx::stdio_filebuf<char> filebuf;
    istream stream;
    bool m_enabled;
    event_device device;
    bool read(input_event* ie);
};

#endif // DIGITIZERHANDLER_H
