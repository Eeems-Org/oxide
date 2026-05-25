/*!
 * \addtogroup Blight
 * \brief The Blight module
 * @{
 * \file
 */
#pragma once
#include <optional>

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
    LIBBLIGHT_EXPORT bool exclusiveModeRepaint();
    /*!
     * \brief Set flags for a surface or connection
     * \param identifier surface or connection identifier
     * \param flags New flags to set to
     * \return If the call succeeded or not
     * \retval false call failed
     */
    LIBBLIGHT_EXPORT bool
    setFlags(std::string identifier, std::vector<std::string> flags);
} // namespace Blight
/*! @} */
