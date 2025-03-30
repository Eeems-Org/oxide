#include "libblight_protocol.h"
#include "socket.h"
#include "_debug.h"
#include "_concurrentqueue.h"

#include <fcntl.h>
#include <cstring>
#include <linux/socket.h>
#include <unistd.h>
#include <sys/mman.h>
#include <random>
#include <sstream>
#include <ios>
#include <thread>
#include <map>
#include <condition_variable>
#include <chrono>
#include <sys/prctl.h>
#include <shared_mutex>
#include <atomic>

using namespace std::chrono_literals;

#define XCONCATENATE(x, y) x ## y
#define CONCATENATE(x, y) XCONCATENATE(x, y)
#define UNIQ_T(x, uniq) CONCATENATE(__unique_prefix_, CONCATENATE(x, uniq))
#define UNIQ __COUNTER__
#define _STRV_FOREACH(s, l, i) for(typeof(*(l)) *s, *i = (l); (s = i) && *i; i++)
#define STRV_FOREACH(s, l) _STRV_FOREACH(s, l, UNIQ_T(i, UNIQ))

char** strv_free(char** v) {
    if(!v){
        return NULL;
    }
    for(char** i = v; *i; i++){
        free(*i);
    }
    free(v);
    return NULL;
}
inline const char* error_message(const sd_bus_error& err, int return_value){
    return err.message != nullptr ? err.message : std::strerror(-return_value);
}

std::string generate_uuid_v4(){
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_int_distribution<> dis(0, 15);
    thread_local std::uniform_int_distribution<> dis2(8, 11);
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    };
    return ss.str();
}

using namespace BlightProtocol;
extern "C" {
    int blight_bus_connect_system(blight_bus** bus){
        return sd_bus_default_system(bus);
    }
    int blight_bus_connect_user(blight_bus** bus){
        return sd_bus_default_user(bus);
    }
    void blight_bus_deref(blight_bus* bus){
        sd_bus_unref(bus);
    }
    bool blight_service_available(blight_bus* bus){
        char** names = NULL;
        char** activatable = NULL;
        int res = sd_bus_list_names(bus, &names, &activatable);
        if(res < 0){
            _WARN(
                "[blight_service_available::sd_bus_list_names] Error: %s",
                std::strerror(-res)
            );
            strv_free(names);
            strv_free(activatable);
            return false;
        }
        STRV_FOREACH(i, names){
            if(std::string(*i) == BLIGHT_SERVICE){
                strv_free(names);
                strv_free(activatable);
                return true;
            }
        }
        STRV_FOREACH(i, activatable){
            if(std::string(*i) == BLIGHT_SERVICE){
                strv_free(names);
                strv_free(activatable);
                return true;
            }
        }
        strv_free(names);
        strv_free(activatable);
        return false;
    }
    int blight_service_open(blight_bus* bus){
        if(!blight_service_available(bus)){
            errno = EAGAIN;
            return -EAGAIN;
        }
        sd_bus_error error{SD_BUS_ERROR_NULL};
        sd_bus_message* message = nullptr;
        int res = sd_bus_call_method(
            bus,
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "open",
            &error,
            &message,
            ""
        );
        if(res < 0){
            _WARN(
                "[blight_service_open::sd_bus_call_method(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int fd;
        res = sd_bus_message_read(message, "h", &fd);
        if(res < 0){
            _WARN(
                "[blight_service_open::sd_bus_message_read(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int dfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        int e = errno;
        if(message != nullptr){
            sd_bus_message_unref(message);
        }
        sd_bus_error_free(&error);
        if(dfd < 0){
            _WARN(
                "[blight_service_open::fcntl(...)] Error: %s",
                std::strerror(e)
            );
            errno = e;
            return -errno;
        }
        return dfd;
    }
    int blight_service_input_open(blight_bus* bus){
        if(!blight_service_available(bus)){
            errno = EAGAIN;
            return -EAGAIN;
        }
        sd_bus_error error{SD_BUS_ERROR_NULL};
        sd_bus_message* message = nullptr;
        int res = sd_bus_call_method(
            bus,
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "openInput",
            &error,
            &message,
            ""
        );
        if(res < 0){
            _WARN(
                "[blight_service_input_open::sd_bus_call_method(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int fd;
        res = sd_bus_message_read(message, "h", &fd);
        if(res < 0){
            _WARN(
                "[blight_service_input_open::sd_bus_message_read(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return res;
        }
        int dfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        int e = errno;
        if(message != nullptr){
            sd_bus_message_unref(message);
        }
        sd_bus_error_free(&error);
        if(dfd < 0){
            _WARN(
                "[blight_service_input_open::fcntl(...)] Error: %s",
                std::strerror(e)
            );
            errno = e;
            return -errno;
        }
        return dfd;
    }
    blight_header_t blight_header_from_data(blight_data_t data){
        blight_header_t header;
        memcpy(&header, data, sizeof(blight_header_t));
        return header;
    }
    blight_message_t* blight_message_from_data(blight_data_t data){
        auto message = new blight_message_t;
        message->header = blight_header_from_data(data);
        if(message->header.size){
            message->data = new unsigned char[message->header.size];
            memcpy(message->data, &data[sizeof(message->header)], message->header.size);
        }else{
            message->data = nullptr;
        }
        return message;
    }
    int blight_message_from_socket(int fd, blight_message_t** message){
        auto maybe = recv(fd, sizeof(blight_header_t));
        if(!maybe.has_value()){
            if(errno != EAGAIN && errno != EINTR){
                _WARN(
                    "Failed to read connection message header: %s socket=%d",
                    std::strerror(errno),
                    fd
                );
            }
            return -errno;
        }
        auto m = new blight_message_t{
            .header = blight_header_t{
              .type = BlightMessageType::Invalid,
              .ackid = 0,
              .size = 0
            },
            .data = nullptr
        };
        memcpy(&m->header, maybe.value(), sizeof(blight_header_t));
        delete[] maybe.value();
        if(!m->header.size){
            *message = m;
            return 0;
        }
        maybe = recv_blocking(fd, m->header.size);
        if(!maybe.has_value()){
            _WARN(
                "Failed to read connection message data: %s "
                "socket=%d, "
                "ackid=%u, "
                "type=%d, "
                "size=%ld",
                std::strerror(errno),
                fd,
                m->header.ackid,
                m->header.type,
                (long int)m->header.size
            );
            delete m;
            return -errno;
        }
        m->data = maybe.value();
        *message = m;
        return m->header.size;
    }
    void blight_message_deref(blight_message_t* message){
        if(message->data != nullptr){
            if(!message->header.size){
                _WARN("Freeing blight_message_t with a data pointer but a size of 0");
            }
            delete[] message->data;
        }
        delete message;
    }
}

struct ack_t{
    int fd;
    bool done;
    unsigned int ackid;
    uint32_t size = 0;
    blight_data_t data = nullptr;
    std::condition_variable condition;
    std::mutex mutex;
    ack_t(int fd, unsigned int ackid) : fd{fd}, ackid{ackid} { }
    ~ack_t(){
        if(data != nullptr && size > 0){
            delete[] data;
        }
    }
    bool wait(unsigned int timeout){
        std::unique_lock lock(mutex);
        if(done){
            return true;
        }
        if(timeout == 0){
            condition.wait(lock, [this]{ return done; });
            lock.unlock();
            return true;
        }
        bool res = condition.wait_for(
            lock,
            timeout * 1ms,
            [this]{ return done; }
        );
        lock.unlock();
        return res;
    }
    void notify_all(){ condition.notify_all(); }
};

static std::shared_mutex ackQueuesMutex;
static std::map<int, moodycamel::ConcurrentQueue<std::shared_ptr<ack_t>>*> ackQueues;

extern "C" {
    int blight_send_message(
        int fd,
        BlightMessageType type,
        unsigned int ackid,
        uint32_t size,
        blight_data_t data,
        int timeout,
        blight_data_t* response
    ){
        bool do_ack;
        std::shared_ptr<ack_t> ack;
        {
            std::shared_lock lock(ackQueuesMutex);
            do_ack = ackQueues.contains(fd);
            ack = std::shared_ptr<ack_t>(new ack_t(fd, ackid));
            if(do_ack && type != BlightMessageType::Ack){
                ackQueues.at(fd)->enqueue(ack);
            }
        }
        if(size && data == nullptr){
            _WARN("Data missing when size is not zero");
            errno = EINVAL;
            return -EINVAL;
        }
        blight_header_t header{
            .type = type,
            .ackid = ackid,
            .size = size
        };
        if(!send_blocking(fd, (blight_data_t)&header, sizeof(blight_header_t))){
            _WARN("Failed to write connection message header: %s", std::strerror(errno));
            return -errno;
        }
        if(size && !send_blocking(fd, data, size)){
            _WARN("Failed to write connection message data: %s", std::strerror(errno));
            return -errno;
        }
        if(timeout < 0 || !do_ack){
            return 0;
        }
        if(!ack->wait(timeout)){
            errno = ETIMEDOUT;
            return -errno;
        }
        *response = ack->data;
        ack->data = nullptr;
        return ack->size;
    }
    blight_buf_t* blight_create_buffer(
        int x,
        int y,
        unsigned int width,
        unsigned int height,
        unsigned int stride,
        BlightImageFormat format
    ){
        if(stride == 0 || width == 0 || height == 0){
            errno = EOVERFLOW;
            return nullptr;
        }
        int fd = memfd_create(generate_uuid_v4().c_str(), MFD_ALLOW_SEALING);
        if(fd > 0 && fd < 3){
            // Someone is doing something funky with with stdin/stdout/stderr
            // Lets force the fd to be 3 or higher
            int oldfd = fd;
            fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
            int e = errno;
            close(oldfd);
            errno = e;
        }
        if(fd < 0){
            return nullptr;
        }
        ssize_t size = stride * height;
        if(stride != 0 && size / stride != (size_t)height){
            errno = EOVERFLOW;
            close(fd);
            return nullptr;
        }
        if(ftruncate(fd, size)){
            int e = errno;
            close(fd);
            errno = e;
            return nullptr;
        }
        void* data = mmap(
            NULL,
            size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED_VALIDATE,
            fd,
            0
        );
        if(data == MAP_FAILED || data == nullptr){
            int e = errno;
            close(fd);
            errno = e;
            return nullptr;
        }
        return new blight_buf_t{
            .fd = fd,
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .stride = stride,
            .format = format,
            .data = (blight_data_t)data
        };
    }
    void blight_buffer_deref(blight_buf_t* buf){
        if(buf == nullptr){
            return;
        }
        if(buf->data != nullptr){
            ssize_t size = buf->stride * buf->height;
            if (munmap(buf->data, size) != 0) {
                _WARN("Failed to unmap buffer: %s", std::strerror(errno));
            }
            buf->data = nullptr;
        }
        if(buf->fd > 0){
            close(buf->fd);
            buf->fd = -1;
        }
        delete buf;
    }
    blight_surface_id_t blight_add_surface(blight_bus* bus, blight_buf_t* buf){
        if(!blight_service_available(bus)){
            errno = EAGAIN;
            return 0;
        }
        sd_bus_error error{SD_BUS_ERROR_NULL};
        sd_bus_message* message = nullptr;
        int res = sd_bus_call_method(
            bus,
            BLIGHT_SERVICE,
            "/",
            BLIGHT_INTERFACE,
            "addSurface",
            &error,
            &message,
            "hiiiiii",
            buf->fd,
            buf->x,
            buf->y,
            buf->width,
            buf->height,
            buf->stride,
            buf->format
        );
        if(res < 0){
            _WARN(
                "[blight_add_surface::sd_bus_call_method(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return 0;
        }
        blight_surface_id_t identifier;
        res = sd_bus_message_read(message, "q", &identifier);
        if(res < 0){
            _WARN(
                "[blight_add_surface::sd_bus_message_read(...)] Error: %s",
                error_message(error, res)
            );
            if(message != nullptr){
                sd_bus_message_unref(message);
            }
            sd_bus_error_free(&error);
            errno = -res;
            return 0;
        }
        return identifier;
    }
    blight_packet_repaint_t* blight_cast_to_repaint_packet(
        blight_message_t* message
    ){
        if(message == nullptr || message->header.type != Repaint){
            errno = EINVAL;
            return nullptr;
        }
        if(message->data == nullptr){
            errno = ENODATA;
            return nullptr;
        }
        if(message->header.size != sizeof(blight_packet_repaint_t)){
            errno = EMSGSIZE;
            return nullptr;
        }
        errno = 0;
        return (blight_packet_repaint_t*)message->data;
    }
    blight_packet_move_t* blight_cast_to_move_packet(
        blight_message_t* message
    ){
        if(message == nullptr || message->header.type != Move){
            errno = EINVAL;
            return nullptr;
        }
        if(message->data == nullptr){
            errno = ENODATA;
            return nullptr;
        }
        if(message->header.size != sizeof(blight_packet_move_t)){
            errno = EMSGSIZE;
            return nullptr;
        }
        errno = 0;
        return (blight_packet_move_t*)message->data;
    }
    blight_packet_surface_info_t* blight_cast_to_surface_info_packet(
        blight_message_t* message
    ){
        if(message == nullptr || message->header.type != Info){
            errno = EINVAL;
            return nullptr;
        }
        if(message->data == nullptr){
            errno = ENODATA;
            return nullptr;
        }
        if(message->header.size != sizeof(blight_packet_surface_info_t)){
            errno = EMSGSIZE;
            return nullptr;
        }
        errno = 0;
        return (blight_packet_surface_info_t*)message->data;
    }
    int blight_event_from_socket(int fd, blight_event_packet_t** packet){
        auto maybe = recv(fd, sizeof(blight_event_packet_t));
        if(!maybe.has_value()){
            return -errno;
        }
        *packet = (blight_event_packet_t*)maybe.value();
        return 0;
    }
}
struct surface_t{
    int fd;
    blight_surface_id_t identifier;
    blight_buf_t* buf;
};

void draw(struct _fbg* fbg){
    auto surface = static_cast<surface_t*>(fbg->user_context);
    memcpy(surface->buf->data, fbg->disp_buffer, fbg->size);
}
void flip(struct _fbg* fbg){
    auto surface = static_cast<surface_t*>(fbg->user_context);
    static unsigned int marker = 0;
    blight_packet_repaint_t repaint{
        .x = 0,
        .y = 0,
        .width = surface->buf->width,
        .height = surface->buf->height,
        .waveform = BlightWaveformMode::HighQualityGrayscale,
        .marker = ++marker,
        .identifier = surface->identifier
    };
    blight_data_t response = nullptr;
    int res = blight_send_message(
        surface->fd,
        BlightMessageType::Repaint,
        0,
        sizeof(blight_packet_repaint_t),
        (blight_data_t)&repaint,
        0,
        &response
    );
    if(res < 0){
        _WARN(
            "[blight_surface_to_fbg::flip(...)] Error: %s",
            std::strerror(errno)
        );
    }
    if(response != nullptr){
        delete[] response;
    }
}
void deref(struct _fbg* fbg){
    auto surface = static_cast<surface_t*>(fbg->user_context);
    blight_data_t response = nullptr;
    int res = blight_send_message(
        surface->fd,
        BlightMessageType::Delete,
        0,
        sizeof(blight_surface_id_t),
        (blight_data_t)&surface->identifier,
        0,
        &response
    );
    if(res < 0){
        _WARN(
            "[blight_surface_to_fbg::deref(...)] Error: %s",
            std::strerror(errno)
        );
    }
    if(response != nullptr){
        delete[] response;
    }
    blight_buffer_deref(surface->buf);
    delete surface;
}
extern "C" {
    struct _fbg* blight_surface_to_fbg(
        int fd,
        blight_surface_id_t identifier,
        blight_buf_t* buf
    ){
        if(buf->format != Format_RGB32){
            errno = EINVAL;
            return nullptr;
        }
        surface_t* surface = new surface_t{
            .fd = fd,
            .identifier = identifier,
            .buf = buf
        };
        _fbg* fbg = fbg_customSetup(
            buf->width,
            buf->height,
            3,
            true,
            true,
            (void*)surface,
            draw,
            flip,
            nullptr,
            deref
        );
        return fbg;
    }

    int blight_move_surface(
        int fd,
        blight_surface_id_t identifier,
        blight_buf_t* buf,
        int x,
        int y
    ){
        blight_packet_move_t packet{
            .identifier = identifier,
            .x = x,
            .y = y
        };
        blight_data_t response = nullptr;
        int res = blight_send_message(
            fd,
            BlightMessageType::Move,
            0,
            sizeof(blight_packet_move_t),
            (blight_data_t)&packet,
            0,
            &response
        );
        if(res >= 0){
            buf->x = x;
            buf->y = y;
        }
        if(response != nullptr){
            delete[] response;
        }
        return res;
    }
}

void connection_thread(
    int fd,
    std::atomic_bool* stop,
    std::mutex& mutex,
    std::condition_variable& condition
){
    prctl(PR_SET_NAME, "BlightWorker\0", 0, 0, 0);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    std::vector<blight_message_t*> completed;
    std::map<unsigned int, std::shared_ptr<ack_t>> waiting;
    moodycamel::ConcurrentQueue<std::shared_ptr<ack_t>> queue;
    {
        std::lock_guard lock(ackQueuesMutex);
        ackQueues[fd] = &queue;
    }
    {
        std::lock_guard lock(mutex);
        condition.notify_all();
        // mutex and condition are no longer safe to access after this
    }
    while(!*stop){
        std::shared_ptr<ack_t> ptr;
        while(queue.try_dequeue(ptr)){
            waiting[ptr->ackid] = ptr;
        }
        if(!completed.empty()){
            auto iter = completed.begin();
            while(iter != completed.end()){
                auto message = *iter;
                auto ackid = message->header.ackid;
                if(!waiting.contains(ackid)){
                    ++iter;
                    continue;
                }
                auto& ack = waiting[ackid];
                if(message->header.size){
                    ack->size = message->header.size;
                    ack->data = message->data;
                    message->data = nullptr; // Prevent double-free when message is dereferenced
                }
                ack->done = true;
                ack->notify_all();
                iter = completed.erase(iter);
                blight_message_deref(message); // Free the message after use
                waiting.erase(ackid);
            }
        }
        blight_message_t* message;
        int res = blight_message_from_socket(fd, &message);
        if(res < 0){
            if(errno == EAGAIN){
                continue;
            }
            _WARN(
                "[blight_start_connection_thread::connection_thread(...)] Error: %s",
                std::strerror(errno)
            );
            break;
        }
        switch(message->header.type){
            case BlightMessageType::Ping:{
                int res = blight_send_message(
                    fd,
                    BlightMessageType::Ack,
                    message->header.ackid,
                    0,
                    nullptr,
                    -1,
                    nullptr
                );
                if(res < 0){
                    _WARN("Failed to ack ping: %s", std::strerror(errno));
                }
                blight_message_deref(message);
                break;
            }
            case BlightMessageType::Ack:{
                completed.push_back(message);
                break;
            }
            default:
                _WARN("Unexpected message type: %d", message->header.type);
                blight_message_deref(message);
        }
    }
    {
        std::unique_lock lock(ackQueuesMutex);
        ackQueues.erase(fd);
    }
}

extern "C" {
    struct blight_thread_t{
        std::atomic_bool* stop;
        std::thread handle;
    };

    blight_thread_t* blight_start_connection_thread(int fd){
        std::shared_lock lock(ackQueuesMutex);
        if(ackQueues.contains(fd)){
            errno = EINVAL;
            return nullptr;
        }
        auto stop = new std::atomic_bool{false};
        blight_thread_t* thread;
        {
            std::mutex mutex;
            std::unique_lock ulock(mutex);
            std::condition_variable condition;
            thread = new blight_thread_t{
                .stop = stop,
                .handle = std::thread(
                    connection_thread,
                    fd,
                    stop,
                    std::ref(mutex),
                    std::ref(condition)
                )
            };
            lock.unlock();
            condition.wait(ulock);
        }
        return thread;
    }
    int blight_join_connection_thread(blight_thread_t* thread){
        if(thread == nullptr){
            errno = EINVAL;
            return -errno;
        }
        if(!thread->handle.joinable()){
            errno = EBADFD;
            return -errno;
        }
        thread->handle.join();
        return 0;
    }
    int blight_detach_connection_thread(blight_thread_t* thread){
        if(thread == nullptr){
            errno = EINVAL;
            return -errno;
        }
        thread->handle.detach();
        return 0;
    }
    int blight_stop_connection_thread(struct blight_thread_t* thread){
        if(thread == nullptr){
            errno = EINVAL;
            return -errno;
        }
        *thread->stop = true;
        return 0;
    }
    int blight_connection_thread_deref(blight_thread_t* thread){
        int res = blight_stop_connection_thread(thread);
        if(res != 0){
            return res;
        }
        res = blight_join_connection_thread(thread);
        if(res != 0){
            return res;
        }
        delete thread->stop;
        delete thread;
        return 0;
    }
}
