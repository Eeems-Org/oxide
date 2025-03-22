#include "connection.h"
#include "libblight.h"
#include "debug.h"
#include "socket.h"
#include "concurrentqueue.h"
#include "clock.h"

#include <unistd.h>
#include <atomic>
#include <functional>
#include <sys/prctl.h>
#include <mutex>
#include <poll.h>
#include <sys/epoll.h>

using namespace std::chrono_literals;

namespace Blight{
    static std::atomic<unsigned int> ackid;
    static moodycamel::ConcurrentQueue<ackid_ptr_t> acks;
    ackid_t::ackid_t(
        unsigned int ackid,
        unsigned int data_size,
        data_t data
    )
    : ackid(ackid),
      done(false),
      data_size(data_size),
      data(data)
    {
#ifdef ACK_DEBUG
        _DEBUG("Ack created: %u", ackid);
#endif
    }

    ackid_t::~ackid_t(){
        if(!ackid){
            return;
        }
#ifdef ACK_DEBUG
        _DEBUG("Ack deleted: %u", ackid);
#endif
        notify_all();
    }

    bool ackid_t::wait(int timeout){
        if(!ackid){
#ifdef ACK_DEBUG
            _DEBUG("Can't wait for invalid ack");
#endif
            return true;
        }
#ifdef ACK_DEBUG
        _DEBUG("Waiting for ack: %u", ackid);
#endif
        std::unique_lock lock(mutex);
        if(timeout <= 0){
            condition.wait(lock, [this]{ return done; });
#ifdef ACK_DEBUG
            _DEBUG("Ack returned: %u", ackid);
#endif
            lock.unlock();
            return true;
        }
        bool res = condition.wait_for(
           lock,
           timeout * 1ms,
           [this]{ return done; }
        );
#ifdef ACK_DEBUG
        _DEBUG("Ack returned: %u", ackid);
#endif
        lock.unlock();
        return res;
    }

    void ackid_t::notify_all(){
#ifdef ACK_DEBUG
        _DEBUG("Ack notified: %u", ackid);
#endif
        std::lock_guard lock(mutex);
        condition.notify_all();
    }

    Connection::Connection(int fd)
    : m_fd(fcntl(fd, F_DUPFD_CLOEXEC, 3)),
      m_inputFd{-1},
      stop_requested(false),
      thread(run, this)
    {
        int flags = fcntl(fd, F_GETFD, NULL);
        fcntl(fd, F_SETFD, flags | O_NONBLOCK);
    }

    Connection::~Connection(){
        stop_requested = true;
        if(thread.joinable()){
            thread.join();
        }
        ::close(m_fd);
    }

    int Connection::handle(){ return m_fd; }

    int Connection::input_handle(){
        if(m_inputFd < 0){
            m_inputFd = Blight::open_input();
            if(m_inputFd < 0){
                _CRIT("Failed to open input stream: %s", std::strerror(errno));
            }
        }
        return m_inputFd;
    }

    void Connection::onDisconnect(std::function<void(int)> callback){
        disconnectCallbacks.push_back(callback);
    }

    std::optional<event_packet_t> Connection::read_event(){
        // Use input_handle() since we don't know if it's open yet
        auto fd = input_handle();
        if(fd < 0){
            _WARN("Failed to read event: %s", "Input stream not open");
            return {};
        }
        auto maybe = Blight::recv(fd, sizeof(event_packet_t));
        if(!maybe.has_value()){
            if(errno != EAGAIN && errno != EINTR){
                _WARN("Failed to read event: %s", std::strerror(errno));
            }
            return {};
        }
        event_packet_t data;
        memcpy(&data, maybe.value(), sizeof(event_packet_t));
        delete[] maybe.value();
        return data;
    }

    message_ptr_t Connection::read(){ return message_t::from_socket(m_fd); }

    maybe_ackid_ptr_t Connection::send(
        MessageType type,
        data_t data,
        size_t size,
        unsigned int __ackid
    ){
        auto _ackid = __ackid ? __ackid : ++ackid;
        auto ack = ackid_ptr_t(new ackid_t(_ackid));
        if(type != MessageType::Ack){
            // Adding acks to queue to make sure it's there by the time a response
            // comes back from the server
            acks.enqueue(ack);
#ifdef ACK_DEBUG
            _DEBUG("Ack enqueued: %u", _ackid);
#endif
        }else{
#ifdef ACK_DEBUG
            _DEBUG("No ack enqueue needed: %u", _ackid);
#endif
        }
        header_t header{
            {
                .type = type,
                .ackid = _ackid,
                .size = size
            }
        };
        if(!Blight::send_blocking(
            m_fd,
            reinterpret_cast<data_t>(&header),
            sizeof(header_t)
        )){
            _WARN("Failed to write connection message header: %s", std::strerror(errno));
            return {};
        }
        if(size && !Blight::send_blocking(m_fd, data, size)){
            _WARN("Failed to write connection message data: %s", std::strerror(errno));
            return {};
        }
#ifdef ACK_DEBUG
        _DEBUG("Sent: %u %d", _ackid, type);
#endif
        if(type == MessageType::Ack){
            // Clear the ackid so that it will not block on wait
            ack->ackid = 0;
        }
        return ack;
    }

    std::optional<std::chrono::duration<double>> Connection::ping(int timeout){
        ClockWatch cw;
        auto ack = send(MessageType::Ping, nullptr, 0);
        if(!ack.has_value()){
            _WARN("Failed to ping: %s", std::strerror(errno));
            return {};
        }
        if(!ack.value()->wait(timeout)){
            _WARN("Ping timed out");
            return {};
        }
        auto duration = cw.diff();
        _DEBUG("ping in %fs", duration.count());
        return duration;
    }

    void Connection::waitForMarker(unsigned int marker){
        auto maybe = send(MessageType::Wait, reinterpret_cast<data_t>(&marker), sizeof(marker));
        if(maybe.has_value()){
            maybe.value()->wait();
        }
    }

    maybe_ackid_ptr_t Connection::repaint(
        surface_id_t identifier,
        int x,
        int y,
        unsigned int width,
        unsigned int height,
        WaveformMode waveform,
        unsigned int marker
    ){
        if(!identifier){
            errno = EINVAL;
            return {};
        }
        repaint_t repaint{
            {
                .x = x,
                .y = y,
                .width = width,
                .height = height,
                .waveform = waveform,
                .marker = marker,
                .identifier = identifier,
            }
        };
        auto ackid = send(MessageType::Repaint, (data_t)&repaint, sizeof(repaint));
        if(!ackid.has_value()){
            return {};
        }
        return ackid;
    }

    void Connection::move(shared_buf_t buf, int x, int y){
        auto ack =  move(buf->surface, x, y);
        if(ack.has_value()){
            ack.value()->wait();
            buf->x = x;
            buf->y = y;
        }
    }

    std::optional<shared_buf_t> Connection::resize(
        shared_buf_t buf,
        int width,
        int height,
        int stride,
        data_t new_data
    ){
        if(!buf->surface){
            errno = EINVAL;
            return {};
        }
        auto res = Blight::createBuffer(
            buf->x,
            buf->y,
            width,
            height,
            stride,
            buf->format
        );
        if(!res.has_value()){
            return {};
        }
        auto buf2 = res.value();
        memcpy(buf2->data, new_data, buf2->size());
        Blight::addSurface(buf2);
        if(!buf2->surface){
            return {};
        }
        auto ack = remove(buf);
        if(ack.has_value()){
            ack.value()->wait();
        }
        return buf2;
    }

    maybe_ackid_ptr_t Connection::raise(surface_id_t identifier){
        if(!identifier){
            errno = EINVAL;
            return {};
        }
        return send(MessageType::Raise, (data_t)&identifier, sizeof(surface_id_t));
    }

    maybe_ackid_ptr_t Connection::raise(shared_buf_t buf){
        if(buf == nullptr){
            errno = EINVAL;
            return {};
        }
        return raise(buf->surface);
    }

    maybe_ackid_ptr_t Connection::lower(surface_id_t identifier){
        if(!identifier){
            errno = EINVAL;
            return {};
        }
        return send(MessageType::Lower, (data_t)&identifier, sizeof(surface_id_t));
    }

    maybe_ackid_ptr_t Connection::lower(shared_buf_t buf){
        if(buf == nullptr){
            errno = EINVAL;
            return {};
        }
        return lower(buf->surface);
    }

    maybe_ackid_ptr_t Connection::move(surface_id_t identifier, int x, int y){
        if(!identifier){
            errno = EINVAL;
            return {};
        }
        move_t move{
            {
                .identifier = identifier,
                .x = x,
                .y = y,
            }
        };
        return send(MessageType::Move, (data_t)&move, sizeof(move));
    }

    std::optional<shared_buf_t> Connection::getBuffer(surface_id_t identifier){
        if(!identifier){
            _WARN("Failed to get buffer for surface %hu: Invalid identifier", identifier);
            return {};
        }
        int fd = getSurface(identifier);
        if(fd == -1){
            _WARN("Failed to get buffer for surface %hu: %s", identifier, std::strerror(errno));
            return {};
        }
        auto maybe = send(MessageType::Info, (data_t)&identifier, sizeof(surface_id_t));
        if(!maybe.has_value()){
            ::close(fd);
            _WARN("Failed to get info for surface %hu: %s", identifier, std::strerror(errno));
            return {};
        }
        auto ack = maybe.value();
        ack->wait();
        if(!ack->data_size){
            ::close(fd);
            _WARN("Failed to get info for surface %hu: It does not exist", identifier);
            return {};
        }
        if(ack->data_size < sizeof(surface_info_t)){
            _WARN(
                "Surface %d info size mismatch: size=%hu, expected=%u",
                identifier,
                ack->data_size,
                sizeof(surface_info_t)
            );
            ::close(fd);
            return {};
        }
        auto info = reinterpret_cast<surface_info_t*>(ack->data.get());
        auto buf = new buf_t{
            .fd = fd,
            .x = info->x,
            .y = info->y,
            .width = info->width,
            .height = info->height,
            .stride = info->stride,
            .format = info->format,
            .data = nullptr,
            .uuid = "",
            .surface = identifier
        };
        buf->data = new unsigned char[buf->size()];
        auto res = mmap(NULL, buf->size(), PROT_READ, MAP_SHARED_VALIDATE, fd, 0);
        if(res == MAP_FAILED){
            _WARN(
                "Failed to map buffer for surface: %hu error: %s",
                identifier,
                std::strerror(errno)
            );
            ::close(fd);
            delete buf->data;
            delete buf;
            return {};
        }
        buf->data = reinterpret_cast<data_t>(res);
        return shared_buf_t(buf);
    }

    std::vector<shared_buf_t> Connection::buffers(){
        std::vector<shared_buf_t> buffers;
        for(const auto& identifier : surfaces()){
            auto maybe = getBuffer(identifier);
            if(!maybe.has_value()){
                continue;
            }
            auto buf = maybe.value();
            if(buf->surface && buf->data != nullptr){
                buffers.push_back(maybe.value());
            }
        }
        return buffers;
    }

    maybe_ackid_ptr_t Connection::remove(shared_buf_t buf){
        if(buf == nullptr){
            return {};
        }
        auto ack = remove(buf->surface);
        if(!ack.has_value()){
            return {};
        }
        buf->surface = 0;
        return ack;
    }

    maybe_ackid_ptr_t Connection::remove(surface_id_t identifier){
        if(!identifier){
            errno = EINVAL;
            return {};
        }
        return send(MessageType::Delete, (data_t)&identifier, sizeof(surface_id_t));
    }

    std::vector<surface_id_t> Connection::surfaces(){
        auto maybe = send(MessageType::List, nullptr, 0);
        if(!maybe.has_value()){
            return std::vector<surface_id_t>();
        }
        auto ack = maybe.value();
        ack->wait();
        if(!ack->data_size){
            return std::vector<surface_id_t>();
        }
        if(ack->data == nullptr){
            _WARN("Missing list data pointer!");
            return std::vector<surface_id_t>();
        }
        auto buf = ack->data.get();
        return std::vector<surface_id_t>(buf, buf + (ack->data_size / sizeof(surface_id_t)));
    }

    void Connection::focused(){
        auto maybe = send(MessageType::Focus, nullptr, 0);
        if(!maybe.has_value()){
            return;
        }
        auto ack = maybe.value();
        ack->wait();
        return;
    }

    static std::atomic<bool> running = false;

    void Connection::run(Connection* connection){
        prctl(PR_SET_NAME, "BlightWorker\0", 0, 0, 0);
        if(running){
            _WARN("Already running");
            return;
        }
        running = true;
        _INFO("Starting");
        std::vector<std::shared_ptr<message_t>> completed;
        std::map<unsigned int, ackid_ptr_t> waiting;
        int error = 0;
        while(!connection->stop_requested){
            // Get any new waiting items
            {
                // Contain scope to avoid longer lived shared pointer
                ackid_ptr_t ptr;
                while(acks.try_dequeue(ptr)){
                    waiting[ptr->ackid] = ptr;
#ifdef ACK_DEBUG
                    _DEBUG("Ack dequeued: %u", ptr->ackid);
#endif
                }
            }
#ifdef ACK_DEBUG
            {
                std::string msg;
                auto iter = waiting.begin();
                while(iter != waiting.end()){
                    auto ackid = (*iter).first;
                    msg += std::to_string(ackid) + ",";
                    ++iter;
                }
                _DEBUG("Waiting acks: [%s]", msg.c_str());
            }
            {
                std::string msg;
                for(auto i = completed.begin(); i != completed.end(); ++i){
                    msg += std::to_string((*i)->header.ackid) + ",";
                }
                _DEBUG("Completed acks: [%s]", msg.c_str());
            }
#endif
            // Handle any pending items that are now being waited on
            if(!completed.empty()){
#ifdef ACK_DEBUG
                _DEBUG("Resolving %u waiting acks", completed.size());
#endif
                auto iter = completed.begin();
                while(iter != completed.end()){
                    auto message = *iter;
                    auto ackid = message->header.ackid;

                    if(!waiting.contains(ackid)){
                        ++iter;
                        continue;
                    }
                    ackid_ptr_t& ack = waiting[ackid];
                    if(message->header.size){
                        ack->data = message->data;
                        ack->data_size = message->header.size;
                    }
                    ack->done = true;
                    ack->notify_all();
                    iter = completed.erase(iter);
                    waiting.erase(ackid);
#ifdef ACK_DEBUG
                    _DEBUG("Ack handled: %u", ackid);
#endif
                }
            }
            auto message = connection->read();
            if(message->header.type == MessageType::Invalid){
                if(errno == EAGAIN){
                    continue;
                }
                _WARN("Failed to read message: %s", std::strerror(errno));
                error = errno;
                break;
            }
            switch(message->header.type){
                case MessageType::Ping:{
#ifdef ACK_DEBUG
                    _DEBUG("pong %u", message->header.ackid);
#endif
                    auto maybe = connection->send(
                        MessageType::Ack,
                        nullptr,
                        0,
                        message->header.ackid
                    );
                    if(!maybe.has_value()){
                        _WARN("Failed to ack ping: %s", std::strerror(errno));
                    }
                    break;
                }
                case MessageType::Ack:{
#ifdef ACK_DEBUG
                    auto ackid = message->header.ackid;
                    _DEBUG("Ack recieved: %u", ackid);
#endif
                    completed.push_back(message);
                    break;
                }
                default:
                    _WARN("Unexpected message type: %d", message->header.type);
            }
        }
        _INFO("Quitting");
        ackid_ptr_t ptr;
        while(acks.try_dequeue(ptr)){
            ptr->notify_all();
        }
        auto iter = waiting.begin();
        while(iter != waiting.end()){
            auto ack = (*iter).second;
            ack->notify_all();
            iter = waiting.erase(iter);
        }
        for(auto& callback : connection->disconnectCallbacks){
            callback(error);
        }
        running = false;
    }
}
