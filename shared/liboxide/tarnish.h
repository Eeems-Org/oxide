/*!
 * \addtogroup Oxide::Tarnish
 * \brief The Tarnish module
 * @{
 * \file
 */
#pragma once
#include "liboxide_global.h"
#include "dbus.h"

namespace Oxide::Tarnish {
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
     * \brief createFrameBuffer
     * \param width
     * \param height
     * \return
     */
    int createFrameBuffer(int width, int height);
    /*!
     * \brief frameBufferSize
     * \return
     */
    QList<int> frameBufferSize();
    /*!
     * \brief frameBuffer
     * \return
     */
    uchar* frameBuffer();
    /*!
     * \brief frameBuffer
     * \return
     */
    QImage* frameBufferImage();
    /*!
     * \brief getEventPipe
     * \return
     */
    int getEventPipe();
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
}
