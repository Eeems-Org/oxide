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
    codes::eeems::oxide1::General* getAPI();
    /*!
     * \brief Request access to an API
     * \param Name of the API
     * \return DBus path to the API, or "/" if access is denied
     */
    QString requestAPI(std::string name);
    /*!
     * \brief Connect to the current tarnish instance
     */
    void connect();
    /*!
     * \brief Disconnect from the current tarnish instance
     */
    void disconnect();
    /*!
     * \brief Register this child application to be managed by Tarnish
     */
    void registerChild();
    /*!
     * \brief Register this child application to be managed by Tarnish
     * \param The name of the application
     */
    void registerChild(std::string name);
    /*!
     * \brief Get the tarnish PID
     * \return The tarnish PID
     */
    int tarnishPid();
    /*!
     * \brief getFrameBuffer
     * \return
     */
    int getFrameBufferFd();
    /*!
     * \brief frameBuffer
     * \return
     */
    uchar* frameBuffer();
    /*!
     * \brief frameBuffer
     * \return
     */
    QImage frameBufferImage();
    /*!
     * \brief getTouchEventPipeFd
     * \return
     */
    int getTouchEventPipeFd();
    /*!
     * \brief getTouchEventPipe
     * \return
     */
    QLocalSocket* getTouchEventPipe();
    /*!
     * \brief getTabletEventPipeFd
     * \return
     */
    int getTabletEventPipeFd();
    /*!
     * \brief getTabletEventPipe
     * \return
     */
    QLocalSocket* getTabletEventPipe();
    /*!
     * \brief getKeyEventPipeFd
     * \return
     */
    int getKeyEventPipeFd();
    /*!
     * \brief getKeyEventPipe
     * \return
     */
    QLocalSocket* getKeyEventPipe();
    /*!
     * \brief connectQtEvents
     * \return
     */
    bool connectQtEvents(
        std::function<void(const input_event&)> touchCallback,
        std::function<void(const input_event&)> tabletCallback,
        std::function<void(const input_event&)> keyCallback
    );
    /*!
     * \brief screenUpdate
     */
    void screenUpdate();
    /*!
     * \brief screenUpdate
     * \param rect
     * \param mode
     */
    void screenUpdate(QRect rect);
    /*!
     * \brief powerAPI
     * \return
     */
    codes::eeems::oxide1::Power* powerAPI();
    /*!
     * \brief wifiAPI
     * \return
     */
    codes::eeems::oxide1::Wifi* wifiAPI();
    /*!
     * \brief screenAPI
     * \return
     */
    codes::eeems::oxide1::Screen* screenAPI();
    /*!
     * \brief appsAPI
     * \return
     */
    codes::eeems::oxide1::Apps* appsAPI();
    /*!
     * \brief systemAPI
     * \return
     */
    codes::eeems::oxide1::System* systemAPI();
    /*!
     * \brief notificationAPI
     * \return
     */
    codes::eeems::oxide1::Notifications* notificationAPI();
    /*!
     * \brief guiAPI
     * \return
     */
    codes::eeems::oxide1::Gui* guiAPI();
    /*!
     * \brief topWindow
     * \return
     */
    codes::eeems::oxide1::Window* topWindow();
}
