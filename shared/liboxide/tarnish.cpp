#include "tarnish.h"
#include "meta.h"
#include "qt.h"
#include "liboxide.h"

#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/file.h>
#include <pthread.h>
#include <QDBusConnection>
#include <QImage>
#include <QtCore/private/qthread_p.h>
#include <QAtomicInteger>

using namespace Oxide::Tarnish;

QSharedPointer<codes::eeems::oxide1::General> api_general;
QSharedPointer<codes::eeems::oxide1::Power> api_power;
QSharedPointer<codes::eeems::oxide1::Wifi> api_wifi;
QSharedPointer<codes::eeems::oxide1::Screen> api_screen;
QSharedPointer<codes::eeems::oxide1::Apps> api_apps;
QSharedPointer<codes::eeems::oxide1::System> api_system;
QSharedPointer<codes::eeems::oxide1::Notifications> api_notification;
QSharedPointer<codes::eeems::oxide1::Gui> api_gui;
QSharedPointer<codes::eeems::oxide1::Window> window;
QRect fbGeometry;
qulonglong fbLineSize = 0;
QImage::Format fbFormat = QImage::Format_Invalid;
QImage::Format fbRequestedFormat = DEFAULT_IMAGE_FORMAT;
uchar* fbData = nullptr;
QFile fbFile;
QMutex fbMutex;
QDaemonThread eventThread;
QLocalSocket childSocket;
QLocalSocket eventSocket;
QDataStream eventStream;
QAtomicInteger<unsigned int> repaintMarker(1);

using namespace Oxide;

// TODO - Get liboxide to automate journalctl logging
//        https://doc.qt.io/qt-5/qtglobal.html#qInstallMessageHandler

bool verifyConnection(){
    if(api_general.isNull()){
        return false;
    }
    if(!api_general->isValid()){
        api_general.clear();
        return false;
    }
    return true;
}

bool setupFbFd(int fd){
    fbFile.close();
    if(!fbFile.open(fd, QFile::ReadWrite, QFile::AutoCloseHandle)){
        O_WARNING("Unable to open framebuffer QFile:" << fbFile.errorString());
        if(::close(fd) == -1){
            O_WARNING("Unable close fd:" << strerror(errno));
        }
        return false;
    }
    fbGeometry = window->geometry();
    fbLineSize = window->bytesPerLine();
    fbFormat = (QImage::Format)window->format();
    O_DEBUG("FrameBuffer" << fbGeometry << fbLineSize << fbFormat << fbFile.size() - sizeof(std::mutex));
    return true;
}
namespace Oxide::Tarnish {
    qsizetype RepaintEventArgs::size(){ return (sizeof(int) * 4) + sizeof(quint64) + sizeof(unsigned int); }

    QRect RepaintEventArgs::geometry() const{ return QRect(x, y, width, height); }

    QByteArray& operator>>(QByteArray& l, RepaintEventArgs& r){
        Q_ASSERT_X(
            l.size() >= RepaintEventArgs::size(), "QByteArray >> RepaintEventArgs",
            QString("Not enough data available: %1 != %2").arg(l.size()).arg(RepaintEventArgs::size()).toStdString().c_str()
        );
        quint64 waveform;
        l >> r.x >> r.y >> r.width >> r.height >> waveform >> r.marker;
        r.waveform = (EPFrameBuffer::WaveformMode)waveform;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, const RepaintEventArgs& r){
        QByteArray a;
        a << r.x
            << r.y
            << r.width
            << r.height
            << (quint64)r.waveform
            << r.marker;
        Q_ASSERT_X(
            a.size() == RepaintEventArgs::size(), "QByteArray << RepaintEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(RepaintEventArgs::size()).toStdString().c_str()
        );
        l += a;
        return l;
    }

    qsizetype GeometryEventArgs::size(){ return sizeof(int) * 5; }

    QRect GeometryEventArgs::geometry() const{ return QRect(x, y, width, height); }

    QByteArray& operator>>(QByteArray& l, GeometryEventArgs& r){
        Q_ASSERT_X(
            l.size() >= GeometryEventArgs::size(), "QByteArray >> GeometryEventArgs",
            QString("Not enough data available: %1 != %2").arg(l.size()).arg(GeometryEventArgs::size()).toStdString().c_str()
        );
        l >> r.x >> r.y >> r.width >> r.height >> r.z;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, const GeometryEventArgs& r){
        QByteArray a;
        a << r.x
            << r.y
            << r.width
            << r.height
            << r.z;
        Q_ASSERT_X(
            a.size() == GeometryEventArgs::size(), "QByteArray << GeometryEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(GeometryEventArgs::size()).toStdString().c_str()
        );
        l += a;
        return l;
    }

    qsizetype ImageInfoEventArgs::size(){ return (sizeof(qulonglong) * 2) + sizeof(int); }

    QByteArray& operator>>(QByteArray& l, ImageInfoEventArgs& r){
        Q_ASSERT_X(
            l.size() >= ImageInfoEventArgs::size(), "QByteArray >> ImageInfoEventArgs",
            QString("Not enough data available: %1 != %2").arg(l.size()).arg(ImageInfoEventArgs::size()).toStdString().c_str()
        );
        int format;
        l >> r.sizeInBytes >> r.bytesPerLine >> format;
        r.format = (QImage::Format)format;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, const ImageInfoEventArgs& r){
        QByteArray a;
        a << r.sizeInBytes
            << r.bytesPerLine
            << (int)r.format;
        Q_ASSERT_X(
            a.size() == ImageInfoEventArgs::size(), "QByteArray << ImageInfoEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(ImageInfoEventArgs::size()).toStdString().c_str()
        );
        l += a;
        return l;
    }

    qsizetype WaitForPaintEventArgs::size(){ return sizeof(unsigned int); }

    QByteArray& operator>>(QByteArray& l, WaitForPaintEventArgs& r){
        Q_ASSERT_X(
            l.size() >= WaitForPaintEventArgs::size(), "QByteArray >> WaitForPaintEventArgs",
            QString("Not enough data available: %1 != %2").arg(l.size()).arg(WaitForPaintEventArgs::size()).toStdString().c_str()
        );
        l >> r.marker;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, const WaitForPaintEventArgs& r){
        QByteArray a;
        a << r.marker;
        Q_ASSERT_X(
            a.size() == WaitForPaintEventArgs::size(), "QByteArray << WaitForPaintEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(WaitForPaintEventArgs::size()).toStdString().c_str()
        );
        l += a;
        return l;
    }

    qsizetype KeyEventArgs::size(){ return sizeof(KeyEventType) + (sizeof(unsigned int) * 3); }

    QByteArray& operator>>(QByteArray& l, KeyEventArgs& r){
        Q_ASSERT_X(
            l.size() >= KeyEventArgs::size(), "QByteArray >> KeyEventArgs",
            QString("Not enough data available: %1 != %2").arg(l.size()).arg(KeyEventArgs::size()).toStdString().c_str()
        );
        unsigned short type;
        l >> type >> r.code >> r.unicode >> r.scanCode;
        r.type = (KeyEventType)type;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, const KeyEventArgs& r){
        QByteArray a;
        a << (unsigned short)r.type << r.code << r.unicode << r.scanCode;
        Q_ASSERT_X(
            a.size() == KeyEventArgs::size(), "QByteArray << KeyEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(KeyEventArgs::size()).toStdString().c_str()
        );
        l += a;
        return l;
    }

    QByteArray& operator>>(QByteArray& l, TouchEventPoint& r){
        Q_ASSERT_X(
            l.size() >= TouchEventArgs::itemSize(), "QByteArray >> TouchEventPoint",
            QString("Not enough data available: %1 != %2")
                .arg(l.size())
                .arg(TouchEventArgs::itemSize())
                .toStdString()
                .c_str()
        );
        unsigned short state, tool;
        l >> r.id >> state >> r.x >> r.y >> r.width >> r.height >> tool >> r.pressure >> r.rotation;
        r.tool = (TouchEventTool)tool;
        r.state = (TouchEventPointState)state;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, const TouchEventPoint& r){
        QByteArray a;
        a << r.id << (unsigned short)r.state << r.x << r.y << r.width << r.height << (unsigned short)r.tool << r.pressure << r.rotation;
        Q_ASSERT_X(
            a.size() == TouchEventArgs::itemSize(), "QByteArray << TouchEventPoint",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(TouchEventArgs::itemSize()).toStdString().c_str()
        );
        l += a;
        return l;
    }

    qsizetype TouchEventArgs::size(){ return sizeof(TouchEventType) + sizeof(unsigned int); }

    qsizetype TouchEventArgs::itemSize(){ return sizeof(int) + (sizeof(double) * 6) + sizeof(TouchEventTool) + sizeof(TouchEventPointState); }

    qsizetype TouchEventArgs::realSize()const { return size() + (points.size() * itemSize()); }

    QRectF TouchEventPoint::geometry() const{ return QRectF(x, y, width, height); }

    QPointF TouchEventPoint::point() const{ return QPointF(x, y); }

    QPointF TouchEventPoint::originalPoint() const{
        int angle = 0;
        bool invertx = false;
        bool inverty = false;
        for(QString arg : QString::fromLatin1(deviceSettings.getTouchEnvSetting()).split(QLatin1Char(':'))){
            if(arg.startsWith("rotate")){
                bool ok;
                uint val = arg.section(QLatin1Char('='), 1, 1).toUInt(&ok);
                if(!ok){
                    continue;
                }
                switch(val){
                    case 90:
                    case 180:
                    case 270:
                        angle = -val;
                        break;
                    default:
                        break;
                }
            }else if(arg == "invertx"){
                invertx = true;
            }else if(arg == "inverty"){
                inverty = true;
            }
        }
        QTransform rotate;
        if(angle){
            rotate = QTransform::fromTranslate(0.5, 0.5).rotate(angle).translate(-0.5, -0.5);
        }
        if(invertx){
            rotate *= QTransform::fromTranslate(0.5, 0.5).scale(-1.0, 1.0).translate(-0.5, -0.5);
        }
        if(inverty){
            rotate *= QTransform::fromTranslate(0.5, 0.5).scale(1.0, -1.0).translate(-0.5, -0.5);
        }
        return rotate.map(point());
    }

    QSizeF TouchEventPoint::size() const{ return QSizeF(width, height); }

    QByteArray& operator>>(QByteArray& l, TouchEventArgs& r){
        Q_ASSERT_X(
            l.size() >= TouchEventArgs::size(), "QByteArray >> TouchEventArgs",
            QString("Not enough data available: %1 != %2").arg(l.size()).arg(TouchEventArgs::size()).toStdString().c_str()
        );
        unsigned short type;
        unsigned int count;
        l >> count >> type;
        r.type = (TouchEventType)type;
        Q_ASSERT_X(
            (unsigned int)l.size() >= (TouchEventArgs::itemSize() * count), "QByteArray >> TouchEventArgs",
            QString("Not enough data available for %1 items: %2 != %3")
                .arg(count)
                .arg(l.size())
                .arg(TouchEventArgs::itemSize() * count)
                .toStdString()
                .c_str()
        );
        r.points.reserve(count);
        for(int i = 0; (unsigned int)i < count; i++){
            TouchEventPoint p;
            l >> p;
            r.points.push_back(p);
        }
        return l;
    }

    QByteArray& operator<<(QByteArray& l, const TouchEventArgs& r){
        QByteArray a;
        a << (unsigned int)r.points.size() << (unsigned short)r.type;
        Q_ASSERT_X(
            a.size() == TouchEventArgs::size(), "QByteArray << TouchEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(TouchEventArgs::size()).toStdString().c_str()
        );
        for(const auto& point : r.points){
            a << point;
        }
        Q_ASSERT_X(
            a.size() == r.realSize(), "QByteArray << TouchEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(r.realSize()).toStdString().c_str()
        );
        l += a;
        return l;
    }

    qsizetype TabletEventArgs::size(){ return (sizeof(unsigned short) * 2) + (sizeof(int) * 4) + sizeof(double); }

    QPoint TabletEventArgs::point() const{ return QPoint(x, y); }

    QByteArray& operator>>(QByteArray& l, TabletEventArgs& r){
        Q_ASSERT_X(
            l.size() >= TabletEventArgs::size(), "QByteArray << TabletEventArgs",
            QString("Not enough data available: %1 != %2").arg(l.size()).arg(TabletEventArgs::size()).toStdString().c_str()
        );
        unsigned short type, tool;
        l >> type >> tool >> r.x >> r.y >> r.pressure >> r.tiltX >> r.tiltY;
        r.type = (TabletEventType)type;
        r.tool = (TabletEventTool)tool;
        return l;
    }

    QByteArray& operator<<(QByteArray& l, const TabletEventArgs& r){
        QByteArray a;
        a << (unsigned short)r.type
            << (unsigned short)r.tool
            << r.x
            << r.y
            << r.pressure
            << r.tiltX
            << r.tiltY;
        Q_ASSERT_X(
            a.size() == TabletEventArgs::size(), "QByteArray << TabletEventArgs",
            QString("Resulting size is incorrect: %1 != %2").arg(a.size()).arg(TabletEventArgs::size()).toStdString().c_str()
        );
        l += a;
        return l;
    }

    QDebug operator<<(QDebug debug, const WindowEventType& type){
        QDebugStateSaver saver(debug);
        Q_UNUSED(saver);
        switch(type){
            case Repaint: debug.nospace() << "Repaint"; break;
            case Geometry: debug.nospace() << "Geometry"; break;
            case ImageInfo: debug.nospace() << "ImageInfo"; break;
            case WaitForPaint: debug.nospace() << "WaitForPaint"; break;
            case Raise: debug.nospace() << "Raise"; break;
            case Lower: debug.nospace() << "Lower"; break;
            case Close: debug.nospace() << "Close"; break;
            case FrameBuffer: debug.nospace() << "FrameBuffer"; break;
            case Ping: debug.nospace() << "Ping"; break;
            case Key: debug.nospace() << "Key"; break;
            case Touch: debug.nospace() << "Touch"; break;
            case Tablet: debug.nospace() << "Tablet"; break;
            case Invalid:
            default:
                 debug.nospace() << "Invalid";
        }
        return debug;
    }

    WindowEvent WindowEvent::fromSocket(QIODevice* socket){
        QMutexLocker locker(&m_readMutex);
        Q_UNUSED(locker);
        O_DEBUG("Reading event from socket");
        WindowEvent event;
        if(socket->atEnd()){
            O_WARNING("Failed to read event from socket: There are no events waiting");
            return event;
        }
        qsizetype size = sizeof(unsigned short);
        auto data = socket->read(size);
        if(data.size() != size){
            O_WARNING("Failed to read event from socket: Expected" << size << "bytes but instead recieved" << data.size() << "bytes");
            return event;
        }
        unsigned short type;
        data >> type;
        switch((WindowEventType)type){
            case Repaint:{
                auto data = dispatchToThread<QByteArray>(socket->thread(), [socket, &event]{ return socket->read(event.repaintData.size()); });
                data >> event.repaintData;
                break;
            }
            case Geometry:{
                auto data = dispatchToThread<QByteArray>(socket->thread(), [socket, &event]{ return socket->read(event.geometryData.size()); });
                data >> event.geometryData;
                break;
            }
            case ImageInfo:{
                auto data = dispatchToThread<QByteArray>(socket->thread(), [socket, &event]{ return socket->read(event.imageInfoData.size()); });
                data >> event.imageInfoData;
                break;
            }
            case WaitForPaint:{
                auto data = dispatchToThread<QByteArray>(socket->thread(), [socket, &event]{ return socket->read(event.waitForPaintData.size()); });
                data >> event.waitForPaintData;
                break;
            }
            case Key:{
                auto data = dispatchToThread<QByteArray>(socket->thread(), [socket, &event]{ return socket->read(event.keyData.size()); });
                data >> event.keyData;
                break;
            }
            case Touch:
                dispatchToThread(socket->thread(), [socket, &event]{
                    auto data = socket->read(event.touchData.size());
                    unsigned int size;
                    auto d = data.left(sizeof(unsigned int));
                    d >> size;
                    data.append(socket->read(TouchEventArgs::itemSize() * size));
                    data >> event.touchData;
                });
                break;
            case Tablet:{
                auto data = dispatchToThread<QByteArray>(socket->thread(), [socket, &event]{ return socket->read(event.tabletData.size()); });
                data >> event.tabletData;
                break;
            }
            case Raise:
            case Lower:
            case Close:
            case FrameBuffer:
            case Ping:
            case Invalid:
                break;
            default:
                O_WARNING("Unknown event type:" << type);
                // TODO - skip to end and send some sort of "oops, I lost track" event?
                return event;
        }
        event.type = (WindowEventType)type;
        O_DEBUG("Read event from socket" << event);
        return event;
    }

    bool WindowEvent::toSocket(QIODevice* socket){
        QMutexLocker locker(&m_writeMutex);
        Q_UNUSED(locker);
        O_DEBUG("Writing event to socket" << this);
        if(!socket->isOpen()){
            O_WARNING("Socket is not open");
            return false;
        }
        if(!socket->isWritable()){
            O_WARNING("Socket is not writable");
            return false;
        }
        if(!isValid()){
            O_WARNING("Invalid WindowEvent");
            return false;
        }
        QByteArray data;
        data << (unsigned short)type;
        switch(type){
            case Repaint:
                data << repaintData;
                break;
            case Geometry:
                data << geometryData;
                break;
            case ImageInfo:
                data << imageInfoData;
                break;
            case WaitForPaint:
                data << waitForPaintData;
                break;
            case Key:
                data << keyData;
                break;
            case Touch:
                data << touchData;
                break;
            case Tablet:
                data << tabletData;
                break;
            case Raise:
            case Lower:
            case Close:
            case FrameBuffer:
            case Ping:
                break;
            case Invalid:
            default:
                O_WARNING("Unknown event type:" << type)
                return false;
        }
        auto res = dispatchToThread<qint64>(socket->thread(), [socket, data]{ return socket->write(data); });
        if(res < data.size()){
            O_WARNING("Expected to write" << data.size() << "bytes but only wrote" << res << "bytes");
            return false;
        }
        return true;
    }

    WindowEvent::WindowEvent() : type{Invalid}{}

    WindowEvent::WindowEvent(const WindowEvent& event)
    : type{event.type},
      repaintData{event.repaintData},
      geometryData{event.geometryData},
      imageInfoData{event.imageInfoData},
      keyData{event.keyData},
      touchData{event.touchData}
    {}

    WindowEvent::~WindowEvent(){}

    bool WindowEvent::isValid(){
       switch(type){
            case Repaint:
            case Geometry:
            case ImageInfo:
            case WaitForPaint:
            case Raise:
            case Lower:
            case Close:
            case FrameBuffer:
            case Ping:
            case Key:
            case Touch:
           case Tablet:
                return true;
            default:
                return false;
       }
    }

    QMutex WindowEvent::m_writeMutex;

    QMutex WindowEvent::m_readMutex;

    QDebug operator<<(QDebug debug, const WindowEvent& event){
        QDebugStateSaver saver(debug);
        Q_UNUSED(saver);
        debug.nospace() << "<WindowEvent " << event.type;
        switch(event.type){
            case Repaint:{
                auto data = event.repaintData;
                debug.nospace() << ' ' << data.geometry() << ' ' << data.waveform << ' ' << data.marker;
                break;
            }
            case Geometry:{
                auto data = event.geometryData;
                debug.nospace() << ' ' << data.geometry() << ' ' << data.z;
                break;
            }
            case ImageInfo:{
                auto data = event.imageInfoData;
                debug.nospace() << ' ' << data.sizeInBytes << ' ' << data.bytesPerLine << ' ' << data.format;
                break;
            }
            case WaitForPaint:
                debug.nospace() << ' ' << event.waitForPaintData.marker;
                break;
            case Key:{
                auto data = event.keyData;
                switch(data.type){
                    case ReleaseKey: debug.nospace() << "Release"; break;
                    case PressKey: debug.nospace() << "Press"; break;
                    case RepeatKey: debug.nospace() << "Repeat"; break;
                    default: debug.nospace() << "Unknown"; break;
                }
                debug.nospace() << ' ' << data.code << ' ';
                break;
            }
            case Touch:{
                auto data = event.touchData;
                switch(data.type){
                    case TouchPress: debug.nospace() << "Press"; break;
                    case TouchUpdate: debug.nospace() << "Update"; break;
                    case TouchRelease: debug.nospace() << "Release"; break;
                    case TouchCancel: debug.nospace() << "Cancel"; break;
                    default: debug.nospace() << "Unknown"; break;
                }
                if(data.points.size()){
                    for(const auto& point : data.points){
                        debug.nospace() << ' ';
                        switch(point.tool){
                            case Finger: debug.nospace() << "Finger"; break;
                            case Token: debug.nospace() << "Token"; break;
                            default: debug.nospace() << "Unknown"; break;
                        }
                        debug.nospace() << "(" << point.id << ' ';
                        switch(point.state){
                            case PointPress: debug.nospace() << "Press"; break;
                            case PointMove: debug.nospace() << "Move"; break;
                            case PointRelease: debug.nospace() << "Release"; break;
                            case PointStationary: debug.nospace() << "None"; break;
                            default: debug.nospace() << "Unknown"; break;
                        }

                        debug.nospace() << ' ' << point.geometry() << ' ' << point.pressure << ' ' << point.rotation << ')';
                    }
                }
                debug.nospace() << '>';
                break;
            }
            case Tablet:{
                auto data = event.tabletData;
                switch(data.type){
                    case PenPress: debug.nospace() << "Press"; break;
                    case PenUpdate: debug.nospace() << "Update"; break;
                    case PenRelease: debug.nospace() << "Release"; break;
                    case PenEnterProximity: debug.nospace() << "EnterProximity"; break;
                    case PenLeaveProximity: debug.nospace() << "LeaveProximity"; break;
                    default: debug.nospace() << "Unknown"; break;
                }
                debug.nospace() << ' ';
                switch(data.tool){
                    case Pen: debug << "Pen"; break;
                    case Eraser: debug << "Eraser"; break;
                    default: debug.nospace() << "Unknown"; break;
                }
                debug.nospace() << ' ' << data.pressure << ' ' << data.point();
                debug.nospace() << " (" << data.tiltX << ", " << data.tiltY << ')';
                break;
            }
            case Raise:
            case Lower:
            case Close:
            case FrameBuffer:
            case Invalid:
            case Ping:
            default:
                break;
        }
        debug.nospace() << '>';
        return debug;
    }

    QDebug operator<<(QDebug debug, WindowEvent* event){
        QDebugStateSaver saver(debug);
        Q_UNUSED(saver);
        if(event != nullptr){
            debug.nospace() << *event;
        }else{
            debug.nospace() << "<Invalid>";
        }
        return debug;
    }

    QSharedPointer<codes::eeems::oxide1::General> getAPI(){
        if(api_general.isNull()){
            connect();
        }
        return api_general;
    }

    QString requestAPI(std::string name){
        connect();
        if(api_general.isNull()){
            O_WARNING("Unable to request " << name.c_str() << " api: Unable to connect to API");
            return "/";
        }
        auto reply = api_general->requestAPI(QString::fromStdString(name));
        if(reply.isError()){
            O_WARNING("Unable to request " << name.c_str() << " api:" << reply.error());
            return "/";
        }
        QDBusObjectPath path = reply;
        if(path.path() == "/"){
            O_WARNING("API " << name.c_str() << " request denied, or unavailable");
        }
        return path.path();
    }

    void releaseAPI(std::string name){
        if(verifyConnection()){
            api_general->releaseAPI(QString::fromStdString(name));
        }
    }

    void connect(){
        if(verifyConnection()){
            return;
        }
        O_DEBUG("Connecting to tarnish over DBus...");
        auto bus = QDBusConnection::systemBus();
        auto interface = bus.interface();
        if(interface == nullptr){
            O_DEBUG("DBus interface not available!");
            return;
        }
        if(!interface->registeredServiceNames().value().contains(OXIDE_SERVICE)){
            O_DEBUG("Waiting for tarnish DBus service to start up...");
            while(!interface->registeredServiceNames().value().contains(OXIDE_SERVICE)){
                timespec args{
                    .tv_sec = 1,
                    .tv_nsec = 0
                };
                nanosleep(&args, NULL);
            }
        }
        O_DEBUG("Tarnish DBus service is online, connecting...");
        auto instance = new codes::eeems::oxide1::General(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
        api_general.reset(instance, &QObject::deleteLater);
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(instance, &codes::eeems::oxide1::General::aboutToQuit, [conn]{
            O_WARNING("Tarnish has indicated that it is about to quit!");
            if(!childSocket.isOpen()){
                disconnect();
            }
            QObject::disconnect(*conn);
            delete conn;
        });
        if(qApp != nullptr){
            O_DEBUG("Connecting to QGuiApplication::aboutToQuit");
            QObject::connect(qApp, &QCoreApplication::aboutToQuit, []{
                O_DEBUG("Application about to quit");
                disconnect();
            });
        }
        qRegisterMetaType<QAbstractSocket::SocketState>();
        O_DEBUG("Connected to tarnish Dbus service");
    }

    void disconnect(){
        O_DEBUG("Disconnecting from tarnish");
        if(!verifyConnection()){
            O_DEBUG("Already disconnected");
            return;
        }
        if(!fbMutex.tryLock()){
            O_DEBUG("Already disconnecting");
            return;
        }
        Oxide::dispatchToMainThread([]{
            fbData = nullptr;
            fbFile.close();
            dispatchToThread(childSocket.thread(), []{
                O_DEBUG("Closing child socket");
                childSocket.disconnect();
                childSocket.close();
                O_DEBUG("Child socket closed");
            });
            dispatchToThread(eventSocket.thread(), []{
                O_DEBUG("Closing event socket");
                if(eventSocket.isOpen() && getpid() == getpgrp()){
                    WindowEvent event;
                    event.type = Close;
                    event.toSocket(&eventSocket);
                }
                eventSocket.disconnect();
                eventSocket.close();
                O_DEBUG("Event socket closed");
            });
#define freeAPI(name) \
            if(!api_##name.isNull()){ \
                if(strcmp(#name, "general") == 0){ \
                    releaseAPI(#name); \
                } \
                api_##name.clear(); \
            }
            freeAPI(power);
            freeAPI(wifi);
            freeAPI(screen);
            freeAPI(apps);
            freeAPI(system);
            freeAPI(notification);
            freeAPI(gui);
            freeAPI(general);
#undef freeAPI
            if(!window.isNull()){
                window.clear();
            }
            if(eventThread.isRunning()){
                // Wait after requesting the quit for both to make sure both are stopping at the same time
                O_DEBUG("Instructing event thread to quit");
                eventThread.quit();
                eventThread.requestInterruption();
                eventThread.wait();
                O_DEBUG("Event thread has quit");
            }
            O_DEBUG("Disconnected");
        });
        fbMutex.unlock();
    }

    int getSocketFd(){
        if(childSocket.isOpen()){
            return childSocket.socketDescriptor();
        }
        auto socket = getSocket();
        return socket != nullptr ? socket->socketDescriptor() : -1;
    }

    QLocalSocket* getSocket(){
        if(childSocket.isOpen()){
            return &childSocket;
        }
        if(getAPI() == nullptr){
            O_WARNING("Unable to register child: Unable to get general API");
            return nullptr;
        }
        O_DEBUG("Connecting to tarnish command socket");
        auto reply = api_general->registerChild();
        if(reply.isError()){
            O_WARNING("Unable to register child: " << reply.error().message());
            return nullptr;
        }
        auto qfd = reply.value();
        if(!qfd.isValid()){
            O_WARNING("Unable to register child: Invalid fd provided");
            return nullptr;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING("Unable to get child socket: No pipe provided");
            return nullptr;
        }
        fd = dup(fd);
        if(!childSocket.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadWrite | QLocalSocket::Unbuffered)){
            ::close(fd);
            O_WARNING("Unable to get child socket:" << childSocket.errorString());
            return nullptr;
        }
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&childSocket, &QLocalSocket::disconnected, [conn]{
            O_WARNING("Lost connection to tarnish socket!");
            childSocket.close();
            disconnect();
            QObject::disconnect(*conn);
            delete conn;
        });
        O_DEBUG("Connected to tarnish command socket");
        return &childSocket;
    }

    int tarnishPid(){
        if(getAPI() == nullptr){
            O_WARNING("Unable to get tarnish PID: Unable to get general API");
            return -1;
        }
        return api_general->tarnishPid();
    }

    void setFrameBufferFormat(QImage::Format format){ fbRequestedFormat = format; }

    int getFrameBufferFd(){
        if(fbFile.isOpen()){
            return fbFile.handle();
        }
        QMutexLocker locker(&fbMutex);
        Q_UNUSED(locker);
        if(guiApi() == nullptr){
            O_WARNING("Unable to get framebuffer: Unable to get GUI API");
            return -1;
        }
        if(getSocket() == nullptr){
            O_WARNING("Unable to get framebuffer: Unable get command socket");
            return -1;
        }
        if(window == nullptr){
            auto reply = api_gui->windows();
            if(reply.isValid()){
                for(QDBusObjectPath qpath : reply.value()){
                    auto path = qpath.path();
                    if(path == "/"){
                        continue;
                    }
                    window = QSharedPointer<codes::eeems::oxide1::Window>(new codes::eeems::oxide1::Window(OXIDE_SERVICE, path, api_gui->connection()));
                    if(window->format() == (int)fbRequestedFormat){
                        break;
                    }
                    window.clear();
                    window = nullptr;
                }
            }
        }
        if(window == nullptr){
            O_DEBUG("Creating with" << fbRequestedFormat << "image format");
            QDBusPendingReply<QDBusObjectPath> reply = api_gui->createWindow(QString::fromStdString(Oxide::getAppName()), fbRequestedFormat);
            reply.waitForFinished();
            if(reply.isError()){
                O_WARNING("Unable to get framebuffer:" << reply.error());
                return -1;
            }
            auto path = reply.value().path();
            if(path == "/"){
                O_WARNING("Unable to get framebuffer: Unable to create window");
                return -1;
            }
            auto instance = new codes::eeems::oxide1::Window(OXIDE_SERVICE, path, api_gui->connection());
            window = QSharedPointer<codes::eeems::oxide1::Window>(instance);
            QObject::connect(instance, &codes::eeems::oxide1::Window::frameBufferChanged, [=](const QDBusUnixFileDescriptor& fd){
                O_DEBUG("frameBufferChanged");
                fbFile.close();
                fbGeometry.adjust(0, 0, 0, 0);
                fbLineSize = 0;
                fbFormat = QImage::Format_Invalid;
                fbData = nullptr;
                if(!fd.isValid()){
                    O_WARNING("Unable to get framebuffer: Invalid DBus response");
                    return;
                }
                auto fb = fd.fileDescriptor();
                if(fb == -1){
                    O_WARNING("Unable to get framebuffer: File descriptor is -1");
                    return;
                }
                auto frameBufferFd = dup(fb);
                if(frameBufferFd == -1){
                    O_WARNING("Failed to unmap framebuffer:" << fbFile.errorString());
                    return;
                }
                setupFbFd(frameBufferFd);
            });
            QObject::connect(instance, &codes::eeems::oxide1::Window::closed, [=](){
                O_DEBUG("Window closed");
                window.clear();
                window = nullptr;
            });
            window->setVisible(true);
        }
        O_DEBUG("Getting framebuffer fd");
        auto qfd = window->frameBuffer();
        O_DEBUG("Checking framebuffer fd");
        if(!qfd.isValid()){
            O_WARNING("Unable to get framebuffer: Invalid DBus response");
            return -1;
        }
        int fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING("Unable to get framebuffer: File descriptor is -1");
            return -1;
        }
        fd = dup(fd);
        if(fd == -1){
            O_WARNING("Unable to dup framebuffer fd:" << strerror(errno));
            return -1;
        }
        if(!setupFbFd(fd)){
            return -1;
        }
        O_DEBUG("Framebuffer fd:" << fd);
        return fbFile.handle();
    }

    uchar* frameBuffer(){
        if(fbData != nullptr){
            return fbData;
        }
        auto fd = getFrameBufferFd();
        if(fd == -1){
            O_WARNING("Unable to get framebuffer fd");
            return nullptr;
        }
        fbData = fbFile.map(0, fbFile.size());
        if(fbData == nullptr){
            O_WARNING("Unable to get framebuffer data:" << fbFile.errorString());
            return nullptr;
        }
        return fbData;
    }

    void lockFrameBuffer(){
        // TODO - explore if mprotect/msync work. This might not be doing anything.
        if(!fbFile.isOpen()){
            return;
        }
        while(flock(fbFile.handle(), LOCK_EX | LOCK_NB) == -1){
            if(errno != EWOULDBLOCK && errno != EINTR){
                O_DEBUG("Failed to lock framebuffer:" << strerror(errno));
                continue;
            }
            if(!QCoreApplication::startingUp()){
                qApp->processEvents(QEventLoop::AllEvents, 100);
            }else{
                timespec args{
                    .tv_sec = 0,
                    .tv_nsec = 100 * 1000
                };
                nanosleep(&args, NULL);
            }
        }
    }

    void unlockFrameBuffer(){
        // TODO - explore if mprotect/msync work. This might not be doing anything.
        if(!fbFile.isOpen()){
            return;
        }
        while(flock(fbFile.handle(), LOCK_UN | LOCK_NB) == -1){
            if(errno != EWOULDBLOCK && errno != EINTR){
                O_DEBUG("Failed to unlock framebuffer:" << strerror(errno));
                continue;
            }
            if(!QCoreApplication::startingUp()){
                qApp->processEvents(QEventLoop::AllEvents, 100);
            }else{
                timespec args{
                    .tv_sec = 0,
                    .tv_nsec = 100 * 1000
                };
                nanosleep(&args, NULL);
            }
        }
    }

    QImage frameBufferImage(){
        auto data = frameBuffer();
        if(data == nullptr){
            O_WARNING("Unable to get framebuffer image: Could not get buffer");
            return QImage();
        }
        QImage image((uchar*)data, fbGeometry.width(), fbGeometry.height(), fbLineSize, fbFormat);
        if(image.isNull()){
            O_WARNING("Unable to get framebuffer image: Image is null");
        }
        return image;
    }

    int getEventPipeFd(){
        if(eventSocket.isOpen()){
            return eventSocket.socketDescriptor();
        }
        if(window == nullptr && getFrameBufferFd() == -1){
            O_WARNING("Unable to get window event pipe, no window");
            return -1;
        }
        auto qfd = window->eventPipe();
        if(!qfd.isValid()){
            O_WARNING("Unable to get window event pipe: Invalid DBus response");
            return -1;
        }
        auto fd = qfd.fileDescriptor();
        if(fd == -1){
            O_WARNING("Unable to get window event pipe: No pipe provided");
            return -1;
        }
        fd = dup(fd);
        if(!dispatchToThread<bool>(eventSocket.thread(), [fd]{
            return eventSocket.setSocketDescriptor(fd, QLocalSocket::ConnectedState, QLocalSocket::ReadWrite | QLocalSocket::Unbuffered);
        })){
            ::close(fd);
            O_WARNING("Unable to get window event pipe:" << eventSocket.errorString());
            return -1;
        }
        if(!eventThread.isRunning()){
            eventThread.setObjectName("liboxide::event");
            Oxide::startThreadWithPriority(&eventThread, QThread::TimeCriticalPriority);
            Oxide::dispatchToThread(eventThread.thread(), []{ eventThread.moveToThread(&eventThread); });
            Oxide::dispatchToThread(eventSocket.thread(), []{ eventSocket.moveToThread(&eventThread); });
        }
        auto conn = new QMetaObject::Connection;
        *conn = QObject::connect(&eventSocket, &QLocalSocket::disconnected, [conn]{
            O_WARNING("Lost connection to window event pipe!");
            eventSocket.close();
            QObject::disconnect(*conn);
            delete conn;
        });
        O_DEBUG("Event Pipe fd:" << fd);
        return eventSocket.socketDescriptor();
    }

    QLocalSocket* getEventPipe(){
        auto fd = getEventPipeFd();
        if(fd == -1){
            O_WARNING("Unable to get window event pipe: Failed to get pipe fd");
            return nullptr;
        }
        return &eventSocket;
    }

    void screenUpdate(QRect rect, EPFrameBuffer::WaveformMode waveform, unsigned int marker){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbFile.size(), MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING("Failed update screen:" << strerror(errno))
            return;
        }
        if(!eventSocket.isOpen()){
            O_WARNING("Failed update screen: event socket not open");
            return;
        }
        if(marker == 0){
            marker = ++repaintMarker;
        }
        WindowEvent event;
        event.type = WindowEventType::Repaint;
        event.repaintData.x = rect.x();
        event.repaintData.y = rect.y();
        event.repaintData.width = rect.width();
        event.repaintData.height = rect.height();
        event.repaintData.waveform = waveform;
        event.repaintData.marker = marker;
        event.toSocket(&eventSocket);
    }

    void screenUpdate(EPFrameBuffer::WaveformMode waveform, unsigned int marker){
        if(fbData == nullptr){
            return;
        }
        if(msync(fbData, fbFile.size(), MS_SYNC | MS_INVALIDATE) == -1){
            O_WARNING("Failed update screen:" << strerror(errno))
            return;
        }
        if(!eventSocket.isOpen()){
            O_WARNING("Failed update screen: event socket not open");
            return;
        }
        auto image = frameBufferImage();
        if(image.isNull()){
            O_WARNING("Failed update screen: Image is null");
            return;
        }
        if(marker == 0){
            marker = ++repaintMarker;
        }
        WindowEvent event;
        event.type = WindowEventType::Repaint;
        auto rect = image.rect();
        event.repaintData.x = rect.x();
        event.repaintData.y = rect.y();
        event.repaintData.width = rect.width();
        event.repaintData.height = rect.height();
        event.repaintData.waveform = waveform;
        event.repaintData.marker = marker;
        event.toSocket(&eventSocket);
    }

    unsigned int requestWaitForUpdate(unsigned int marker){
        if(!eventSocket.isOpen()){
            O_WARNING("Failed wait for screen update: event socket not open");
            return 0;
        }
        WindowEvent event;
        event.type = WindowEventType::WaitForPaint;
        event.waitForPaintData.marker = marker == 0 ? (unsigned int)repaintMarker : marker;
        event.toSocket(&eventSocket);
        return event.waitForPaintData.marker;
    }

    unsigned int requestWaitForLastUpdate(){ return requestWaitForUpdate(0); }

    QSharedPointer<codes::eeems::oxide1::Power> powerApi(){
        if(!api_power.isNull()){
            return api_power;
        }
        auto path = requestAPI("power");
        if(path == "/"){
            return api_power;
        }
        api_power.reset(new codes::eeems::oxide1::Power(OXIDE_SERVICE, path, api_general->connection()), &QObject::deleteLater);
        return api_power;
    }

    QSharedPointer<codes::eeems::oxide1::Wifi> wifiApi(){
        if(!api_wifi.isNull()){
            return api_wifi;
        }
        auto path = requestAPI("wifi");
        if(path == "/"){
            return api_wifi;
        }
        api_wifi.reset(new codes::eeems::oxide1::Wifi(OXIDE_SERVICE, path, api_general->connection()), &QObject::deleteLater);
        return api_wifi;
    }

    QSharedPointer<codes::eeems::oxide1::Screen> screenApi(){
        if(!api_screen.isNull()){
            return api_screen;
        }
        auto path = requestAPI("screen");
        if(path == "/"){
            return api_screen;
        }
        api_screen.reset(new codes::eeems::oxide1::Screen(OXIDE_SERVICE, path, api_general->connection()), &QObject::deleteLater);
        return api_screen;
    }

    QSharedPointer<codes::eeems::oxide1::Apps> appsApi(){
        if(!api_apps.isNull()){
            return api_apps;
        }
        auto path = requestAPI("apps");
        if(path == "/"){
            return api_apps;
        }
        api_apps.reset(new codes::eeems::oxide1::Apps(OXIDE_SERVICE, path, api_general->connection()), &QObject::deleteLater);
        return api_apps;
    }

    QSharedPointer<codes::eeems::oxide1::System> systemApi(){
        if(!api_system.isNull()){
            return api_system;
        }
        auto path = requestAPI("system");
        if(path == "/"){
            return api_system;
        }
        api_system.reset(new codes::eeems::oxide1::System(OXIDE_SERVICE, path, api_general->connection()), &QObject::deleteLater);
        return api_system;
    }

    QSharedPointer<codes::eeems::oxide1::Notifications> notificationApi(){
        if(!api_notification.isNull()){
            return api_notification;
        }
        auto path = requestAPI("notification");
        if(path == "/"){
            return api_notification;
        }
        api_notification.reset(new codes::eeems::oxide1::Notifications(OXIDE_SERVICE, path, api_general->connection()), &QObject::deleteLater);
        return api_notification;
    }

    QSharedPointer<codes::eeems::oxide1::Gui> guiApi(){
        if(!api_gui.isNull()){
            return api_gui;
        }
        auto path = requestAPI("gui");
        if(path == "/"){
            return api_gui;
        }
        api_gui.reset(new codes::eeems::oxide1::Gui(OXIDE_SERVICE, path, api_general->connection()), &QObject::deleteLater);
        return api_gui;
    }

    QSharedPointer<codes::eeems::oxide1::Window> topWindow(){ return window; }
}
