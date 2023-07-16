/*!
 * \addtogroup Oxide::Tarnish
 * \brief The Tarnish module
 * @{
 * \file
 */
#pragma once
#include "liboxide_global.h"
#include "dbus.h"
#include "debug.h"

#include <QLocalSocket>

#ifndef SHIM_INPUT_FOR_PRELOAD
#include <linux/input.h>
#else
struct input_event {
#if (__BITS_PER_LONG != 32 || !defined(__USE_TIME_BITS64)) && !defined(__KERNEL__)
    struct timeval time;
#define input_event_sec time.tv_sec
#define input_event_usec time.tv_usec
#else
    __kernel_ulong_t __sec;
#if defined(__sparc__) && defined(__arch64__)
    unsigned int __usec;
#else
    __kernel_ulong_t __usec;
#endif
#define input_event_sec  __sec
#define input_event_usec __usec
#endif
    __u16 type;
    __u16 code;
    __s32 value;
};
#endif
#include <epframebuffer.h>

namespace Oxide::Tarnish {
    /*!
     * \brief The InputEventSocket class
     */
    class LIBOXIDE_EXPORT InputEventSocket : public QLocalSocket {
        Q_OBJECT

    public:
        /*!
         * \brief InputEventSocket
         */
        InputEventSocket();
        /*!
         * \brief ~InputEventSocket
         */
        ~InputEventSocket();
        /*!
         * \brief setCallback
         * \param callback
         */
        void setCallback(std::function<void(const input_event&)> callback);

    signals:
        /*!
         * \brief inputEvent
         */
        void inputEvent(const input_event&);

    private:
        QDataStream stream;
        std::function<void(const input_event&)> m_callback = nullptr;

    private slots:
        void readEvent();
    };

    // WindowEventType::Repaint
    struct RepaintEventArgs{
        static qsizetype size();
        int x;
        int y;
        int width;
        int height;
        EPFrameBuffer::WaveformMode waveform;
        unsigned int marker;
        QRect geometry() const;
    };
    typedef RepaintEventArgs RepaintEventArgs;
    QByteArray& operator>>(QByteArray& l, RepaintEventArgs& r);
    QByteArray& operator<<(QByteArray& l, RepaintEventArgs& r);

    // WindowEventType::Geometry
    struct GeometryEventArgs{
        static qsizetype size();
        int x;
        int y;
        int width;
        int height;
        int z;
        QRect geometry() const;
    };
    typedef GeometryEventArgs GeometryEventArgs;
    QByteArray& operator>>(QByteArray& l, GeometryEventArgs& r);
    QByteArray& operator<<(QByteArray& l, GeometryEventArgs& r);

    // WindowEventType::ImageInfo
    struct ImageInfoEventArgs{
        static qsizetype size();
        qulonglong sizeInBytes;
        qulonglong bytesPerLine;
        QImage::Format format;
    };
    typedef ImageInfoEventArgs ImageInfoEventArgs;
    QByteArray& operator>>(QByteArray& l, ImageInfoEventArgs& r);
    QByteArray& operator<<(QByteArray& l, ImageInfoEventArgs& r);

    // WindowEventType::WaitForPaint
    struct WaitForPaintEventArgs{
        static qsizetype size();
        unsigned int marker;
    };
    typedef WaitForPaintEventArgs WaitForPaintEventArgs;
    QByteArray& operator>>(QByteArray& l, WaitForPaintEventArgs& r);
    QByteArray& operator<<(QByteArray& l, WaitForPaintEventArgs& r);

    //WindowEventType::Key
    enum KeyEventType: unsigned short{
        ReleaseKey = 0,
        PressKey,
        RepeatKey,
    };
    struct KeyEventArgs{
        static qsizetype size();
        unsigned int code; // Qt keycode
        KeyEventType type;
        unsigned int unicode;
    };
    typedef KeyEventArgs KeyEventArgs;
    QByteArray& operator>>(QByteArray& l, KeyEventArgs& r);
    QByteArray& operator<<(QByteArray& l, KeyEventArgs& r);

    // WindowEventType::Touch
    enum TouchEventType: unsigned short{
        TouchPress = 0,
        TouchUpdate,
        TouchRelease,
        TouchCancel,
    };
    enum TouchEventTool: unsigned short{
        Finger = 0,
        Token,
    };
    struct TouchEventPoint{
        int id;
        int x;
        int y;
        unsigned int width;
        unsigned int height;
        TouchEventTool tool;
        unsigned int pressure;
        int orientation;
        QRect geometry() const;
        QPoint point() const;
        QSize size() const;
    };
    struct TouchEventArgs{
        static qsizetype size();
        static qsizetype itemSize();
        TouchEventType type;
        unsigned short count;
        TouchEventPoint points[];
        qsizetype realSize();
    };
    typedef TouchEventArgs TouchEventArgs;
    QByteArray& operator>>(QByteArray& l, TouchEventArgs& r);
    QByteArray& operator<<(QByteArray& l, TouchEventArgs& r);

    // WindowEventType::Tablet
    enum TabletEventType: unsigned short{
        PenPress = 0,
        PenUpdate,
        PenRelease,
    };
    enum TabletEventTool: unsigned short{
        Pen = 0,
        Eraser,
    };
    struct TabletEventArgs{
        static qsizetype size();
        TabletEventType type;
        TabletEventTool tool;
        int x;
        int y;
        unsigned int pressure;
        int tiltX;
        int tiltY;
        QPoint point() const;
    };
    typedef TabletEventArgs TabletEventArgs;
    QByteArray& operator>>(QByteArray& l, TabletEventArgs& r);
    QByteArray& operator<<(QByteArray& l, TabletEventArgs& r);

    /*!
     * \brief The WindowEventType enum
     */
    enum WindowEventType : unsigned short{
        Invalid = 0, /*!< Invalid event */
        Ping, /*!< */
        Repaint, /*!< */
        WaitForPaint, /*!< */
        Geometry, /*!< */
        ImageInfo, /*!< */
        Raise, /*!< */
        Lower, /*!< */
        Close, /*!< */
        FrameBuffer, /*!< */
        Key, /*!< */
        Touch, /*!< */
        Tablet, /*!< */
    };
    QDebug operator<<(QDebug debug, const WindowEventType& type);
    /*!
     * \brief The WindowEvent class
     */
    class LIBOXIDE_EXPORT WindowEvent{
    public:
        static WindowEvent fromSocket(QIODevice* socket);
        bool toSocket(QIODevice* socket);

        WindowEvent();
        WindowEvent(const WindowEvent& event);
        ~WindowEvent();
        bool isValid();
        WindowEventType type;
        RepaintEventArgs repaintData;
        GeometryEventArgs geometryData;
        ImageInfoEventArgs imageInfoData;
        WaitForPaintEventArgs waitForPaintData;
        KeyEventArgs keyData;
        TabletEventArgs tabletData;
        TouchEventArgs touchData;

    private:
        static QMutex m_writeMutex;
        static QMutex m_readMutex;
    };
    QDebug operator<<(QDebug debug, const WindowEvent& event);
    QDebug operator<<(QDebug debug, WindowEvent* event);
    /*!
     * \brief Get the current General API instance
     * \return The current General API instance
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::General* getAPI();
    /*!
     * \brief Request access to an API
     * \param Name of the API
     * \return DBus path to the API, or "/" if access is denied
     */
    LIBOXIDE_EXPORT QString requestAPI(std::string name);
    /*!
     * \brief Connect to the current tarnish instance
     */
    LIBOXIDE_EXPORT void connect();
    /*!
     * \brief Disconnect from the current tarnish instance
     */
    LIBOXIDE_EXPORT void disconnect();
    /*!
     * \brief Register this child application to be managed by Tarnish
     */
    LIBOXIDE_EXPORT int getSocketFd();
    /*!
     * \brief Register this child application to be managed by Tarnish
     */
    LIBOXIDE_EXPORT QLocalSocket* getSocket();
    /*!
     * \brief Get the tarnish PID
     * \return The tarnish PID
     */
    LIBOXIDE_EXPORT int tarnishPid();
    /*!
     * \brief getFrameBuffer
     * \return
     */
    LIBOXIDE_EXPORT int getFrameBufferFd(QImage::Format format = DEFAULT_IMAGE_FORMAT);
    /*!
     * \brief frameBuffer
     * \return
     */
    LIBOXIDE_EXPORT uchar* frameBuffer(QImage::Format format = DEFAULT_IMAGE_FORMAT);
    /*!
     * \brief lockFrameBuffer
     * \return
     */
    LIBOXIDE_EXPORT void lockFrameBuffer();
    /*!
     * \brief unlockFrameBuffer
     * \return
     */
    LIBOXIDE_EXPORT void unlockFrameBuffer();
    /*!
     * \brief frameBuffer
     * \return
     */
    LIBOXIDE_EXPORT QImage frameBufferImage(QImage::Format format = DEFAULT_IMAGE_FORMAT);
    /*!
     * \brief getTouchEventPipeFd
     * \return
     */
    LIBOXIDE_EXPORT int getTouchEventPipeFd();
    /*!
     * \brief getTouchEventPipe
     * \return
     */
    LIBOXIDE_EXPORT InputEventSocket* getTouchEventPipe();
    /*!
     * \brief getTabletEventPipeFd
     * \return
     */
    LIBOXIDE_EXPORT int getTabletEventPipeFd();
    /*!
     * \brief getTabletEventPipe
     * \return
     */
    LIBOXIDE_EXPORT InputEventSocket* getTabletEventPipe();
    /*!
     * \brief getKeyEventPipeFd
     * \return
     */
    LIBOXIDE_EXPORT int getKeyEventPipeFd();
    /*!
     * \brief getKeyEventPipe
     * \return
     */
    LIBOXIDE_EXPORT InputEventSocket* getKeyEventPipe();
    /*!
     * \brief getEventPipeFd
     * \return
     */
    LIBOXIDE_EXPORT int getEventPipeFd();
    /*!
     * \brief getEventPipe
     * \return
     */
    LIBOXIDE_EXPORT QLocalSocket* getEventPipe();
    /*!
     * \brief screenUpdate
     * \param waveform
     */
    LIBOXIDE_EXPORT void screenUpdate(EPFrameBuffer::WaveformMode waveform = EPFrameBuffer::Mono, unsigned int marker = 0);
    /*!
     * \brief screenUpdate
     * \param rect
     * \param waveform
     */
    LIBOXIDE_EXPORT void screenUpdate(QRect rect, EPFrameBuffer::WaveformMode waveform = EPFrameBuffer::Mono, unsigned int marker = 0);
    /*!
     * \brief requestWaitForUpdate
     * \param marker
     * \return
     */
    LIBOXIDE_EXPORT unsigned int requestWaitForUpdate(unsigned int marker);
    /*!
     * \brief waitForLastUpdate
     */
    LIBOXIDE_EXPORT unsigned int requestWaitForLastUpdate();
    /*!
     * \brief powerAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Power* powerApi();
    /*!
     * \brief wifiAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Wifi* wifiApi();
    /*!
     * \brief screenAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Screen* screenApi();
    /*!
     * \brief appsAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Apps* appsApi();
    /*!
     * \brief systemAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::System* systemApi();
    /*!
     * \brief notificationAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Notifications* notificationApi();
    /*!
     * \brief guiAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Gui* guiApi();
    /*!
     * \brief topWindow
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Window* topWindow();
}
