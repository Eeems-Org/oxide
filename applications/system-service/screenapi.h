#ifndef SCREENSHOTAPI_H
#define SCREENSHOTAPI_H

#include <QObject>
#include <QDebug>
#include <QFile>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "apibase.h"
#include "mxcfb.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "fb2png.h"
#include "devicesettings.h"

#define DISPLAYWIDTH 1404
#define DISPLAYHEIGHT 1872
#define TEMP_USE_REMARKABLE_DRAW 0x0018
#define remarkable_color uint16_t
#define DISPLAYSIZE DISPLAYWIDTH * DISPLAYHEIGHT * sizeof(remarkable_color)

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
            return false;
        }
        int width, height, channels;
        auto decoded = (uint32_t*)stbi_load(path.toStdString().c_str(), &width, &height, &channels, 4);
        int target_width = DeviceSettings::instance().getFrameBufferWidth();
        int target_height = DeviceSettings::instance().getFrameBufferHeight();
        int fd = open("/dev/fb0", O_RDWR);
        auto ptr = (remarkable_color*)mmap(NULL,
              DeviceSettings::instance().getFrameBufferSize(),
              PROT_WRITE, MAP_SHARED, fd, 0);
        auto src = decoded;

        for(int j = 0; j < height; j++){
            if(j >= target_height){
              break;
            }
            for(int i = 0; i < width; i++){
              if(i >= target_width){
                break;
              }
              if(src[i] != 0){
                ptr[i] = (remarkable_color)src[i];
              }
            }
            ptr += target_width;
            src += width;
        }
        mxcfb_update_data update_data;
        mxcfb_rect update_rect;
        update_rect.top = 0;
        update_rect.left = 0;
        update_rect.width = DISPLAYWIDTH;
        update_rect.height = DISPLAYHEIGHT;
        update_data.update_marker = 0;
        update_data.update_region = update_rect;
        update_data.waveform_mode = WAVEFORM_MODE_AUTO;
        update_data.update_mode = UPDATE_MODE_FULL;
        update_data.dither_mode = EPDC_FLAG_USE_DITHERING_MAX;
        update_data.temp = TEMP_USE_REMARKABLE_DRAW;
        update_data.flags = 0;
        ioctl(fd, MXCFB_SEND_UPDATE, &update_data);
        free(decoded);
        close(fd);
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
