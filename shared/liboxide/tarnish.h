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

#include <linux/input.h>
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
    typedef enum{
        Raised,
        Lowered,
        RaisedHidden,
        LoweredHidden
    } WindowState;
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
    LIBOXIDE_EXPORT void registerChild();
    /*!
     * \brief Register this child application to be managed by Tarnish
     * \param The name of the application
     */
    LIBOXIDE_EXPORT void registerChild(std::string name);
    /*!
     * \brief Get the tarnish PID
     * \return The tarnish PID
     */
    LIBOXIDE_EXPORT int tarnishPid();
    /*!
     * \brief getFrameBuffer
     * \return
     */
    LIBOXIDE_EXPORT int getFrameBufferFd();
    /*!
     * \brief frameBuffer
     * \return
     */
    LIBOXIDE_EXPORT uchar* frameBuffer();
    /*!
     * \brief frameBuffer
     * \return
     */
    LIBOXIDE_EXPORT QImage frameBufferImage();
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
     * \brief connectQtEvents
     * \return
     */
    LIBOXIDE_EXPORT bool connectQtEvents(
        std::function<void(const input_event&)> touchCallback,
        std::function<void(const input_event&)> tabletCallback,
        std::function<void(const input_event&)> keyCallback
    );
    /*!
     * \brief screenUpdate
     */
    LIBOXIDE_EXPORT void screenUpdate();
    /*!
     * \brief screenUpdate
     * \param rect
     * \param mode
     */
    LIBOXIDE_EXPORT void screenUpdate(QRect rect);
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
