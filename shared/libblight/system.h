/*!
 * \addtogroup Blight
 * \brief The Blight module
 * @{
 * \file
 */
#pragma once
#include <optional>
#include <vector>

#include "dbus.h"
#include "libblight_global.h"
#include "types.h"

/*!
 * \brief Blight namespace
 */
namespace Blight {
#if defined(LIBBLIGHT_LIBRARY)
    [[maybe_unused]] extern DBus* dbus;
#endif
    /*!
     * \brief Get the file descriptor of the primary framebuffer
     * \return Negative number if there was an error. Otherwise the file
     * descriptor of the buffer for primary framebuffer
     */
    LIBBLIGHT_EXPORT int frameBuffer();
    /*!
     * \brief Get the width, height, and stride of the primary framebuffer
     * \return tuple of with width, height, and stride of the primary buffer
     * \retval {-1, -1, -1 } there was an error
     */
    LIBBLIGHT_EXPORT std::tuple<int, int, int> frameBufferInfo();
    /*!
     * \brief Enter exclusive mode, this disables compositing and expects that
     * all framebuffer interactions happens directly against frameBuffer()
     * \return If the call succeeded or not
     * \retval false call failed
     */
    LIBBLIGHT_EXPORT bool enterExclusiveMode();
    /*!
     * \brief Exit exclusive mode and enable compositing again
     * \return If the call succeeded or not
     * \retval false call failed
     */
    LIBBLIGHT_EXPORT bool exitExclusiveMode();
    /*!
     * \brief While in exclusive mode, repaint the entire screen from the
     * famebuffer
     * \return If the call succeeded or not
     * \retval false call failed
     */
    LIBBLIGHT_EXPORT bool exclusiveModeRepaintFull();
    /*!
     * \brief While in exclusive mode, repaint the entire screen from the
     * famebuffer
     * \return If the call succeeded or not
     * \retval false call failed
     */
    LIBBLIGHT_EXPORT bool exclusiveModeRepaint(
        int x,
        int y,
        int width,
        int height,
        WaveformMode waveform,
        UpdateMode updateMode
    );
    /*!
     * \brief Set flags for a surface or connection
     * \param identifier surface or connection identifier
     * \param flags New flags to set to
     * \return If the call succeeded or not
     * \retval false call failed
     */
    LIBBLIGHT_EXPORT bool
    setFlags(std::string identifier, std::vector<std::string> flags);
    /*!
     * \brief Check if a connection identifier exists
     * \param identifier Connection identifier
     * \return If the connection exists or not
     */
    LIBBLIGHT_EXPORT bool connectionExists(std::string identifier);
} // namespace Blight
/*! @} */
