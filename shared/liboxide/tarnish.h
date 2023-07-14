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

    struct RepaintEventArgs{
        static const qsizetype size = (sizeof(int) * 4) + sizeof(quint64) + sizeof(unsigned int);
        QRect geometry;
        EPFrameBuffer::WaveformMode waveform;
        unsigned int marker;
    };
    typedef RepaintEventArgs RepaintEventArgs;
    QByteArray& operator>>(QByteArray& l, RepaintEventArgs& r);
    QByteArray& operator<<(QByteArray& l, RepaintEventArgs& r);

    struct GeometryEventArgs{
        static const qsizetype size = sizeof(int) * 5;
        QRect geometry;
        int z;
    };
    typedef GeometryEventArgs GeometryEventArgs;
    QByteArray& operator>>(QByteArray& l, GeometryEventArgs& r);
    QByteArray& operator<<(QByteArray& l, GeometryEventArgs& r);

    struct ImageInfoEventArgs{
        static const qsizetype size = (sizeof(qulonglong) * 2) + sizeof(int);
        qulonglong sizeInBytes;
        qulonglong bytesPerLine;
        QImage::Format format;
    };
    typedef ImageInfoEventArgs ImageInfoEventArgs;
    QByteArray& operator>>(QByteArray& l, ImageInfoEventArgs& r);
    QByteArray& operator<<(QByteArray& l, ImageInfoEventArgs& r);

    struct WaitForPaintEventArgs{
        static const qsizetype size = sizeof(unsigned int);
        unsigned int marker;
    };
    typedef WaitForPaintEventArgs WaitForPaintEventArgs;
    QByteArray& operator>>(QByteArray& l, WaitForPaintEventArgs& r);
    QByteArray& operator<<(QByteArray& l, WaitForPaintEventArgs& r);

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
    };
    QDebug operator<<(QDebug debug, const WindowEventType& type);
    /*!
     * \brief The WindowEvent class
     */
    class LIBOXIDE_EXPORT WindowEvent{
    public:
        static WindowEvent fromSocket(QLocalSocket* socket);
        bool toSocket(QLocalSocket* socket);

        WindowEvent();
        WindowEvent(const WindowEvent& event);
        ~WindowEvent();
        bool isValid();
        WindowEventType type;
        RepaintEventArgs repaintData;
        GeometryEventArgs geometryData;
        ImageInfoEventArgs imageInfoData;
        WaitForPaintEventArgs waitForPaintData;

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
     * \brief waitForLastUpdate
     */
    LIBOXIDE_EXPORT void requestWaitForLastUpdate();
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
