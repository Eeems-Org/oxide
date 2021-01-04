#ifndef SCREENSHOTAPI_H
#define SCREENSHOTAPI_H

#include <QObject>
#include <QDebug>
#include <QFile>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <epframebuffer.h>

#include "apibase.h"
#include "mxcfb.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "fb2png.h"

#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872
#define TEMP_USE_REMARKABLE_DRAW 0x0018
#define remarkable_color uint16_t
#define DISPLAYSIZE DISPLAYWIDTH * DISPLAYHEIGHT * sizeof(remarkable_color)
#define RDISPLAYWIDTH 1408
#define RDISPLAYHEIGHT 1920
#define RDISPLAYSIZE RDISPLAYWIDTH * RDISPLAYHEIGHT * sizeof(remarkable_color)

#define screenAPI ScreenAPI::singleton()

class ScreenAPI : public APIBase {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", OXIDE_SCREEN_INTERFACE)
public:
    static ScreenAPI* singleton(ScreenAPI* self = nullptr){
        static ScreenAPI* instance;
        if(self != nullptr){
            instance = self;
        }
        return instance;
    }
    ScreenAPI(QObject* parent) : APIBase(parent) {
        singleton(this);
    }
    ~ScreenAPI(){}
    void setEnabled(bool enabled){
        qDebug() << "Screen API" << enabled;
    }

    Q_INVOKABLE bool drawFullscreenImage(QString path){
        if(!QFile(path).exists()){
            qDebug() << "Can't find image" << path;
            return false;
        }
        QImage img(path);
        if(img.isNull()){
            qDebug() << "Image data invalid" << path;
            return false;
        }
        auto size = EPFrameBuffer::framebuffer()->size();
        QRect rect(0, 0, size.width(), size.height());
        QPainter painter(EPFrameBuffer::framebuffer());
        painter.drawImage(rect, img);
        painter.end();
        EPFrameBuffer::sendUpdate(rect, EPFrameBuffer::HighQualityGrayscale, EPFrameBuffer::FullUpdate, true);
        EPFrameBuffer::waitForLastUpdate();
        return true;
    }

    Q_INVOKABLE int screenshot(){
        auto res = screenshot("/tmp/fb.png");
        if(res){
            qDebug() << "Failed to take screenshot" << res;
        }
        return res;
    }

private:
    static int screenshot(QString path){
        qDebug() << "Taking screenshot";
        std::string progname = "fb2png";
        std::string device = "/dev/fb0";
        return fb2png_exec(&progname[0], &device[0], path.toUtf8().data());
    }
};

#endif // SCREENSHOTAPI_H
