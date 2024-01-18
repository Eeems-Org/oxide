#include <QCommandLineParser>
#include <QTextStream>
#include <QDebug>

#include <liboxide/meta.h>
#include <liboxide/sentry.h>
#include <liboxide/devicesettings.h>
#include <epframebuffer.h>
#include <QFile>
#include <mxcfb.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

using namespace Oxide::Sentry;

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

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("fbinfo", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("notify-send");
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
    close(fd);
    if(res == -1){
        qDebug() << "Failed to get screen info:" << strerror(err);
        return err;
    }
    QFile file;
    file.open(stdout, QIODevice::WriteOnly);
    QDebug(&file)
        << "path:" << path << Qt::endl
        << vinfo;
    return EXIT_SUCCESS;
}
