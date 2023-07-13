/*!
 * \addtogroup Oxide::Tarnish
 * \brief The Tarnish module
 * @{
 * \file
 */
#pragma once
#include "liboxide_global.h"
#include "dbus.h"

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
        QRect geometry;
        EPFrameBuffer::WaveformMode waveform;
    };
    typedef RepaintEventArgs RepaintEventArgs;
    struct GeometryEventArgs{
        QRect geometry;
        int z;
    };
    typedef GeometryEventArgs GeometryEventArgs;
    struct ImageInfoEventArgs{
        qulonglong sizeInBytes;
        qulonglong bytesPerLine;
        QImage::Format format;
    };
    typedef ImageInfoEventArgs ImageInfoEventArgs;

    /*!
     * \brief The WindowEventType enum
     */
    enum WindowEventType{
        Repaint, /*!< */
        WaitForPaint, /*!< */
        Geometry, /*!< */
        ImageInfo, /*!< */
        Raise, /*!< */
        Lower, /*!< */
        Close, /*!< */
        FrameBuffer, /*!< */
    };
    /*!
     * \brief The WindowEvent class
     */
    class LIBOXIDE_EXPORT WindowEvent : public QObject{
        Q_OBJECT

    public:
        WindowEvent();
        WindowEventType type;
        void* data;

        template<typename T>
        void setData(T args){ data = &args; }

        template<typename T>
        T* getData(){ return reinterpret_cast<T*>(data); }
    };
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
    LIBOXIDE_EXPORT QDataStream* getEventPipe();
    /*!
     * \brief screenUpdate
     * \param waveform
     */
    LIBOXIDE_EXPORT void screenUpdate(EPFrameBuffer::WaveformMode waveform = EPFrameBuffer::Mono);
    /*!
     * \brief screenUpdate
     * \param rect
     * \param waveform
     */
    LIBOXIDE_EXPORT void screenUpdate(QRect rect, EPFrameBuffer::WaveformMode waveform = EPFrameBuffer::Mono);
    /*!
     * \brief waitForLastUpdate
     */
    LIBOXIDE_EXPORT void waitForLastUpdate();
    /*!
     * \brief powerAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Power* powerAPI();
    /*!
     * \brief wifiAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Wifi* wifiAPI();
    /*!
     * \brief screenAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Screen* screenAPI();
    /*!
     * \brief appsAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Apps* appsAPI();
    /*!
     * \brief systemAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::System* systemAPI();
    /*!
     * \brief notificationAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Notifications* notificationAPI();
    /*!
     * \brief guiAPI
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Gui* guiAPI();
    /*!
     * \brief topWindow
     * \return
     */
    LIBOXIDE_EXPORT codes::eeems::oxide1::Window* topWindow();
}
/*!
 * \brief operator <<
 * \return
 */
LIBOXIDE_EXPORT QDataStream& operator<<(QDataStream& stream, const Oxide::Tarnish::WindowEvent& event);
/*!
 * \brief operator >>
 * \return
 */
LIBOXIDE_EXPORT QDataStream& operator>>(QDataStream& stream, Oxide::Tarnish::WindowEvent& event);
