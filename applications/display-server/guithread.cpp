#include "guithread.h"
#ifdef EPAPER
#include "connection.h"
#include "dbusinterface.h"

#include <epframebuffer.h>
#include <QAbstractEventDispatcher>
#include <QTimer>
#include <QPainter>
#include <fcntl.h>
#include <mxcfb.h>
#include <liboxide/debug.h>
#include <liboxide/threading.h>
#include <libblight/clock.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <functional>

#ifdef DEBUG
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <cxxabi.h>
void print_trace() {
    unw_cursor_t cursor;
    unw_context_t context;

           // Initialize cursor to current frame for local unwinding.
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

           // Unwind frames one by one, going up the frame stack.
    while(unw_step(&cursor) > 0){
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if(pc == 0){
            break;
        }
        std::printf("0x%lx:", pc);

        char sym[256];
        if(unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0){
            char* nameptr = sym;
            int status;
            char* demangled = abi::__cxa_demangle(sym, nullptr, nullptr, &status);
            if (status == 0) {
                nameptr = demangled;
            }
            std::printf(" (%s+0x%lx) ", nameptr, offset);
            std::free(demangled);
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "addr2line -e /proc/%d/exe %p", getpid(), (void*)pc);
            system(cmd);
            std::printf("\n");
        } else {
            std::printf(" -- error: unable to obtain symbol name for this frame\n");
        }
    }
}
#endif

static thread_local sigjmp_buf jumpBuffer;
static thread_local bool catchExceptions;
static struct sigaction oldSigSegv;
static struct sigaction oldSigBus;
static void __ex_handle__(int sig, siginfo_t* info, void* context){
    Q_UNUSED(sig);
    Q_UNUSED(info);
    Q_UNUSED(context);
    if(catchExceptions){
        siglongjmp(jumpBuffer, 1);
    }else if(sig == SIGSEGV && oldSigSegv.sa_sigaction){
#ifdef DEBUG
        print_trace();
#endif
        oldSigSegv.sa_sigaction(sig, info, context);
    }else if(sig == SIGBUS && oldSigBus.sa_sigaction){
#ifdef DEBUG
        print_trace();
#endif
        oldSigBus.sa_sigaction(sig, info, context);
    }
}
#define noop
#define TRY(expression, c_expression) \
    { \
        catchExceptions = true; \
        if(setjmp(jumpBuffer) == 0){ \
            expression; \
        }else{ \
            c_expression; \
        } \
        catchExceptions = false; \
    }

void GUIThread::run(){
    O_DEBUG("Thread started");
    clearFrameBuffer();
    QTimer::singleShot(0, this, [this]{
        Q_ASSERT(QThread::currentThread() == (QThread*)this);
        QMutexLocker locker(&m_repaintMutex);
        Q_UNUSED(locker);
        while(!isInterruptionRequested()){
            // New repaint request each loop as we have a shared pointer we need to clear
            RepaintRequest event;
            if(!m_repaintEvents.try_dequeue(event)){
                dbusInterface->processRemovedSurfaces();
                dbusInterface->processClosingConnections();
                emit settled();
                // Wait for up to 500ms before trying again
                m_repaintWait.wait(&m_repaintMutex, 500);
                auto found = m_repaintEvents.try_dequeue(event);
                if(!found){
                    // Woken by something needing to cleanup connections/surfaces
                    continue;
                }
            }
            do{
                redraw(event);
                if(event.callback != nullptr){
                    event.callback();
                }
            }while(m_repaintEvents.try_dequeue(event));
            eventDispatcher()->processEvents(QEventLoop::AllEvents);
        }
    });
    clearFrameBuffer();
    auto res = exec();
    O_DEBUG("Thread stopped with exit code:" << res);
}

GUIThread* GUIThread::singleton(){
    static GUIThread* instance = nullptr;
    if(instance == nullptr){
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = __ex_handle__;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, &oldSigSegv);
        sigaction(SIGBUS, &sa, &oldSigBus);
        instance = new GUIThread(EPFrameBuffer::instance()->framebuffer()->rect());
        Oxide::startThreadWithPriority(instance, QThread::TimeCriticalPriority);
    }
    return instance;
}

GUIThread::GUIThread(QRect screenGeometry)
: QThread(),
  m_screenGeometry{screenGeometry},
  m_screenOffset{screenGeometry.topLeft()},
  m_screenRect{m_screenGeometry.translated(-m_screenOffset)}
{
    m_frameBufferFd = open("/dev/fb0", O_RDWR);
    if(m_frameBufferFd == -1){
        qFatal("Failed to open framebuffer");
    }
    moveToThread(this);
}

GUIThread::~GUIThread(){
    RepaintRequest event;
    while(m_repaintEvents.try_dequeue(event));
    requestInterruption();
    quit();
    wait();
}

void GUIThread::enqueue(
    std::shared_ptr<Surface> surface,
    QRect region,
    Blight::WaveformMode waveform,
    unsigned int marker,
    bool global,
    std::function<void()> callback
){
    if(isInterruptionRequested() || dbusInterface->inExclusiveMode()){
        if(callback != nullptr){
            callback();
        }
        return;
    }
    Q_ASSERT(global || surface != nullptr);
    QRect intersected;
    if(global){
        intersected = region;
    }else{
        if(!surface->visible()){
            O_WARNING("Surface is not currently visible" << surface->id());
            if(callback != nullptr){
                callback();
            }
            return;
        }
        auto surfaceGeometry = surface->geometry();
        intersected = region
            .translated(surfaceGeometry.topLeft())
            .intersected(surfaceGeometry)
            .intersected(m_screenRect);
    }
    if(intersected.isEmpty()){
        O_WARNING("Region does not intersect with screen" << surface->id() << region);
        if(callback != nullptr){
            callback();
        }
        return;
    }
    auto visibleSurfaces = this->visibleSurfaces();
    QRegion repaintRegion(intersected);
    if(!global){
        // Don't repaint portions covered by another surface that doesn't have alpha channel
        auto i = visibleSurfaces.constEnd();
        while(i != visibleSurfaces.constBegin()){
            --i;
            auto _surface = *i;
            if(surface == _surface){
                break;
            }
            bool hasAlpha = false;
            TRY(
                auto image = _surface->image();
                hasAlpha = image != nullptr && !image->isNull() && image->hasAlphaChannel(),
                noop
            )
            if(hasAlpha){
                continue;
            }
            auto geometry = _surface->geometry();
            repaintRegion -= region
                .intersected(geometry)
                .translated(-geometry.topLeft())
                .intersected(m_screenRect);
        }
    }
    if(repaintRegion.isEmpty()){
        O_WARNING("Region is currently covered" << surface->id() << region);
        if(callback != nullptr){
            callback();
        }
        return;
    }
    m_repaintEvents.enqueue(RepaintRequest{
        .surface = surface,
        .region = repaintRegion,
        .waveform = waveform,
        .marker = marker,
        .global = global,
        .callback = callback
    });
    notify();
}

void GUIThread::notify(){
    if(!dbusInterface->inExclusiveMode()){
        m_repaintWait.notify_one();
    }
}

void GUIThread::clearFrameBuffer(){
    EPFrameBuffer::instance()->framebuffer()->fill(Qt::white);
    EPFrameBuffer::sendUpdate(
        m_screenGeometry,
        EPFrameBuffer::Initialize,
        EPFrameBuffer::FullUpdate,
        true
    );
    EPFrameBuffer::sendUpdate(
        m_screenGeometry,
        EPFrameBuffer::HighQualityGrayscale,
        EPFrameBuffer::FullUpdate,
        true
    );
}

int GUIThread::framebuffer(){ return m_frameBufferFd; }


void GUIThread::repaintSurface(QPainter* painter, QRect* rect, std::shared_ptr<Surface> surface){
     // This should already be handled, but just in case it leaks
    if(surface->isRemoved()){
         return;
    }
    const QRect surfaceGeometry = surface->geometry();
    const QRect surfaceGlobalRect = surfaceGeometry.translated(-m_screenGeometry.topLeft());
    if(!rect->intersects(surfaceGlobalRect)){
        return;
    }
    TRY(
        {
            auto image = surface->image();
            if(image == nullptr || image->isNull()){
                return;
            }
            const QRect imageRect = rect
                ->translated(-surfaceGlobalRect.left(), -surfaceGlobalRect.top())
                .intersected(image->rect());
            const QRect sourceRect = rect->intersected(surfaceGlobalRect);
            if(
                imageRect.isEmpty()
                || !imageRect.isValid()
                || sourceRect.isEmpty()
                || !sourceRect.isValid()
            ){
                return;
            }
            O_DEBUG("Repaint surface" << surface->id() << sourceRect << imageRect);
            // TODO - See if there is a way to detect if there is just transparency in the region
            //        and don't mark this as repainted.
            painter->drawImage(sourceRect, *image.get(), imageRect);
        },
        O_WARNING("Failed to access surface buffer due to signal")
    );
}

void GUIThread::redraw(RepaintRequest& event){
    Q_ASSERT(QThread::currentThread() == (QThread*)this);
    if(dbusInterface->inExclusiveMode()){
        O_DEBUG("In exclusive mode, skipping redraw");
        return;
    }
    if(!event.global){
        if(event.surface == nullptr){
            O_WARNING("surface missing");
            return;
        }
        if(event.surface->isRemoved()){
            return;
        }
    }
    auto& region = event.region;
    if(region.isEmpty()){
        O_WARNING("Empty repaint region" << region);
        return;
    }
    Blight::ClockWatch cw;
    // Get visible region on the screen to repaint
    O_DEBUG("Repainting" << region.boundingRect());
    auto frameBuffer = EPFrameBuffer::instance()->framebuffer();
    Qt::GlobalColor colour = frameBuffer->hasAlphaChannel() ? Qt::transparent : Qt::white;
    QPainter painter(frameBuffer);
    while(!painter.isActive()){
        if(!eventDispatcher()->processEvents(QEventLoop::AllEvents)){
            QThread::msleep(1);
        }
        painter.begin(frameBuffer);
    }
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for(QRect rect : event.region){
        bool hasAlpha = false;
        TRY(
            {
                hasAlpha = event.global;
                if(!hasAlpha){
                    auto image = event.surface->image();
                    hasAlpha = image != nullptr
                        && !image->isNull()
                        && image->hasAlphaChannel();
                }
            },
            noop
        );
        if(hasAlpha){
            painter.fillRect(rect, colour);
            for(auto& surface : visibleSurfaces()){
                if(surface != nullptr){
                    repaintSurface(&painter, &rect, surface);
                }
            }
        }else{
            repaintSurface(&painter, &rect, event.surface);
        }
        sendUpdate(rect, event.waveform, event.marker);
    }
    painter.end();
    O_DEBUG("Repaint" << region.boundingRect() << "done in" << region.rectCount() << "paints, and" << cw.elapsed() << "seconds");
}

void GUIThread::sendUpdate(const QRect& rect, Blight::WaveformMode waveform, unsigned int marker){
    auto mode = rect == m_screenRect
        ? EPFrameBuffer::FullUpdate
        : EPFrameBuffer::PartialUpdate;
    O_DEBUG("Sending screen update" << rect << waveform << mode);
    mxcfb_update_data data{
        .update_region = mxcfb_rect{
            .top = (unsigned int)rect.top(),
            .left = (unsigned int)rect.left(),
            .width = (unsigned int)rect.width(),
            .height = (unsigned int)rect.height(),
        },
        .waveform_mode = waveform,
        .update_mode = mode,
        .update_marker = marker ? marker : ++m_currentMarker,
        // TODO allow this to be passed through
        .temp = 0x0018,
        .flags = 0,
        .dither_mode = 0,
        .quant_bit = 0,
        // TODO - explore this
        .alt_buffer_data = mxcfb_alt_buffer_data{},
    };
    ioctl(m_frameBufferFd, MXCFB_SEND_UPDATE, &data);
}

QList<std::shared_ptr<Surface>> GUIThread::visibleSurfaces(){
    auto visibleSurfaces = dbusInterface->visibleSurfaces();
    visibleSurfaces.erase(
        std::remove_if(
            visibleSurfaces.begin(),
            visibleSurfaces.end(),
            [](std::shared_ptr<Surface> surface){
                auto connection = surface->connection();
                if(!connection->isRunning()){
                    return true;
                }
                if(!surface->has("system") && getpgid(connection->pgid()) < 0){
                    O_WARNING(surface->id() << "With no running process");
                    return true;
                }
                auto image = surface->image();
                if(image == nullptr || image->isNull()){
                    O_WARNING(surface->id() << "Null framebuffer");
                    return true;
                }
                return false;
            }
        ),
        visibleSurfaces.end()
    );
    return visibleSurfaces;
}
#endif
