#include <QCommandLineParser>
#include <QTextStream>
#include <QDebug>
#include <QFile>
#include <mxcfb.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../shared/liboxide/meta.h"

QDebug operator<<(QDebug debug, const fb_bitfield& bitfield){
    QDebugStateSaver saver(debug);
    Q_UNUSED(saver);
    return debug.nospace()
        << "offset=" << bitfield.offset
        << " length=" << bitfield.length
        << " msb_right=" << bitfield.msb_right;
}

QDebug operator<<(QDebug debug, const fb_var_screeninfo& vinfo){
    QDebugStateSaver saver(debug);
    Q_UNUSED(saver);
    return debug.nospace()
        << "xres: " << vinfo.xres << Qt::endl
        << "yres: " << vinfo.yres << Qt::endl
        << "xres_virtual: " << vinfo.xres_virtual << Qt::endl
        << "yres_virtual: " << vinfo.yres_virtual << Qt::endl
        << "xoffset: " << vinfo.xoffset << Qt::endl
        << "yoffset: " << vinfo.yoffset << Qt::endl
        << "bits_per_pixel: " << vinfo.bits_per_pixel << Qt::endl
        << "grayscale: " << vinfo.grayscale << Qt::endl
        << "red: " << vinfo.red << Qt::endl
        << "green: " << vinfo.green << Qt::endl
        << "blue: " << vinfo.blue << Qt::endl
        << "transp: " << vinfo.transp << Qt::endl
        << "nonstd: " << vinfo.nonstd << Qt::endl
        << "activate: " << vinfo.activate << Qt::endl
        << "width: " << vinfo.width << Qt::endl
        << "height: " << vinfo.height << Qt::endl
        << "accel_flags: " << vinfo.accel_flags << Qt::endl
        << "pixclock: " << vinfo.pixclock << Qt::endl
        << "left_margin: " << vinfo.left_margin << Qt::endl
        << "right_margin: " << vinfo.right_margin << Qt::endl
        << "upper_margin: " << vinfo.upper_margin << Qt::endl
        << "lower_margin: " << vinfo.lower_margin << Qt::endl
        << "hsync_len: " << vinfo.hsync_len << Qt::endl
        << "vsync_len: " << vinfo.vsync_len << Qt::endl
        << "sync: " << vinfo.sync << Qt::endl
        << "vmode: " << vinfo.vmode << Qt::endl
        << "rotate: " << vinfo.rotate << Qt::endl
        << "colorspace: " << vinfo.colorspace << Qt::endl
        << "reserved 0: " << vinfo.reserved[0] << Qt::endl
        << "reserved 1: " << vinfo.reserved[1] << Qt::endl
        << "reserved 2: " << vinfo.reserved[2] << Qt::endl
        << "reserved 3: " << vinfo.reserved[3] << Qt::endl;
}

QString to_hex(unsigned long x){ return "0x" + QString::number(x, 16).toUpper(); }

QDebug operator<<(QDebug debug, const fb_fix_screeninfo& finfo){
    QDebugStateSaver saver(debug);
    Q_UNUSED(saver);
    return debug.nospace().noquote()
        << "id: " << finfo.id << Qt::endl
        << "smem_start: " << to_hex(finfo.smem_start) << Qt::endl
        << "smem_len: " << finfo.smem_len << Qt::endl
        << "type: " << finfo.type << Qt::endl
        << "type_aux: " << finfo.type_aux << Qt::endl
        << "visual: " << finfo.visual << Qt::endl
        << "xpanstep: " << finfo.xpanstep << Qt::endl
        << "ypanstep: " << finfo.ypanstep << Qt::endl
        << "ywrapstep: " << finfo.ywrapstep << Qt::endl
        << "line_length: " << finfo.line_length << Qt::endl
        << "mmio_start: " << to_hex(finfo.mmio_start) << Qt::endl
        << "mmio_len: " << finfo.mmio_len << Qt::endl
        << "accel: " << finfo.accel << Qt::endl
        << "capabilities: " << to_hex(finfo.capabilities) << Qt::endl
        << "reserved 0: " << finfo.reserved[0] << Qt::endl
        << "reserved 1: " << finfo.reserved[1] << Qt::endl;
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("fbinfo");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("a tool to get framebuffer information");
    parser.addVersionOption();
    auto path = "/dev/fb0";
    if(QString::fromLatin1(path).isEmpty()){
        qDebug() << "Unable to find framebuffer";
        return EXIT_FAILURE;
    }
    auto fd = open(path, O_RDONLY);
    if(fd == -1){
        qDebug() << "Failed to open framebuffer:" << strerror(errno);
        return errno;
    }
    fb_var_screeninfo vinfo;
    auto res = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
    auto err = errno;
    if(res == -1){
        qDebug() << "Failed to get variable screen info:" << strerror(err);
        return err;
    }
    fb_fix_screeninfo finfo;
    res = ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
    err = errno;
    close(fd);
    if(res == -1){
        qDebug() << "Failed to get fixed screen info:" << strerror(err);
        return err;
    }
    QFile file;
    file.open(stdout, QIODevice::WriteOnly);
    QDebug(&file).nospace()
        << "[framebuffer]" << Qt::endl
        << "path: " << path << Qt::endl
        << Qt::endl
        << "[fb_var_screeninfo]" << Qt::endl
        << vinfo
        << Qt::endl
        << "[fb_fix_screeninfo]" << Qt::endl
        << finfo;
    return EXIT_SUCCESS;
}
