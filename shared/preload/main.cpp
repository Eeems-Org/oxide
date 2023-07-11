#include <iostream>
#include <unistd.h>
#include <dirent.h>
#include <chrono>
#include <signal.h>
#include <mxcfb.h>
#include <dlfcn.h>
#include <linux/fb.h>
#include <linux/ioctl.h>
#include <asm/ioctl.h>
#include <liboxide/meta.h>
#include <liboxide/debug.h>
#include <liboxide/devicesettings.h>
#include <liboxide/tarnish.h>

class ClockWatch {
public:
    std::chrono::high_resolution_clock::time_point t1;

    ClockWatch(){ t1 = std::chrono::high_resolution_clock::now(); }

    auto elapsed(){
        auto t2 = std::chrono::high_resolution_clock::now();
        auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
        return time_span.count();
    }
};

static bool IS_INITIALIZED = false;
static bool DEBUG_LOGGING = false;
static int argc;
static char** argv;
static int fbFd = -1;
static int touchFd = -1;
static int tabletFd = -1;
static int keyFd = -1;

void __sigacton__handler(int signo, siginfo_t* info, void* context){
    Q_UNUSED(info)
    Q_UNUSED(context)
    Oxide::Tarnish::disconnect();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    raise(signo);
}

int fb_ioctl(unsigned long request, char* ptr){
    switch(request){
        // Look at linux/fb.h and mxcfb.h for more possible request types
        // https://www.kernel.org/doc/html/latest/fb/api.html
        case MXCFB_SEND_UPDATE:{
            if(DEBUG_LOGGING){
                qDebug() << "ioctl /dev/fb0 MXCFB_SEND_UPDATE";
            }
            mxcfb_update_data* update = reinterpret_cast<mxcfb_update_data*>(ptr);
            auto region = update->update_region;
            Oxide::Tarnish::screenUpdate(QRect(region.top, region.left, region.width, region.height));
            return 0;
        }
        case MXCFB_WAIT_FOR_UPDATE_COMPLETE:{
            if(DEBUG_LOGGING){
                qDebug() << "ioctl /dev/fb0 MXCFB_WAIT_FOR_UPDATE_COMPLETE";
            }
            ClockWatch cz;
            if(DEBUG_LOGGING){
                qDebug() << "CLIENT: sync";
            }
            Oxide::Tarnish::waitForLastUpdate();
            if(DEBUG_LOGGING){
                qDebug() << "FINISHED WAIT IOCTL " << cz.elapsed();
            }
            return 0;
        }
        case FBIOGET_VSCREENINFO:{
            if(DEBUG_LOGGING){
                qDebug() << "ioctl /dev/fb0 FBIOGET_VSCREENINFO";
            }
            auto frameBuffer = Oxide::Tarnish::frameBufferImage(QImage::Format_RGB16);
            auto pixelFormat = frameBuffer.pixelFormat();
            fb_var_screeninfo* screenInfo = reinterpret_cast<fb_var_screeninfo*>(ptr);
            screenInfo->xres = frameBuffer.width();
            screenInfo->yres = frameBuffer.height();
            screenInfo->bits_per_pixel = pixelFormat.bitsPerPixel();
            screenInfo->grayscale = 0;
            screenInfo->xres_virtual = frameBuffer.width();
            screenInfo->yres_virtual = frameBuffer.height();
            // QImage::Format_RGB16
            screenInfo->red.offset = 11;
            screenInfo->red.length = 5;
            screenInfo->green.offset = 5;
            screenInfo->green.length = 6;
            screenInfo->blue.offset = 0;
            screenInfo->blue.length = 5;
            Q_UNUSED(screenInfo)
            Q_UNUSED(frameBuffer)
            return 0;
        }
        case FBIOGET_FSCREENINFO:{
            if(DEBUG_LOGGING){
                qDebug() << "ioctl /dev/fb0 FBIOGET_FSCREENINFO";
            }
            fb_fix_screeninfo* screeninfo = reinterpret_cast<fb_fix_screeninfo*>(ptr);
            auto frameBuffer = Oxide::Tarnish::frameBufferImage(QImage::Format_RGB16);
            screeninfo->smem_len = frameBuffer.sizeInBytes();
            screeninfo->smem_start = (unsigned long)Oxide::Tarnish::frameBuffer(QImage::Format_RGB16);
            screeninfo->line_length = frameBuffer.bytesPerLine();
            constexpr char fb_id[] = "mxcfb";
            memcpy(screeninfo->id, fb_id, sizeof(fb_id));
            return 0;
        }
        case FBIOPUT_VSCREENINFO:
            if(DEBUG_LOGGING){
                qDebug() << "ioctl /dev/fb0 FBIOPUT_VSCREENINFO";
            }
            // TODO - Explore allowing some screen info updating
            return -1;
        case MXCFB_SET_AUTO_UPDATE_MODE:
            if(DEBUG_LOGGING){
                qDebug() << "ioctl /dev/fb0 MXCFB_SET_AUTO_UPDATE_MODE";
            }
            return 0;
        case MXCFB_SET_UPDATE_SCHEME:
            if(DEBUG_LOGGING){
                qDebug() << "ioctl /dev/fb0 MXCFB_SET_UPDATE_SCHEME";
            }
            return 0;
        case MXCFB_ENABLE_EPDC_ACCESS:
            if(DEBUG_LOGGING){
                qDebug() << "ioctl /dev/fb0 MXCFB_ENABLE_EPDC_ACCESS";
            }
            return 0;
        case MXCFB_DISABLE_EPDC_ACCESS:
            if(DEBUG_LOGGING){
                qDebug() << "ioctl /dev/fb0 MXCFB_DISABLE_EPDC_ACCESS";
            }
            return 0;
        default:
            qDebug() << "UNHANDLED FB IOCTL " << _IOC_DIR(request) << (char)_IOC_TYPE(request) << QString::number(_IOC_NR(request), 16) << _IOC_SIZE(request) << request;
            return -1;
    }
}

int touch_ioctl(unsigned long request, char* ptr){
    Q_UNUSED(ptr)
    switch(request){
        default:
            qDebug() << "UNHANDLED TOUCH IOCTL " <<  _IOC_DIR(request) << (char)_IOC_TYPE(request) << QString::number(_IOC_NR(request), 16) << _IOC_SIZE(request) << request;
            return -1;
    }
}

int tablet_ioctl(unsigned long request, char* ptr){
    Q_UNUSED(ptr)
    switch(request){
        default:
            qDebug() << "UNHANDLED TABLET IOCTL " <<  _IOC_DIR(request) << (char)_IOC_TYPE(request) << QString::number(_IOC_NR(request), 16) << _IOC_SIZE(request) << request;
            return -1;
    }
}

int key_ioctl(unsigned long request, char* ptr){
    Q_UNUSED(ptr)
    switch(request){
        default:
            qDebug() << "UNHANDLED KEY IOCTL " << request;
            return -1;
    }
}

int open_from_tarnish(const char* pathname){
    if(!IS_INITIALIZED){
        return -2;
    }
    if(pathname == std::string("/dev/fb0")){
        return fbFd = Oxide::Tarnish::getFrameBufferFd(QImage::Format_RGB16);
    }else if(pathname == deviceSettings.getTouchDevicePath()){
        return touchFd = Oxide::Tarnish::getTouchEventPipeFd();
    }else if(pathname == deviceSettings.getWacomDevicePath()){
        return tabletFd = Oxide::Tarnish::getTabletEventPipeFd();
    }else if(pathname == deviceSettings.getButtonsDevicePath()){
        return keyFd = Oxide::Tarnish::getKeyEventPipeFd();
    }else{
        return -2;
    }
}

extern "C" {
//    __attribute__((visibility("default")))
//    void _ZN6QImageC1EiiNS_6FormatE(void* that, int x, int y, int f){
//        static bool FIRST_ALLOC = true;
//        static const auto qImageCtor = (void(*)(void*, int, int, int))dlsym(RTLD_NEXT, "_ZN6QImageC1EiiNS_6FormatE");
//        static const auto qImageCtorWithBuffer = (void(*)(
//            void*,
//            uint8_t*,
//            int32_t,
//            int32_t,
//            int32_t,
//            int,
//            void(*)(void*),
//            void*
//        ))dlsym(RTLD_NEXT, "_ZN6QImageC1EPhiiiNS_6FormatEPFvPvES2_");

//        auto frameBuffer = Oxide::Tarnish::frameBufferImage(QImage::Format_RGB16);
//        if(!FIRST_ALLOC || x != frameBuffer.width() || y != frameBuffer.height()){
//            qImageCtor(that, x, y, f);
//            return;
//        }
//        qDebug() << "REPLACING THE IMAGE with shared memory";
//        FIRST_ALLOC = false;
//        qImageCtorWithBuffer(
//            that,
//            (uint8_t *)Oxide::Tarnish::frameBuffer(QImage::Format_RGB16),
//            frameBuffer.width(),
//            frameBuffer.height(),
//            frameBuffer.sizeInBytes(),
//            frameBuffer.format(),
//            nullptr,
//            nullptr
//        );
//    }

    __attribute__((visibility("default")))
    int open64(const char* pathname, int flags, mode_t mode = 0){
        if(DEBUG_LOGGING){
            printf("open64 ");
            printf(pathname);
            printf("\n");
        }
        int fd = open_from_tarnish(pathname);
        if(fd == -2){
            static const auto func_open = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");
            fd = func_open(pathname, flags, mode);
        }
        if(DEBUG_LOGGING){
            printf("opened ");
            printf(pathname);
            printf(" with fd ");
            printf(std::to_string(fd).c_str());
            printf("\n");
        }
        return fd;
    }

    __attribute__((visibility("default")))
    int openat(int dirfd, const char* pathname, int flags, mode_t mode = 0){
        if(DEBUG_LOGGING){
            printf("openat ");
            printf(pathname);
            printf("\n");
        }
        int fd = open_from_tarnish(pathname);
        if(fd == -2){
            DIR* save = opendir(".");
            fchdir(dirfd);
            char path[PATH_MAX+1];
            getcwd(path, PATH_MAX);
            fchdir(::dirfd(save));
            closedir(save);
            fd = open_from_tarnish(QString("%1/%2").arg(path, pathname).toStdString().c_str());
        }
        if(fd == -2){
            static const auto func_open = (int(*)(int, const char*, int, mode_t))dlsym(RTLD_NEXT, "openat");
            fd = func_open(dirfd, pathname, flags, mode);
        }
        if(DEBUG_LOGGING){
            printf("opened ");
            printf(pathname);
            printf(" with fd ");
            printf(std::to_string(fd).c_str());
            printf("\n");
        }
        return fd;
    }

    __attribute__((visibility("default")))
    int open(const char* pathname, int flags, mode_t mode = 0){
        if(DEBUG_LOGGING){
            printf("open ");
            printf(pathname);
            printf("\n");
        }
        int fd = open_from_tarnish(pathname);
        if(fd == -2){
            static const auto func_open = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");
            fd = func_open(pathname, flags, mode);
        }
        if(DEBUG_LOGGING){
            printf("opened ");
            printf(pathname);
            printf(" with fd ");
            printf(std::to_string(fd).c_str());
            printf("\n");
        }
        return fd;
    }

    __attribute__((visibility("default")))
    int close(int fd){
        if(DEBUG_LOGGING){
            printf("close ");
            printf(std::to_string(fd).c_str());
            printf("\n");
        }
        if(fbFd != -1 && fd == fbFd){
            // Maybe actually close it?
            return 0;
        }
        if(touchFd != -1 && fd == touchFd){
            // Maybe actually close it?
            return 0;
        }
        if(tabletFd != -1 && fd == tabletFd){
            // Maybe actually close it?
            return 0;
        }
        if(keyFd != -1 && fd == keyFd){
            // Maybe actually close it?
            return 0;
        }
        static const auto func_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
        return func_close(fd);
    }

    __attribute__((visibility("default")))
    int ioctl(int fd, unsigned long request, char* ptr){
        if(IS_INITIALIZED){
            if(fbFd != -1 && fd == fbFd){
                return fb_ioctl(request, ptr);
            }
            if(touchFd != -1 && fd == touchFd){
                return touch_ioctl(request, ptr);
            }
            if(tabletFd != -1 && fd == tabletFd){
                return tablet_ioctl(request, ptr);
            }
            if(keyFd != -1 && fd == keyFd){
                return key_ioctl(request, ptr);
            }
        }
        static auto func_ioctl = (int(*)(int, unsigned long request, ...))dlsym(RTLD_NEXT, "ioctl");
        return func_ioctl(fd, request, ptr);
    }

    __attribute__((visibility("default")))
    bool _Z7qputenvPKcRK10QByteArray(const char* name, const QByteArray& val) {
        static const auto orig_fn = (bool(*)(const char*, const QByteArray&))dlsym(RTLD_NEXT, "_Z7qputenvPKcRK10QByteArray");
        if(strcmp(name, "QMLSCENE_DEVICE") == 0 || strcmp(name, "QT_QUICK_BACKEND") == 0){
            return orig_fn(name, "software");
        }
        if(strcmp(name, "QT_QPA_PLATFORM") == 0){
            return orig_fn(name, "epaper:enable_fonts");
        }
        return orig_fn(name, val);
    }

//    typedef uint32_t (*NotifyFunc)(void*, void*);

//    NotifyFunc f_notify = 0;

//    void new_update_int4(void* arg, int x1, int y1, int x2, int y2, int waveform, int flags) {
//        if(DEBUG_LOGGING){
//            qDebug() << "UPDATE HOOK CALLED" << Qt::endl
//                << "x " << x1 << " " << x2 << Qt::endl
//                << "y " << y1 << " " << y2 << Qt::endl
//                << "wav " << waveform << " flags " << flags;
//        }
//        // TODO - Allow requesting a specific waveform
//        Oxide::Tarnish::screenUpdate(QRect(QPoint(x1, y1), QPoint(x2, y2)));
//        if(f_notify != 0){
//            QRect someRect(x1, y1, x2-x1, y2-y1);
//            f_notify(arg, &someRect);
//        }
//    }

//    void new_update_QRect(void* arg, QRect& rect, int waveform, bool flags) {
//        new_update_int4(
//            arg,
//            rect.x(), rect.y(),
//            rect.x() + rect.width(),
//            rect.y() + rect.height(),
//            waveform,
//            flags
//        );
//    }

//    // this should work fine as a proof of concept, but for optimal performance, you
//    // could cache the sendUpdateMethod/newSendUpdate lookup done below.
//    const QMetaObject *metaObject = QMetaType::metaObjectForType(QMetaType::type("EPFramebuffer*"));
//    Q_ASSERT_X(metaObject, "", "EPFramebuffer is missing");

//    bool newSendUpdate = false;
//    std::optional<QMetaMethod> sendUpdateMethod;
//    for (int i = 0; i < metaObject->methodCount(); ++i) {
//        const auto &method = metaObject->method(i);
//        if (method.methodSignature() == "sendUpdate(QRect,EPFramebuffer::Waveform,EPFramebuffer::UpdateFlags)") {
//            newSendUpdate = true;
//            sendUpdateMethod = method;
//            break;
//        } else if (method.methodSignature() == "sendUpdate(QRect,EPFramebuffer::WaveformMode,EPFramebuffer::UpdateFlags)") {
//            sendUpdateMethod = method;
//            break;
//        }
//        // qDebug() << i << method.name() << method.methodSignature();
//    }
//    Q_ASSERT_X(sendUpdateMethod.has_value(), "", "EPFramebuffer::sendUpdate is missing");

//    // now actually make the call..
//    QRect rect;

//    int waveform = 0;
//    int flags = 0;

//    if (newSendUpdate) {
//        // 3.3+ here
//        // NOTE: the waveform values have changed from 3.3+
//        // 1 = mono (DU), 3 = greyscale
//        // so you might need to remap them appropriately..
//        qDebug() << "sendUpdate 3.3+";
//        QGenericArgument argWaveform("EPFramebuffer::Waveform", &waveform);
//        QGenericArgument argUpdateMode("EPFramebuffer::UpdateFlags", &flags);
//        sendUpdateMethod->invoke(instance, Q_ARG(QRect, rect), argWaveform, argUpdateMode);
//    } else {
//        // call earlier versions here
//        qDebug() << "sendUpdate old";
//        QGenericArgument argWaveform("EPFramebuffer::WaveformMode", &waveform);
//        QGenericArgument argUpdateMode("EPFramebuffer::UpdateFlags", &flags);
//        sendUpdateMethod->invoke(instance, Q_ARG(QRect, rect), argWaveform, argUpdateMode);
//    }

    static void _libhook_init() __attribute__((constructor));
    static void _libhook_init(){
        DEBUG_LOGGING = qEnvironmentVariableIsSet("OXIDE_PRELOAD_DEBUG");
        if(DEBUG_LOGGING){
            qDebug() << "Connecting to tarnish";
        }
        setenv("OXIDE_PRELOAD", "1", true);
        struct sigaction action;
        action.sa_flags = SA_SIGINFO | SA_RESETHAND;
        action.sa_sigaction = &__sigacton__handler;
        if(sigaction(SIGSEGV, &action, NULL) == -1){
            qFatal("Failed to connect SIGSEGV");
        }
        if(sigaction(SIGINT, &action, NULL) == -1){
            qFatal("Failed to connect SIGINT");
        }
        if(sigaction(SIGTERM, &action, NULL) == -1){
            qFatal("Failed to connect SIGTERM");
        }
        if(Oxide::Tarnish::getSocket() == nullptr){
            qFatal("Failed to connect to tarnish");
        }
        deviceSettings;
        if(DEBUG_LOGGING){
            qDebug() << "Connected to tarnish";
        }
        IS_INITIALIZED = true;
    }
    __attribute__((visibility("default")))
    int __libc_start_main(
        int(*_main)(int, char**, char**),
        int _argc,
        char** _argv,
        int(*init)(int, char**, char**),
        void(*fini)(void),
        void(*rtld_fini)(void),
        void* stack_end
    ){
        argc = _argc;
        argv = _argv;
        auto func_main = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");
        return func_main(_main, _argc, _argv, init, fini, rtld_fini, stack_end);
    }
}
