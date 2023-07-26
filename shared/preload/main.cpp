#include <iostream>
#include <chrono>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <mxcfb.h>
#include <dlfcn.h>
#include <stdio.h>
#include <linux/fb.h>
#include <linux/input-event-codes.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <liboxide/meta.h>
#include <liboxide/math.h>
#include <liboxide/debug.h>
#include <liboxide/devicesettings.h>
#include <liboxide/tarnish.h>
#include <liboxide/threading.h>
#include <systemd/sd-journal.h>

#include "input.h"

class ClockWatch {
public:
    std::chrono::high_resolution_clock::time_point t1;

    ClockWatch(){ t1 = std::chrono::high_resolution_clock::now(); }

    auto elapsed(){
        auto t2 = std::chrono::high_resolution_clock::now();
        auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
        return time_span.count();
    }
};

static bool IS_INITIALIZED = false;
thread_local bool IS_OPENING = false;
static bool DEBUG_LOGGING = false;
static bool FAILED_INIT = true;
static bool IS_XOCHITL = false;
static bool IS_FBDEPTH = false;
static bool IS_LOADABLE_APP = false;
static bool DO_HANDLE_FB = true;
static bool DO_HANDLE_INPUT = true;
static bool PIPE_OPENED = false;
static bool IS_RAISED = false;
static int fbFd = -1;
static int touchFds[2] = {-1, -1};
static int tabletFds[2] = {-1, -1};
static int keyFds[2] = {-1, -1};
static ssize_t(*func_write)(int, const void*, size_t);
static int(*func_open)(const char*, int, mode_t);
static int(*func_ioctl)(int, unsigned long, ...);
static int(*func_close)(int);
static QMutex logMutex;
static unsigned int completedMarker;
static QMutex completedMarkerMutex;
static QMutex eventPipeMutex;

// Use this instead of Oxide::getAppName to avoid recursion when logging in open()
QString appName(){
    if(!QCoreApplication::startingUp()){
        return qApp->applicationName();
    }
    static QString name;
    if(!name.isEmpty()){
        return name;
    }
    int fd = func_open("/proc/self/comm", O_RDONLY, 0);
    if(fd != -1){
        FILE* f = fdopen(fd, "r");
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        char* where = new char[size];
        rewind(f);
        fread(where, sizeof(char), size, f);
        name = QString::fromLatin1(where, size);
        delete[] where;
    }
    if(!name.isEmpty()){
        return name;
    }
    name = QFileInfo("/proc/self/exe").canonicalFilePath();
    if(name.isEmpty()){
        return "liboxide_preload.so";
    }
    return name;
}

template<class... Args>
void __printf(char const* file, unsigned int line, char const* func, int priority, Args... args){
    if(!DEBUG_LOGGING){
        return;
    }
    QStringList list;
    ([&]{ list << QVariant(args).toString(); }(), ...);
    auto msg = list.join(' ');
    sd_journal_send(
        "MESSAGE=%s", msg.toStdString().c_str(),
        "PRIORITY=%i", priority,
        "CODE_FILE=%s", file,
        "CODE_LINE=%i", line,
        "CODE_FUNC=%s", func,
        "SYSLOG_IDENTIFIER=%s", appName().toStdString().c_str(),
        "SYSLOG_FACILITY=%s", "LOG_DAEMON",
        NULL
    );
    if(!isatty(fileno(stdin)) || !isatty(fileno(stdout)) || !isatty(fileno(stderr))){
        return;
    }
    QMutexLocker locker(&logMutex);
    Q_UNUSED(logMutex)
    std::string level;
    switch(priority){
        case LOG_INFO:
            level = "Info";
        break;
        case LOG_WARNING:
            level = "Warning";
        break;
        case LOG_CRIT:
            level = "Critical";
        break;
        default:
            level = "Debug";
    }
    fprintf(
        stderr,
        "[%i:%i:%i %s] %s: %s (%s:%u, %s)\n",
        getpgrp(),
        getpid(),
        gettid(),
        appName().toStdString().c_str(),
        level.c_str(),
        msg.toStdString().c_str(),
        file,
        line,
        func
    );
}
#define _PRINTF(priority, ...) __printf("shared/preload/main.cpp", __LINE__, __PRETTY_FUNCTION__, priority, __VA_ARGS__)
#define _DEBUG(...) _PRINTF(LOG_DEBUG, __VA_ARGS__)
#define _WARN(...) _PRINTF(LOG_WARNING, __VA_ARGS__)
#define _INFO(...) _PRINTF(LOG_INFO, __VA_ARGS__)
#define _CRIT(...) _PRINTF(LOG_CRIT, __VA_ARGS__)

static struct sigaction int_action;
static struct sigaction term_action;
static struct sigaction segv_action;
static struct sigaction abrt_action;

void __sigacton__handler(int signo, siginfo_t* info, void* context){
    Q_UNUSED(info)
    Q_UNUSED(context)
    switch(signo){
        case SIGSEGV:
            sigaction(signo, &segv_action, NULL);
            break;
        case SIGTERM:
            sigaction(signo, &term_action, NULL);
            break;
        case SIGINT:
            sigaction(signo, &int_action, NULL);
            break;
        case SIGABRT:
            sigaction(signo, &abrt_action, NULL);
            break;
        default:
            signal(signo, SIG_DFL);
    }
    static bool KILLED = false;
    _DEBUG("Signal", strsignal(signo), "recieved.");
    if(!KILLED){
        KILLED = true;
        Oxide::Tarnish::disconnect();
        qApp->exit(signo);
    }
    raise(signo);
}

bool __sigaction_connect(int signo){
    switch(signo){
        case SIGSEGV:
            sigaction(signo, NULL, &segv_action);
            break;
        case SIGTERM:
            sigaction(signo, NULL, &term_action);
            break;
        case SIGINT:
            sigaction(signo, NULL, &int_action);
            break;
        case SIGABRT:
            sigaction(signo, NULL, &abrt_action);
            break;
        default:
            return false;
    }
    struct sigaction action;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    action.sa_sigaction = &__sigacton__handler;
    return sigaction(signo, &action, NULL) != -1;
}

void writeInputEvent(int fd, timeval time, short unsigned int type, short unsigned int code, int value){
    input_event ie{
        .time = time,
        .type = type,
        .code = code,
        .value = value
    };
    int res = func_write(fd, &ie, sizeof(input_event));
    if(res == -1){
        _WARN("Failed to write input event", strerror(errno));
    }else if((unsigned int)res < sizeof(input_event)){
        _WARN("Only wrote", res, "of", sizeof(input_event), "bytes of input_event");
    }
}

void __read_event_pipe(){
    QMutexLocker locker(&eventPipeMutex);
    Q_UNUSED(locker);
    auto eventPipe = Oxide::Tarnish::getEventPipe();
    if(eventPipe == nullptr){
        return;
    }
    Q_ASSERT(QThread::currentThread() == eventPipe->thread());
    while(eventPipe->isOpen() && !eventPipe->atEnd()){
        auto event = Oxide::Tarnish::WindowEvent::fromSocket(eventPipe);
        switch(event.type){
            case Oxide::Tarnish::WaitForPaint:
                _DEBUG("Wait for paint finished", QString::number(event.waitForPaintData.marker));
                completedMarkerMutex.lock();
                if(event.waitForPaintData.marker > completedMarker){
                    completedMarker = event.waitForPaintData.marker;
                }
                completedMarkerMutex.unlock();
                break;
            case Oxide::Tarnish::Close:
                _INFO("Window closed");
                kill(getpid(), SIGTERM);
                break;
            case Oxide::Tarnish::Ping:
                Oxide::runLater(eventPipe->thread(), [eventPipe]{
                    Oxide::Tarnish::WindowEvent event;
                    event.type = Oxide::Tarnish::Ping;
                    event.toSocket(eventPipe);
                });
                break;
            case Oxide::Tarnish::Key:{
                if(!DO_HANDLE_INPUT || keyFds[0] == -1){
                    break;
                }
                auto data = event.keyData;
                timeval time;
                ::gettimeofday(&time, NULL);
                writeInputEvent(keyFds[0], time, EV_KEY, data.scanCode, data.type);
                writeInputEvent(keyFds[0], time, EV_SYN, SYN_REPORT, 0);
                break;
            }
            case Oxide::Tarnish::Touch:{
                if(!DO_HANDLE_INPUT || touchFds[0] == -1){
                    break;
                }
                auto data = event.touchData;
                static std::vector<Oxide::Tarnish::TouchEventPoint> previousPoints;
                timeval time;
                ::gettimeofday(&time, NULL);
                previousPoints.reserve(data.points.size());
                switch(data.type){
                    case Oxide::Tarnish::TouchPress:
                        for(unsigned int i = 0; i < data.points.size(); i++){
                            auto point = data.points[i];
                            if(point.state != Oxide::Tarnish::PointPress){
                                continue;
                            }
                            auto pos = point.originalPoint();
                            bool existingPoint = previousPoints.size() > i;
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_SLOT, i);
                            if(!existingPoint || previousPoints[i].id != point.id){
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_TRACKING_ID, point.id);
                            }
                            if(!existingPoint || previousPoints[i].x != point.x){
                                unsigned int x = ceil(pos.x() * deviceSettings.getTouchWidth());
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_POSITION_X, x);
                            }
                            if(!existingPoint || previousPoints[i].y != point.y){
                                unsigned int y = ceil(pos.y() * deviceSettings.getTouchHeight());
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_POSITION_Y, y);
                            }
                            if(!existingPoint || previousPoints[i].width != point.width){
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_TOUCH_MAJOR, point.width);
                            }
                            if(!existingPoint || previousPoints[i].height != point.height){
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_TOUCH_MINOR, point.height);
                            }
                            if(!existingPoint || previousPoints[i].pressure != point.pressure){
                                unsigned int pressure = ceil(point.pressure * deviceSettings.getTouchPressure());
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_PRESSURE, pressure);
                            }
                            if(!existingPoint || previousPoints[i].rotation != point.rotation){
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_ORIENTATION, point.rotation);
                            }
                            writeInputEvent(touchFds[0], time, EV_SYN, SYN_MT_REPORT, 0);
                            _DEBUG("touch press", i, point.id);
                            previousPoints[i] = data.points[i];
                        }
                        writeInputEvent(touchFds[0], time, EV_SYN, SYN_REPORT, 0);
                        break;
                    case Oxide::Tarnish::TouchUpdate:
                        for(unsigned int i = 0; i < data.points.size(); i++){
                            auto point = data.points[i];
                            if(point.state != Oxide::Tarnish::PointMove){
                                continue;
                            }
                            auto pos = point.originalPoint();
                            bool existingPoint = previousPoints.size() > i;
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_SLOT, i);
                            if(!existingPoint || previousPoints[i].id != point.id){
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_TRACKING_ID, point.id);
                            }
                            if(!existingPoint || previousPoints[i].x != point.x){
                                unsigned int x = ceil(pos.x() * deviceSettings.getTouchWidth());
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_POSITION_X, x);
                            }
                            if(!existingPoint || previousPoints[i].y != point.y){
                                unsigned int y = ceil(pos.y() * deviceSettings.getTouchHeight());
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_POSITION_Y, y);
                            }
                            if(!existingPoint || previousPoints[i].width != point.width){
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_TOUCH_MAJOR, point.width);
                            }
                            if(!existingPoint || previousPoints[i].height != point.height){
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_TOUCH_MINOR, point.height);
                            }
                            if(!existingPoint || previousPoints[i].pressure != point.pressure){
                                unsigned int pressure = ceil(point.pressure * deviceSettings.getTouchPressure());
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_PRESSURE, pressure);
                            }
                            if(!existingPoint || previousPoints[i].rotation != point.rotation){
                                writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_ORIENTATION, point.rotation);
                            }
                            writeInputEvent(touchFds[0], time, EV_SYN, SYN_MT_REPORT, 0);
                            _DEBUG("touch update", i, point.id);
                            previousPoints[i] = data.points[i];
                        }
                        writeInputEvent(touchFds[0], time, EV_SYN, SYN_REPORT, 0);
                        break;
                    case Oxide::Tarnish::TouchRelease:
                        for(unsigned int i = 0; i < data.points.size(); i++){
                            auto point = data.points[i];
                            if(point.state != Oxide::Tarnish::PointRelease){
                                continue;
                            }
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_SLOT, i);
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_TRACKING_ID, -1);
                            writeInputEvent(touchFds[0], time, EV_SYN, SYN_MT_REPORT, 0);
                            _DEBUG("touch release", i, point.id);
                            previousPoints[i] = data.points[i];
                        }
                        writeInputEvent(touchFds[0], time, EV_SYN, SYN_REPORT, 0);
                        break;
                    case Oxide::Tarnish::TouchCancel:
                        // TODO - cancel in progress touches
                        for(unsigned int i = 0; i < data.points.size(); i++){
                            auto point = data.points[i];
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_SLOT, i);
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_TRACKING_ID, point.id);
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_POSITION_X, -1);
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_POSITION_Y, -1);
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_PRESSURE, 0);
                            writeInputEvent(touchFds[0], time, EV_SYN, SYN_MT_REPORT, 0);
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_SLOT, i);
                            writeInputEvent(touchFds[0], time, EV_ABS, ABS_MT_TRACKING_ID, -1);
                            writeInputEvent(touchFds[0], time, EV_SYN, SYN_MT_REPORT, 0);
                            _DEBUG("touch cancel", i, point.id);
                        }
                        writeInputEvent(touchFds[0], time, EV_SYN, SYN_REPORT, 0);
                        previousPoints.clear();
                        break;
                }
                break;
            }
            case Oxide::Tarnish::Tablet:{
                if(!DO_HANDLE_INPUT || tabletFds[0] == -1){
                    break;
                }
                static bool penDown = false;
                static unsigned int lastPressure = 0;
                static int lastX = 0;
                static int lastY = 0;
                static int lastTiltX = 0;
                static int lastTiltY = 0;
                auto data = event.tabletData;
                auto size = Oxide::Tarnish::frameBufferImage().size();
                // Convert from screen size to device size
                timeval time;
                ::gettimeofday(&time, NULL);
                switch(data.type){
                    case Oxide::Tarnish::PenPress:{
                        penDown = true;
                        unsigned int x = ceil(qreal(data.x) * deviceSettings.getWacomWidth() / size.width());
                        unsigned int y = ceil(qreal(data.y) * deviceSettings.getWacomHeight() / size.height());
                        // Raw axis need to be inverted on a rM1
                        int tiltX = Oxide::Math::convertRange(data.tiltY, -90, 90, deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMaxYTilt());
                        int tiltY = Oxide::Math::convertRange(data.tiltX, -90, 90, deviceSettings.getWacomMinXTilt(), deviceSettings.getWacomMaxXTilt());
                        unsigned int pressure = ceil(data.pressure * deviceSettings.getWacomPressure());
                        writeInputEvent(tabletFds[0], time, EV_ABS, ABS_X, x);
                        writeInputEvent(tabletFds[0], time, EV_ABS, ABS_Y, y);
                        writeInputEvent(tabletFds[0], time, EV_ABS, ABS_TILT_X, tiltX);
                        writeInputEvent(tabletFds[0], time, EV_ABS, ABS_TILT_Y, tiltY);
                        writeInputEvent(tabletFds[0], time, EV_ABS, ABS_PRESSURE, pressure);
                        writeInputEvent(tabletFds[0], time, EV_KEY, BTN_TOUCH, 1);
                        switch(data.tool){
                            case Oxide::Tarnish::Eraser:
                                writeInputEvent(tabletFds[0], time, EV_KEY, BTN_TOOL_RUBBER, 1);
                                break;
                            case Oxide::Tarnish::Pen:
                            default:
                                writeInputEvent(tabletFds[0], time, EV_KEY, BTN_TOOL_PEN, 1);
                        }
                        writeInputEvent(tabletFds[0], time, EV_SYN, SYN_REPORT, 0);
                        _DEBUG("tablet press", data.x, data.y, data.tiltX, data.tiltY, data.pressure);
                        break;
                    }
                    case Oxide::Tarnish::PenUpdate:{
                        if(data.x != lastX){
                            unsigned int x = ceil(qreal(data.x) * deviceSettings.getWacomWidth() / size.width());
                            writeInputEvent(tabletFds[0], time, EV_ABS, ABS_X, x);
                        }
                        if(data.y != lastY){
                            unsigned int y = ceil(qreal(data.y) * deviceSettings.getWacomHeight() / size.height());
                            writeInputEvent(tabletFds[0], time, EV_ABS, ABS_Y, y);
                        }
                        if(penDown){
                            if(data.tiltX != lastTiltX){
                                int tiltX = Oxide::Math::convertRange(data.tiltY, -90, 90, deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMaxYTilt());
                                writeInputEvent(tabletFds[0], time, EV_ABS, ABS_TILT_X, tiltX);
                            }
                            if(data.tiltY != lastTiltY){
                                int tiltY = Oxide::Math::convertRange(data.tiltX, -90, 90, deviceSettings.getWacomMinXTilt(), deviceSettings.getWacomMaxXTilt());
                                writeInputEvent(tabletFds[0], time, EV_ABS, ABS_TILT_Y, tiltY);
                            }
                            if(data.pressure != lastPressure){
                                unsigned int pressure = ceil(data.pressure * deviceSettings.getWacomPressure());
                                writeInputEvent(tabletFds[0], time, EV_ABS, ABS_PRESSURE, pressure);
                            }
                        }
                        writeInputEvent(tabletFds[0], time, EV_SYN, SYN_REPORT, 0);
                        _DEBUG("tablet update", data.x, data.y, data.tiltX, data.tiltY, data.pressure);
                        break;
                    }
                    case Oxide::Tarnish::PenRelease:{
                        penDown = false;
                        if(data.x != lastX){
                            unsigned int x = ceil(qreal(data.x) * deviceSettings.getWacomWidth() / size.width());
                            writeInputEvent(tabletFds[0], time, EV_ABS, ABS_X, x);
                        }
                        if(data.y != lastY){
                            unsigned int y = ceil(qreal(data.y) * deviceSettings.getWacomHeight() / size.height());
                            writeInputEvent(tabletFds[0], time, EV_ABS, ABS_Y, y);
                        }
                        if(data.tiltX != lastTiltX){
                            int tiltX = Oxide::Math::convertRange(data.tiltY, -90, 90, deviceSettings.getWacomMinYTilt(), deviceSettings.getWacomMaxYTilt());
                            writeInputEvent(tabletFds[0], time, EV_ABS, ABS_TILT_X, tiltX);
                        }
                        if(data.tiltY != lastTiltY){
                            int tiltY = Oxide::Math::convertRange(data.tiltX, -90, 90, deviceSettings.getWacomMinXTilt(), deviceSettings.getWacomMaxXTilt());
                            writeInputEvent(tabletFds[0], time, EV_ABS, ABS_TILT_Y, tiltY);
                        }
                        if(data.pressure != lastPressure){
                            unsigned int pressure = ceil(data.pressure * deviceSettings.getWacomPressure());
                            writeInputEvent(tabletFds[0], time, EV_ABS, ABS_PRESSURE, pressure);
                        }
                        writeInputEvent(tabletFds[0], time, EV_KEY, BTN_TOUCH, 0);
                        switch(data.tool){
                            case Oxide::Tarnish::Eraser:
                                writeInputEvent(tabletFds[0], time, EV_KEY, BTN_TOOL_RUBBER, 0);
                                break;
                            case Oxide::Tarnish::Pen:
                            default:
                                writeInputEvent(tabletFds[0], time, EV_KEY, BTN_TOOL_PEN, 0);
                        }
                        writeInputEvent(tabletFds[0], time, EV_SYN, SYN_REPORT, 0);
                        _DEBUG("tablet release", data.x, data.y, data.tiltX, data.tiltY, data.pressure);
                        break;
                    }
                    case Oxide::Tarnish::PenEnterProximity:
                    case Oxide::Tarnish::PenLeaveProximity:
                        break;
                }
                lastPressure = data.pressure;
                lastX = data.x;
                lastY = data.y;
                lastTiltX = data.tiltX;
                lastTiltY = data.tiltY;
                break;
            }
            case Oxide::Tarnish::Raise:
                IS_RAISED = true;
                break;
            case Oxide::Tarnish::Lower:
                IS_RAISED = false;
                break;
            case Oxide::Tarnish::Invalid:
            case Oxide::Tarnish::Repaint:
            // TODO - sort out how to manage changes to the framebuffer
            //        being reflected in the app
            case Oxide::Tarnish::Geometry:
            case Oxide::Tarnish::ImageInfo:
            case Oxide::Tarnish::FrameBuffer:
                break;
            default:
                _DEBUG("Recieved unhandled WindowEvent", QString::number(event.type));
                break;
        }
    }
}

int fb_ioctl(unsigned long request, char* ptr){
    switch(request){
        // Look at linux/fb.h and mxcfb.h for more possible request types
        // https://www.kernel.org/doc/html/latest/fb/api.html
        case MXCFB_SEND_UPDATE:{
            _DEBUG("ioctl /dev/fb0 MXCFB_SEND_UPDATE");
            mxcfb_update_data* update = reinterpret_cast<mxcfb_update_data*>(ptr);
            auto region = update->update_region;
            // TODO - Add support for recommending these
            //update->update_mode;
            //update->dither_mode;
            //update->temp;
            QRect rect;
            rect.setLeft(region.left);
            rect.setTop(region.top);
            rect.setWidth(region.width);
            rect.setHeight(region.height);

            auto eventPipe = Oxide::Tarnish::getEventPipe();
            if(eventPipe == nullptr){
                _DEBUG("Unable to get event pipe");
                errno = EBADF;
                return -1;
            }
            Oxide::runLater(eventPipe->thread(), [eventPipe, rect, update]{
                if(!IS_RAISED){
                    Oxide::Tarnish::WindowEvent event;
                    event.type = Oxide::Tarnish::Raise;
                    event.toSocket(eventPipe);
                }
                Oxide::Tarnish::WindowEvent event;
                event.type = Oxide::Tarnish::Repaint;
                event.repaintData = Oxide::Tarnish::RepaintEventArgs{
                    .x = rect.x(),
                    .y = rect.y(),
                    .width = rect.width(),
                    .height = rect.height(),
                    .waveform = (EPFrameBuffer::WaveformMode)update->waveform_mode,
                    .marker = update->update_marker,
                };
                event.toSocket(eventPipe);
            });
            if(deviceSettings.getDeviceType() != Oxide::DeviceSettings::RM2){
                return 0;
            }
            QString path = QFileInfo("/proc/self/exe").canonicalFilePath();
            if(path.isEmpty() || path != "/usr/bin/xochitl"){
                return 0;
            }
            // TODO - notify on rM2 for screensharing
            return 0;
        }
        case MXCFB_WAIT_FOR_UPDATE_COMPLETE:{
            _DEBUG("ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE");
            mxcfb_update_marker_data* update = reinterpret_cast<mxcfb_update_marker_data*>(ptr);
            ClockWatch cz;
            completedMarkerMutex.lock();
            if(update->update_marker > completedMarker){
                completedMarkerMutex.unlock();
                QEventLoop loop;
                QTimer timer;
                QObject::connect(&timer, &QTimer::timeout, [&loop, &timer, update]{
                    if(update->update_marker > completedMarker){
                        timer.stop();
                        loop.quit();
                    }
                });
                timer.start(0);
                loop.exec();
            }else{
                completedMarkerMutex.lock();
            }
            _DEBUG("ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE done:", cz.elapsed());
            return 0;
        }
        case FBIOGET_VSCREENINFO:{
            _DEBUG("ioctl /dev/fb0 FBIOGET_VSCREENINFO");
            auto frameBuffer = Oxide::Tarnish::frameBufferImage();
            auto pixelFormat = frameBuffer.pixelFormat();
            fb_var_screeninfo* screenInfo = reinterpret_cast<fb_var_screeninfo*>(ptr);
            int fd = func_open("/dev/fb0", O_RDONLY, 0);
            if(fd == -1){
                return -1;
            }
            if(func_ioctl(fd, request, ptr) == -1){
                return -1;
            }
            screenInfo->xres = frameBuffer.width();
            screenInfo->yres = frameBuffer.height();
            screenInfo->xres_virtual = frameBuffer.width();
            screenInfo->yres_virtual = frameBuffer.height();
            screenInfo->bits_per_pixel = pixelFormat.bitsPerPixel();
            screenInfo->grayscale = 0;
            // QImage::Format_RGB16
            screenInfo->red.offset = 11;
            screenInfo->red.length = 5;
            screenInfo->red.msb_right = 0;
            screenInfo->green.offset = 5;
            screenInfo->green.length = 6;
            screenInfo->green.msb_right = 0;
            screenInfo->blue.offset = 0;
            screenInfo->blue.length = 5;
            screenInfo->blue.msb_right = 0;
            return 0;
        }
        case FBIOGET_FSCREENINFO:{
            _DEBUG("ioctl /dev/fb0 FBIOGET_FSCREENINFO");
            fb_fix_screeninfo* screeninfo = reinterpret_cast<fb_fix_screeninfo*>(ptr);
            int fd = func_open("/dev/fb0", O_RDONLY, 0);
            if(fd == -1){
                // Failed to get the actual information, try to dummy our way with our own
                constexpr char fb_id[] = "mxcfb";
                memcpy(screeninfo->id, fb_id, sizeof(fb_id));
            }else if(func_ioctl(fd, request, ptr) == -1){
                return -1;
            }
            auto frameBuffer = Oxide::Tarnish::frameBufferImage();
            screeninfo->smem_len = frameBuffer.sizeInBytes();
            screeninfo->smem_start = (unsigned long)Oxide::Tarnish::frameBuffer();
            screeninfo->line_length = frameBuffer.bytesPerLine();
            return 0;
        }
        case FBIOPUT_VSCREENINFO:{
            _DEBUG("ioctl /dev/fb0 FBIOPUT_VSCREENINFO");
            // TODO - Explore allowing some screen info updating
//            fb_var_screeninfo* screenInfo = reinterpret_cast<fb_fix_screeninfo*>(ptr);
//            auto eventPipe = Oxide::Tarnish::getEventPipe();
//            if(eventPipe == nullptr){
//                _DEBUG("Unable to get event pipe");
//                errno = EBADF;
//                return -1;
//            }
//            Oxide::Tarnish::WindowEvent geometryEvent;
//            geometryEvent.type = Oxide::Tarnish::Geometry;
//            geometryEvent.geometryData = Oxide::Tarnish::GeometryEventArgs{
//                .x = 0,
//                .y = 0,
//                .width = screenInfo->xres_virtual,
//                .height = screenInfo->yres_virtual,
//            };
//            geometryEvent.toSocket(eventPipe);
//            QEventLoop loop;
//            QTimer timer;
//            QObject::connect(&timer, &QTimer::timeout, [&loop, &timer]{
//                //Do whatever is needed here to wait for changes
//                timer.stop();
//                loop.quit();
//            });
//            timer.start(0);
//            loop.exec();
            return 0;
        }
        case MXCFB_SET_AUTO_UPDATE_MODE:
            _DEBUG("ioctl /dev/fb0 MXCFB_SET_AUTO_UPDATE_MODE");
            return 0;
        case MXCFB_SET_UPDATE_SCHEME:
            _DEBUG("ioctl /dev/fb0 MXCFB_SET_UPDATE_SCHEME");
            return 0;
        case MXCFB_ENABLE_EPDC_ACCESS:
            _DEBUG("ioctl /dev/fb0 MXCFB_ENABLE_EPDC_ACCESS");
            return 0;
        case MXCFB_DISABLE_EPDC_ACCESS:
            _DEBUG("ioctl /dev/fb0 MXCFB_DISABLE_EPDC_ACCESS");
            return 0;
        default:
            _WARN(
                "UNHANDLED FB IOCTL ",
                QString::number(_IOC_DIR(request)),
                (char)_IOC_TYPE(request),
                QString::number(_IOC_NR(request), 16),
                QString::number(_IOC_SIZE(request)),
                QString::number(request)
            );
            return 0;
    }
}

int evdev_ioctl(const char* path, unsigned long request, char* ptr){
    static auto func_ioctl = (int(*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
    switch(request){
        case EVIOCGID:
        case EVIOCGREP:
        case EVIOCGVERSION:
        case EVIOCGKEYCODE:
        case EVIOCGKEYCODE_V2:
            int fd = func_open(path, O_RDONLY, 0);
            auto res = func_ioctl(fd, request, ptr);
            func_close(fd);
            return res;
    }
    unsigned int cmd = request;
    switch(EVIOC_MASK_SIZE(cmd)){
        case EVIOCGPROP(0):
        case EVIOCGMTSLOTS(0):
        case EVIOCGKEY(0):
        case EVIOCGLED(0):
        case EVIOCGSND(0):
        case EVIOCGSW(0):
        case EVIOCGNAME(0):
        case EVIOCGPHYS(0):
        case EVIOCGUNIQ(0):
        case EVIOC_MASK_SIZE(EVIOCSFF):
            int fd = func_open(path, O_RDONLY, 0);
            auto res = func_ioctl(fd, request, ptr);
            func_close(fd);
            return res;
    }

    if(_IOC_DIR(cmd) == _IOC_READ){
        if((_IOC_NR(cmd) & ~EV_MAX) == _IOC_NR(EVIOCGBIT(0, 0))){
            int fd = func_open(path, O_RDONLY, 0);
            auto res = func_ioctl(fd, request, ptr);
            func_close(fd);
            return res;
        }else if((_IOC_NR(cmd) & ~ABS_MAX) == _IOC_NR(EVIOCGABS(0))){
            int fd = func_open(path, O_RDONLY, 0);
            auto res = func_ioctl(fd, request, ptr);
            func_close(fd);
            return res;
        }
    }
    return -2;
}

int touch_ioctl(unsigned long request, char* ptr){
    switch(request){
        case EVIOCGRAB: return 0;
        case EVIOCREVOKE: return 0;
        default:
            auto res = evdev_ioctl(deviceSettings.getTouchDevicePath(), request, ptr);
            if(res == -2){
                _WARN(
                    "UNHANDLED TOUCH IOCTL ",
                    QString::number(_IOC_DIR(request)),
                    (char)_IOC_TYPE(request),
                    QString::number(_IOC_NR(request), 16),
                    QString::number(_IOC_SIZE(request)),
                    QString::number(request)
                );
                return -1;
            }
            return res;
    }
}

int tablet_ioctl(unsigned long request, char* ptr){
    switch(request){
        case EVIOCGRAB: return 0;
        case EVIOCREVOKE: return 0;
        default:
            auto res = evdev_ioctl(deviceSettings.getWacomDevicePath(), request, ptr);
            if(res == -2){
                _WARN(
                    "UNHANDLED TABLET IOCTL ",
                    QString::number(_IOC_DIR(request)),
                    (char)_IOC_TYPE(request),
                    QString::number(_IOC_NR(request), 16),
                    QString::number(_IOC_SIZE(request)),
                    QString::number(request)
                );
                return -1;
            }
            return res;
    }
}

int key_ioctl(unsigned long request, char* ptr){
    switch(request){
        case EVIOCGRAB: return 0;
        case EVIOCREVOKE: return 0;
        default:
            auto res = evdev_ioctl(deviceSettings.getButtonsDevicePath(), request, ptr);
            if(res == -2){
                _WARN(
                    "UNHANDLED KEY IOCTL ",
                    QString::number(_IOC_DIR(request)),
                    (char)_IOC_TYPE(request),
                    QString::number(_IOC_NR(request), 16),
                    QString::number(_IOC_SIZE(request)),
                    QString::number(request)
                );
                return -1;
            }
            return res;
    }
}

void connectEventPipe(){
    auto eventPipe = Oxide::Tarnish::getEventPipe();
    if(eventPipe == nullptr){
        qFatal("Could not get event pipe");
    }
    QObject::connect(eventPipe, &QLocalSocket::readyRead, eventPipe, &__read_event_pipe, Qt::DirectConnection);
    QObject::connect(eventPipe, &QLocalSocket::disconnected, []{ kill(getpid(), SIGTERM); });
    _DEBUG("Connected to event pipe");
}

int open_from_tarnish(const char* pathname){
    if(!IS_INITIALIZED || IS_OPENING){
        return -2;
    }
    IS_OPENING = true;
    int res = -2;
    if(DO_HANDLE_INPUT){
        if(strcmp(pathname, deviceSettings.getTouchDevicePath()) == 0){
            if(touchFds[1] == -1){
                _DEBUG("Opening touch device socket");
                // TODO - handle events written to touchFds[1] by program
                res = ::socketpair(AF_UNIX, SOCK_STREAM, 0, touchFds);
            }
            if(res != -1 && touchFds[1] != -1){
                res = touchFds[1];
            }
        }else if(strcmp(pathname, deviceSettings.getWacomDevicePath()) == 0){
            if(tabletFds[1] == -1){
                _DEBUG("Opening tablet device socket");
                // TODO - handle events written to touchFds[1] by program
                res = ::socketpair(AF_UNIX, SOCK_STREAM, 0, tabletFds);
            }
            if(res != -1 && tabletFds[1] != -1){
                res = tabletFds[1];
            }
        }else if(strcmp(pathname, deviceSettings.getButtonsDevicePath()) == 0){
            if(keyFds[1] == -1){
                _DEBUG("Opening buttons device socket");
                // TODO - handle events written to touchFds[1] by program
                res = ::socketpair(AF_UNIX, SOCK_STREAM, 0, keyFds);
            }
            if(res != -1 && keyFds[1] != -1){
                res = keyFds[1];
            }
        }
    }
    if(DO_HANDLE_FB && strcmp(pathname, "/dev/fb0") == 0){
        res = fbFd = Oxide::Tarnish::getFrameBufferFd();
    }
    if(!PIPE_OPENED && res != -2){
        PIPE_OPENED = true;
        if(QCoreApplication::startingUp()){
            QTimer::singleShot(0, []{ connectEventPipe(); });
        }else{
            connectEventPipe();
        }
    }
    IS_OPENING = false;
    if(res == -1){
        errno = EIO;
    }
    return res;
}

extern "C" {
    __attribute__((visibility("default")))
    int open64(const char* pathname, int flags, mode_t mode = 0){
        static const auto func_open64 = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");
        if(!IS_INITIALIZED){
            return func_open64(pathname, flags, mode);
        }
        _DEBUG("open64", pathname);
        int fd = open_from_tarnish(pathname);
        if(fd == -2){
            fd = func_open64(pathname, flags, mode);
        }
        _DEBUG("opened", pathname, "with fd", fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int openat(int dirfd, const char* pathname, int flags, mode_t mode = 0){
        static const auto func_openat = (int(*)(int, const char*, int, mode_t))dlsym(RTLD_NEXT, "openat");
        if(!IS_INITIALIZED){
            return func_openat(dirfd, pathname, flags, mode);
        }
        _DEBUG("openat", pathname);
        int fd = open_from_tarnish(pathname);
        if(fd == -2){
            DIR* save = opendir(".");
            fchdir(dirfd);
            char path[PATH_MAX+1];
            getcwd(path, PATH_MAX);
            fchdir(::dirfd(save));
            closedir(save);
            fd = open_from_tarnish(QString("%1/%2").arg(path, pathname).toStdString().c_str());
        }
        if(fd == -2){
            fd = func_openat(dirfd, pathname, flags, mode);
        }
        _DEBUG("opened", pathname, "with fd", fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int open(const char* pathname, int flags, mode_t mode = 0){
        if(!IS_INITIALIZED){
            return func_open(pathname, flags, mode);;
        }
        _DEBUG("open", pathname);
        int fd = open_from_tarnish(pathname);
        if(fd == -2){
            fd = func_open(pathname, flags, mode);
        }
        _DEBUG("opened", pathname, "with fd", fd);
        return fd;
    }

    __attribute__((visibility("default")))
    int close(int fd){
        if(IS_INITIALIZED){
            _DEBUG("close", fd);
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                // Maybe actually close it?
                return 0;
            }
            if(DO_HANDLE_INPUT){
                if(touchFds[1] != -1 && fd == touchFds[1]){
                    // Maybe actually close it?
                    return 0;
                }
                if(tabletFds[1] != -1 && fd == tabletFds[1]){
                    // Maybe actually close it?
                    return 0;
                }
                if(keyFds[1] != -1 && fd == keyFds[1]){
                    // Maybe actually close it?
                    return 0;
                }
            }
        }
        static const auto func_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
        return func_close(fd);
    }

    __attribute__((visibility("default")))
    int ioctl(int fd, unsigned long request, char* ptr){
        if(IS_INITIALIZED){
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                return fb_ioctl(request, ptr);
            }
            if(DO_HANDLE_INPUT){
                if(touchFds[1] != -1 && fd == touchFds[1]){
                    return touch_ioctl(request, ptr);
                }
                if(tabletFds[1] != -1 && fd == tabletFds[1]){
                    return tablet_ioctl(request, ptr);
                }
                if(keyFds[1] != -1 && fd == keyFds[1]){
                    return key_ioctl(request, ptr);
                }
            }
        }
        return func_ioctl(fd, request, ptr);
    }

    __attribute__((visibility("default")))
    ssize_t _write(int fd, const void* buf, size_t n){
        if(IS_INITIALIZED){
            if(fd < 3){
                // No need to debug stdout/stderr writes
                _DEBUG("write", fd, n);
            }
            if(DO_HANDLE_FB && fbFd != -1 && fd == fbFd){
                Oxide::Tarnish::lockFrameBuffer();
                auto res = func_write(fd, buf, n);
                Oxide::Tarnish::unlockFrameBuffer();
                return res;
            }
            if(DO_HANDLE_INPUT){
                if(touchFds[1] != -1 && fd == touchFds[1]){
                    errno = EBADF;
                    return -1;
                }
                if(tabletFds[1] != -1 && fd == tabletFds[1]){
                    errno = EBADF;
                    return -1;
                }
                if(keyFds[1] != -1 && fd == keyFds[1]){
                    errno = EBADF;
                    return -1;
                }
            }
        }
        return func_write(fd, buf, n);
    }
    __asm__(".symver _write, write@GLIBC_2.4");

    void __attribute__ ((constructor)) init(void);
    void init(void){
        func_write = (ssize_t(*)(int, const void*, size_t))dlvsym(RTLD_NEXT, "write", "GLIBC_2.4");
        func_open = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");
        func_ioctl = (int(*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
        func_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
    }

    static void _libhook_init() __attribute__((constructor));
    static void _libhook_init(){
        DEBUG_LOGGING = qEnvironmentVariableIsSet("OXIDE_PRELOAD_DEBUG");
        DO_HANDLE_FB = !qEnvironmentVariableIsSet("OXIDE_PRELOAD_EXPOSE_FB");
        DO_HANDLE_INPUT = !qEnvironmentVariableIsSet("OXIDE_PRELOAD_EXPOSE_INPUT");
        QString path = QFileInfo("/proc/self/exe").canonicalFilePath();
        IS_XOCHITL = path == "/usr/bin/xochitl";
        IS_FBDEPTH = path.endsWith("fbdepth");
        IS_LOADABLE_APP = IS_XOCHITL || path.isEmpty() || path.startsWith("/opt") || path.startsWith("/home");
        if(IS_LOADABLE_APP){
            if(!__sigaction_connect(SIGSEGV)){
                _CRIT("Failed to connect SIGSEGV:", strerror(errno));
                DEBUG_LOGGING = true;
                return;
            }
            if(!__sigaction_connect(SIGINT)){
                _CRIT("Failed to connect SIGINT:", strerror(errno));
                DEBUG_LOGGING = true;
                return;
            }
            if(!__sigaction_connect(SIGTERM)){
                _CRIT("Failed to connect SIGTERM:", strerror(errno));
                DEBUG_LOGGING = true;
                return;
            }
            if(!__sigaction_connect(SIGABRT)){
                _CRIT("Failed to connect SIGABRT:", strerror(errno));
                DEBUG_LOGGING = true;
                return;
            }
            bool doConnect = !qEnvironmentVariableIsSet("OXIDE_PRELOAD");
            if(!doConnect){
                bool ok;
                auto epgid = qEnvironmentVariableIntValue("OXIDE_PRELOAD", &ok);
                doConnect = !ok || epgid != getpgrp();
            }
            Oxide::Tarnish::setFrameBufferFormat(QImage::Format_RGB16);
            if(doConnect){
                auto pid = QString::number(getpid());
                _DEBUG("Connecting", pid, "to tarnish");
                if(qEnvironmentVariableIsSet("OXIDE_PRELOAD_NO_QAPP")){
                    QTimer::singleShot(0, [pid]{
                        auto socket = Oxide::Tarnish::getSocket();
                        if(socket == nullptr){
                            DEBUG_LOGGING = true;
                            _CRIT("Failed to connect", pid, "to tarnish");
                            return;
                        }
                        QObject::connect(socket, &QLocalSocket::disconnected, []{ kill(getpid(), SIGTERM); });
                    });
                }else{
                    auto socket = Oxide::Tarnish::getSocket();
                    if(socket == nullptr){
                        DEBUG_LOGGING = true;
                        _CRIT("Failed to connect", pid, "to tarnish");
                        return;
                    }
                    QObject::connect(socket, &QLocalSocket::disconnected, []{ kill(getpid(), SIGTERM); });
                }
                _DEBUG("Connected", pid, "to tarnish");
            }
        }
        FAILED_INIT = false;
        IS_INITIALIZED = IS_LOADABLE_APP;
        qputenv("OXIDE_PRELOAD", QByteArray::number(getpgrp()));
    }

    __attribute__((visibility("default")))
    int __libc_start_main(
        int(*_main)(int, char**, char**),
        int argc,
        char** argv,
        int(*init)(int, char**, char**),
        void(*fini)(void),
        void(*rtld_fini)(void),
        void* stack_end
    ){
        if(FAILED_INIT){
            return EXIT_FAILURE;
        }
        auto func_main = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");
        if(!IS_LOADABLE_APP){
            return func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
        }
        if(qEnvironmentVariableIsSet("OXIDE_PRELOAD_NO_QAPP")){
            deviceSettings.setupQtEnvironment(Oxide::DeviceSettings::Default);
            auto res = func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
            _DEBUG("Exit code:", QString::number(res));
            Oxide::Tarnish::disconnect();
            return res;
        }
        QCoreApplication app(argc, argv);
        QThread thread;
        thread.setObjectName("QCoreApplication");
        QObject::connect(&thread, &QThread::started, &app, [&]{
            auto res = func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
            _DEBUG("Exit code:", QString::number(res));
            Oxide::Tarnish::disconnect();
            app.exit(res);
        }, Qt::DirectConnection);
        thread.start();
        return app.exec();
    }

    __attribute__((visibility("default")))
    [[noreturn]] void exit(int status){
        _DEBUG("exit", status);
        Oxide::Tarnish::disconnect();
        qApp->exit(status);
        ((void(*)(int))dlsym(RTLD_NEXT, "exit"))(status);
        while(1);; // Make compiler happy about noreturn
    }

    __attribute__((visibility("default")))
    [[noreturn]] void abort() noexcept{
        _DEBUG("abort");
        Oxide::Tarnish::disconnect();
        qApp->exit(SIGABRT);
        ((void(*)(void) noexcept)dlsym(RTLD_NEXT, "abort"))();
        while(1);; // Make compiler happy about noreturn
    }

    __attribute__((visibility("default")))
    [[noreturn]] void terminate() noexcept{
        _DEBUG("terminate");
        Oxide::Tarnish::disconnect();
        qApp->exit(SIGTERM);
        ((void(*)(void) noexcept)dlsym(RTLD_NEXT, "terminate"))();
        while(1);; // Make compiler happy about noreturn
    }

    __attribute__((visibility("default")))
    [[noreturn]] void quick_exit(int exit_code) noexcept{
        _DEBUG("quick_exit", exit_code);
        Oxide::Tarnish::disconnect();
        qApp->exit(exit_code);
        ((void(*)(int) noexcept)dlsym(RTLD_NEXT, "quick_exit"))(exit_code);
        while(1);; // Make compiler happy about noreturn
    }
}
