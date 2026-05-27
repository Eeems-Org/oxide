#include "fb.h"

#include "libc.h"
#include "state.h"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>

#include <libblight.h>
#include <libblight/clock.h>
#include <libblight/debug.h>
#include <libblight/system.h>
#include <libblight/types.h>

namespace FB {
    Blight::shared_buf_t buffer = nullptr;
    Blight::Connection* connection = nullptr;
    int frameBuffer = -1;
    int epframebufferLockFd = -1;
    int epdLockFd = -1;
    int msgq = -1;
    bool is_fb(int fd)
    {
        return Client::HANDLE_FB && buffer->fd > 0 && buffer->fd == fd;
    }
    int send_update(mxcfb_update_data* update)
    {
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SEND_UPDATE")
        Blight::ClockWatch cw;
        if (!buffer->surface) {
            Blight::addSurface(buffer);
        }
        if (!buffer->surface) {
            _CRIT("Failed to create surface: %s", std::strerror(errno));
            std::exit(errno);
        }
        auto region = update->update_region;
        auto maybe = connection->repaint(
            buffer,
            region.left,
            region.top,
            region.width,
            region.height,
            (Blight::WaveformMode)update->waveform_mode,
            (Blight::UpdateMode)update->update_mode,
            update->update_marker
        );
        if (maybe.has_value()) {
            maybe.value()->wait();
        }
        _DEBUG("ioctl /dev/fb0 MXCFB_SEND_UPDATE done: %f", cw.elapsed());
        // TODO - notify on rM2 for screensharing
        return 0;
    }
    int wait(mxcfb_update_marker_data* update)
    {
        _DEBUG("%s", "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE");
        Blight::ClockWatch cw;
        connection->waitForMarker(update->update_marker);
        _DEBUG(
            "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE done: %f",
            cw.elapsed()
        );
        return 0;
    }
    int get_vscreeninfo(fb_var_screeninfo* screenInfo)
    {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_VSCREENINFO");
        screenInfo->xres = buffer->width;
        screenInfo->yres = buffer->height;
        screenInfo->xoffset = 0;
        screenInfo->yoffset = 0;
        screenInfo->xres_virtual = buffer->width;
        screenInfo->yres_virtual = buffer->height;
        screenInfo->bits_per_pixel = buffer->stride / buffer->width * 8;
        // TODO - determine the following from the buffer format
        // Format_RGB16 / RGB565
        screenInfo->grayscale = 0;
        screenInfo->red.offset = 11;
        screenInfo->red.length = 5;
        screenInfo->red.msb_right = 0;
        screenInfo->green.offset = 5;
        screenInfo->green.length = 6;
        screenInfo->green.msb_right = 0;
        screenInfo->blue.offset = 0;
        screenInfo->blue.length = 5;
        screenInfo->blue.msb_right = 0;
        // TODO what should this even be?
        screenInfo->nonstd = 0;
        screenInfo->activate = FB_ACTIVATE_FORCE;
        // It would be cool to have the actual mm width/height here
        screenInfo->height = 0;
        screenInfo->width = 0;
        screenInfo->accel_flags = 0;
        // screenInfo->pixclock = 28800; // Stick with what is reported
        screenInfo->left_margin = 0;
        screenInfo->right_margin = 0;
        screenInfo->upper_margin = 0;
        screenInfo->lower_margin = 0;
        screenInfo->hsync_len = 1;
        screenInfo->vsync_len = 1;
        screenInfo->sync = 0;
        screenInfo->vmode = 0;
        screenInfo->rotate = 0;
        screenInfo->colorspace = 0;
        screenInfo->reserved[0] = 0;
        screenInfo->reserved[1] = 0;
        screenInfo->reserved[2] = 0;
        screenInfo->reserved[3] = 0;
        return 0;
    }
    int get_fscreeninfo(fb_fix_screeninfo* screenInfo)
    {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_FSCREENINFO");
        constexpr char fb_id[] = "mxcfb";
        memcpy(screenInfo->id, fb_id, sizeof(fb_id));
        // TODO determine all the values dynamically
        screenInfo->smem_len = buffer->size();
        screenInfo->smem_start = (unsigned long)buffer->data;
        screenInfo->type = 0;
        screenInfo->type_aux = 0;
        screenInfo->visual = FB_VISUAL_TRUECOLOR;
        screenInfo->xpanstep = 8;
        screenInfo->ypanstep = 0;
        screenInfo->ywrapstep = 5772;
        screenInfo->line_length = buffer->stride;
        screenInfo->mmio_start = 0;
        screenInfo->mmio_len = 0;
        screenInfo->accel = 0;
        screenInfo->reserved[0] = 0;
        screenInfo->reserved[1] = 0;
        return 0;
    }
    int put_vscreeninfo(fb_var_screeninfo* screenInfo)
    {
        _DEBUG("%s", "ioctl /dev/fb0 FBIOPUT_VSCREENINFO");
        // TODO - Explore allowing some screen info updating
        _DEBUG(
            "\n"
            "res: %dx%d\n"
            "res_virtual: %dx%d\n"
            "offset: %d, %d\n"
            "bits_per_pixel: %d\n"
            "grayscale: %d\n"
            "red: %d, %d, %d\n"
            "green: %d, %d, %d\n"
            "blue: %d, %d, %d\n"
            "tansp: %d, %d, %d\n"
            "nonstd: %d\n"
            "activate: %d\n"
            "size: %dx%dmm\n"
            "accel_flags: %d\n"
            "pixclock: %d\n"
            "margins: %d %d %d %d\n"
            "hsync_len: %d\n"
            "vsync_len: %d\n"
            "sync: %d\n"
            "vmode: %d\n"
            "rotate: %d\n"
            "colorspace: %d\n"
            "reserved: %d, %d, %d, %d\n",
            screenInfo->xres,
            screenInfo->yres,
            screenInfo->xres_virtual,
            screenInfo->yres_virtual,
            screenInfo->xoffset,
            screenInfo->yoffset,
            screenInfo->bits_per_pixel,
            screenInfo->grayscale,
            screenInfo->red.offset,
            screenInfo->red.length,
            screenInfo->red.msb_right,
            screenInfo->green.offset,
            screenInfo->green.length,
            screenInfo->green.msb_right,
            screenInfo->blue.offset,
            screenInfo->blue.length,
            screenInfo->blue.msb_right,
            screenInfo->transp.offset,
            screenInfo->transp.length,
            screenInfo->transp.msb_right,
            screenInfo->nonstd,
            screenInfo->activate,
            screenInfo->height,
            screenInfo->width,
            screenInfo->accel_flags,
            screenInfo->pixclock,
            screenInfo->left_margin,
            screenInfo->right_margin,
            screenInfo->upper_margin,
            screenInfo->lower_margin,
            screenInfo->hsync_len,
            screenInfo->vsync_len,
            screenInfo->sync,
            screenInfo->vmode,
            screenInfo->rotate,
            screenInfo->colorspace,
            screenInfo->reserved[0],
            screenInfo->reserved[1],
            screenInfo->reserved[2],
            screenInfo->reserved[3]
        );
        // TODO allow resizing?
        return 0;
    }
    int ioctl(unsigned long request, char* ptr)
    {
        switch (request) {
            // Look at linux/fb.h and mxcfb.h for more possible request types
            // https://www.kernel.org/doc/html/latest/fb/api.html
            case MXCFB_SEND_UPDATE: {
                return send_update(reinterpret_cast<mxcfb_update_data*>(ptr));
            }
            case MXCFB_WAIT_FOR_UPDATE_COMPLETE: {
                return wait(reinterpret_cast<mxcfb_update_marker_data*>(ptr));
            }
            case FBIOGET_VSCREENINFO: {
                // TODO - handle getting information on rMPP/rMPPM
                int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
                if (fd == -1) {
                    return -1;
                }
                if (Libc::ioctl(fd, request, ptr) == -1) {
                    return -1;
                }
                Libc::close(fd);
                return get_vscreeninfo(
                    reinterpret_cast<fb_var_screeninfo*>(ptr)
                );
            }
            case FBIOGET_FSCREENINFO: {
                // TODO - handle getting information on rMPP/rMPPM
                int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
                if (fd == -1) {
                    return -1;
                }
                if (Libc::ioctl(fd, request, ptr) == -1) {
                    return -1;
                }
                Libc::close(fd);
                return get_fscreeninfo(
                    reinterpret_cast<fb_fix_screeninfo*>(ptr)
                );
            }
            case FBIOPUT_VSCREENINFO: {
                return put_vscreeninfo(
                    reinterpret_cast<fb_var_screeninfo*>(ptr)
                );
            }
            case MXCFB_SET_AUTO_UPDATE_MODE:
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_AUTO_UPDATE_MODE");
                return 0;
            case MXCFB_SET_UPDATE_SCHEME:
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_UPDATE_SCHEME");
                return 0;
            case MXCFB_ENABLE_EPDC_ACCESS:
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_ENABLE_EPDC_ACCESS");
                return 0;
            case MXCFB_DISABLE_EPDC_ACCESS:
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_DISABLE_EPDC_ACCESS");
                return 0;
            default:
                _WARN(
                    "UNHANDLED Fb IOCTL %lu %c %lu %lu %lu",
                    _IOC_DIR(request),
                    (char)_IOC_TYPE(request),
                    _IOC_NR(request),
                    _IOC_SIZE(request),
                    request
                );
                return 0;
        }
    }
    int exclusive_ioctl(unsigned long request, char* ptr)
    {
        switch (request) {
            // Look at linux/fb.h and mxcfb.h for more possible request types
            // https://www.kernel.org/doc/html/latest/fb/api.html
            case MXCFB_SEND_UPDATE: {
                // TODO handle region updates
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SEND_UPDATE");
                Blight::ClockWatch cw;
                mxcfb_update_data* update =
                    reinterpret_cast<mxcfb_update_data*>(ptr);
                auto region = update->update_region;
                Blight::exclusiveModeRepaint(
                    region.left,
                    region.top,
                    region.width,
                    region.height,
                    (Blight::WaveformMode)update->waveform_mode,
                    (Blight::UpdateMode)update->update_mode
                );
                _DEBUG(
                    "ioctl /dev/fb0 MXCFB_SEND_UPDATE done: %f", cw.elapsed()
                )
                return 0;
            }
            case MXCFB_WAIT_FOR_UPDATE_COMPLETE: {
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE");
                // TODO handle waiting
                return 0;
            }
            case FBIOGET_FSCREENINFO: {
                _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_FSCREENINFO");
                // TODO - handle getting information on rMPP/rMPPM
                int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
                if (fd == -1) {
                    return -1;
                }
                int res = Libc::ioctl(fd, request, ptr);
                Libc::close(fd);
                return res;
            }
            case FBIOGET_VSCREENINFO: {
                _DEBUG("%s", "ioctl /dev/fb0 FBIOGET_VSCREENINFO");
                // TODO - handle getting information on rMPP/rMPPM
                int fd = Libc::open("/dev/fb0", O_RDONLY, 0);
                if (fd == -1) {
                    return -1;
                }
                int res = Libc::ioctl(fd, request, ptr);
                Libc::close(fd);
                return res;
            }
            case FBIOPUT_VSCREENINFO: {
                return put_vscreeninfo(
                    reinterpret_cast<fb_var_screeninfo*>(ptr)
                );
            }
            case MXCFB_SET_AUTO_UPDATE_MODE:
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_AUTO_UPDATE_MODE");
                return 0;
            case MXCFB_SET_UPDATE_SCHEME:
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_SET_UPDATE_SCHEME");
                return 0;
            case MXCFB_ENABLE_EPDC_ACCESS:
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_ENABLE_EPDC_ACCESS");
                return 0;
            case MXCFB_DISABLE_EPDC_ACCESS:
                _DEBUG("%s", "ioctl /dev/fb0 MXCFB_DISABLE_EPDC_ACCESS");
                return 0;
            default:
                _WARN(
                    "UNHANDLED Fb IOCTL %lu %c %lu %lu %lu",
                    _IOC_DIR(request),
                    (char)_IOC_TYPE(request),
                    _IOC_NR(request),
                    _IOC_SIZE(request),
                    request
                );
                return 0;
        }
    }
}
