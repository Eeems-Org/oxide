#include "connection.h"
#include "libblight.h"
#include "debug.h"

#include <unistd.h>
#include <atomic>
#include <functional>
#include <sys/prctl.h>
#include <mutex>
#include <poll.h>
#include <sys/epoll.h>
#include <chrono>

using namespace std::chrono_literals;

namespace Blight{
    static std::atomic<unsigned int> ackid;
    static std::mutex ackMutex;
    static auto acks = new std::map<unsigned int, ackid_ptr_t>();
    static auto markers = new std::map<unsigned int, unsigned int>();

    ackid_t::ackid_t(
        Connection* connection,
        unsigned int ackid,
        unsigned int data_size,
        data_t data
    )
    : ackid(ackid),
      connection(connection),
      done(false),
      data_size(data_size),
      data(data)
    {
#ifdef ACK_DEBUG
        _DEBUG("Ack registered: %d", ackid);
#endif
    }

    ackid_t::~ackid_t(){
        notify_all();
#ifdef ACK_DEBUG
        _DEBUG("Ack deleted: %d", ackid);
#endif
    }

    bool ackid_t::wait(int timeout){
        if(!ackid){
            return true;
        }
        ackMutex.lock();
        if(!acks->contains(ackid)){
            ackMutex.unlock();
#ifdef ACK_DEBUG
            _WARN("Ack missing due to timing issues: %d", ackid);
#endif
            return true;
        }
#ifdef ACK_DEBUG
        _DEBUG("Waiting for ack: %d", ackid);
#endif
        std::unique_lock lock(mutex);
        ackMutex.unlock();
        if(timeout <= 0){
            condition.wait(lock, [this]{ return done; });
#ifdef ACK_DEBUG
            _DEBUG("Ack returned: %d", ackid);
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
        _DEBUG("Ack returned: %d", ackid);
#endif
        lock.unlock();
        return res;
    }

    void ackid_t::notify_all(){
#ifdef ACK_DEBUG
        _DEBUG("Ack notified: %d", ackid);
#endif
        std::lock_guard lock(mutex);
        condition.notify_all();
    }

    Connection::Connection(int fd)
    : m_fd(fcntl(fd, F_DUPFD_CLOEXEC, 3)),
      stop_requested(false),
      thread(run, this)
    {
        int flags = fcntl(fd, F_GETFD, NULL);
        fcntl(fd, F_SETFD, flags | O_NONBLOCK);
        m_inputFd = Blight::open_input();
        if(m_inputFd < 0){
            _CRIT("Failed to open input stream: %s", std::strerror(errno));
        }
    }

    Connection::~Connection(){
        stop_requested = true;
        if(thread.joinable()){
            thread.join();
        }
        ::close(m_fd);
    }

    int Connection::handle(){ return m_fd; }

    int Connection::input_handle(){ return m_inputFd; }

    void Connection::onDisconnect(std::function<void(int)> callback){
        ackMutex.lock();
        disconnectCallbacks.push_back(callback);
        ackMutex.unlock();
    }

    std::optional<event_packet_t> Connection::read_event(){
        if(m_inputFd < 0){
            _WARN("Failed to read event: %s", "Input stream not open");
            return {};
        }
        event_packet_t data;
        auto res = ::recv(m_inputFd, &data, sizeof(data), 0);
        if(res < 0){
            if(errno != EAGAIN && errno != EINTR){
                _WARN("Failed to read event: %s", std::strerror(errno));
            }
            return {};
        }
        if(res != sizeof(data)){
            _WARN("Failed to read event: Size mismatch! %d", res);
            return {};
        }
        return data;
    }

    message_ptr_t Connection::read(){ return message_t::from_socket(m_fd); }

    maybe_ackid_ptr_t Connection::send(MessageType type, data_t data, size_t size){
        // Adding acks to queue to make sure it's there by the time a response
        // comes back from the server
        ackMutex.lock();
        auto _ackid = ++ackid;
        auto ack = (*acks)[_ackid] = ackid_ptr_t(new ackid_t(this, _ackid));
        ackMutex.unlock();
        header_t header{
            .type = type,
            .ackid = _ackid,
            .size = size
        };
        int res = -1;
        while(res < 0){
            res = ::send(m_fd, &header, sizeof(header_t), MSG_EOR);
            if(res > -1){
                break;
            }
            if(errno == EAGAIN || errno == EINTR){
                timespec remaining;
                timespec requested{
                    .tv_sec = 0,
                    .tv_nsec = 5000
                };
                nanosleep(&requested, &remaining);
                continue;
            }
            break;
        }
        if(res < 0){
            _WARN("Failed to write connection message header: %s", std::strerror(errno));
            return {};
        }
        if(res != sizeof(header_t)){
             _WARN("Failed to write connection message header: %s", "Size mismatch!");
            errno = EMSGSIZE;
            // No need to erase the ack, something went really wrong
            return {};
        }
        res = -1;
        while(res < 0){
            res = ::send(m_fd, data, size, MSG_EOR);
            if(res > -1){
                break;
            }
            if(errno == EAGAIN || errno == EINTR){
                timespec remaining;
                timespec requested{
                    .tv_sec = 0,
                    .tv_nsec = 5000
                };
                nanosleep(&requested, &remaining);
                continue;
            }
            break;
        }
        if(res < 0){
            _WARN("Failed to write connection message data: %s", std::strerror(errno));
            return {};
        }
        if(res != (ssize_t)size){
            _WARN("Failed to write connection message data: %s", "Size mismatch!");
            errno = EMSGSIZE;
            // No need to erase the ack, something went really wrong
            return {};
        }
#ifdef ACK_DEBUG
        _DEBUG("Sent: %d %d", _ackid, type);
#endif
        return ack;
    }

    void Connection::waitForMarker(unsigned int marker){
        ackMutex.lock();
        if(!markers->contains(marker)){
            ackMutex.unlock();
            return;
        }
        auto ackid = markers->at(marker);
        markers->erase(marker);
        if(!acks->contains(ackid)){
            ackMutex.unlock();
            return;
        }
        auto ack = acks->at(ackid);
        ackMutex.unlock();
        ack->wait();
    }

    maybe_ackid_ptr_t Connection::repaint(
        std::string identifier,
        int x,
        int y,
        int width,
        int height,
        WaveformMode waveform,
        unsigned int marker
    ){
        unsigned char buf[sizeof(repaint_header_t) + identifier.size()];
        repaint_header_t header{
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .waveform = waveform,
            .identifier_len = identifier.size()
        };
        memcpy(buf, &header, sizeof(header));
        memcpy(&buf[sizeof(header)], identifier.data(), identifier.size());
        auto ackid = send(MessageType::Repaint, buf, sizeof(buf));
        if(!ackid.has_value()){
            return {};
        }
        if(marker){
            (*markers)[marker] = ackid.value()->ackid;
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
        if(buf2->surface.empty()){
            return {};
        }
        auto ack = remove(buf);
        if(ack.has_value()){
            ack.value()->wait();
        }
        return buf2;
    }

    maybe_ackid_ptr_t Connection::raise(std::string identifier){
        if(identifier.empty()){
            return {};
        }
        return send(MessageType::Raise, (data_t)identifier.data(), identifier.size());
    }

    maybe_ackid_ptr_t Connection::raise(shared_buf_t buf){
        if(buf == nullptr){
            return {};
        }
        return raise(buf->surface);
    }

    maybe_ackid_ptr_t Connection::lower(std::string identifier){
        if(identifier.empty()){
            return {};
        }
        return send(MessageType::Lower, (data_t)identifier.data(), identifier.size());
    }

    maybe_ackid_ptr_t Connection::lower(shared_buf_t buf){
        if(buf == nullptr){
            return {};
        }
        return lower(buf->surface);
    }

    maybe_ackid_ptr_t Connection::move(std::string identifier, int x, int y){
        unsigned char buf[sizeof(move_header_t) + identifier.size()];
        move_header_t header{
             .x = x,
             .y = y,
            .identifier_len = identifier.size()
        };
        memcpy(buf, &header, sizeof(header));
        memcpy(&buf[sizeof(header)], identifier.data(), identifier.size());
        return send(MessageType::Move, buf, sizeof(buf));
    }

    std::optional<shared_buf_t> Connection::getBuffer(std::string identifier){
        int fd = getSurface(identifier);
        if(fd == -1){
            _WARN("Failed to get buffer for surface: %s", identifier.c_str());
            return {};
        }
        auto ack = send(MessageType::Info, (data_t)identifier.data(), identifier.size());
        if(!ack.has_value()){
            ::close(fd);
            _WARN("Failed to get info for surface: %s", identifier.c_str());
            return {};
        }
        ack.value()->wait();
        if(ack.value()->data_size < sizeof(surface_info_t)){
            _WARN("Surface info size mismatch for surface: %s", identifier.c_str());
            ::close(fd);
            return {};
        }
        auto info = reinterpret_cast<surface_info_t*>(ack.value()->data.get());
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
                "Failed to map buffer for surface: %s error: %s",
                identifier.c_str(),
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
            auto buffer = getBuffer(identifier);
            if(buffer.has_value() && buffer.value()->data != nullptr){
                buffers.push_back(buffer.value());
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
        buf->surface = "";
        return ack;
    }

    maybe_ackid_ptr_t Connection::remove(std::string identifier){
        if(identifier.empty()){
            return {};
        }
        return send(MessageType::Delete, (data_t)identifier.data(), identifier.size());
    }

    std::vector<std::string> Connection::surfaces(){
        auto ack = send(MessageType::List, nullptr, 0);
        if(!ack.has_value()){
            return std::vector<std::string>();
        }
        ack.value()->wait();
        if(ack.value()->data == nullptr){
            _WARN("Missing list data pointer!");
            return std::vector<std::string>();
        }
        return list_t::from_data(ack.value()->data.get()).identifiers();
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
        std::vector<std::shared_ptr<message_t>> pending;
        int error = 0;
        while(!connection->stop_requested){
            // Handle any pending items that are now being waited on
            ackMutex.lock();
#ifdef ACK_DEBUG

            {
                std::string msg;
                for(auto i = acks->begin(); i != acks->end(); ++i){
                    msg += std::to_string(i->first) + ",";
                }
                _DEBUG("Waiting acks: [%s]", msg.c_str());
            }
            {
                std::string msg;
                for(auto i = pending.begin(); i != pending.end(); ++i){
                    msg += std::to_string((*i)->header.ackid) + ",";
                }
                _DEBUG("Completed acks: [%s]", msg.c_str());
            }
#endif
            if(!pending.empty()){
                auto iter = pending.begin();
                while(iter != pending.end()){
                    auto message = *iter;
                    auto ackid = message->header.ackid;
                    if(!acks->contains(ackid)){
                        ++iter;
#ifdef ACK_DEBUG
                        _WARN("Ack still pending: %d", ackid);
#endif
                        continue;
                    }
                    ackid_ptr_t ack = acks->at(ackid);;
                    if(message->header.size){
                        ack->data = message->data;
                        ack->data_size = message->header.size;
                    }
                    ack->done = true;
                    iter = pending.erase(iter);
                    acks->erase(ackid);
                    ack->notify_all();
                    auto iter2 = markers->begin();
                    while(iter2 != markers->end()){
                        if(iter2->second == ackid){
                            iter2 = markers->erase(iter2);
                            continue;
                        }
                        ++iter2;
                    }
#ifdef ACK_DEBUG
                    _DEBUG("Ack handled: %d", ackid);
#endif
                }
            }
            ackMutex.unlock();
            // Wait for data on the socket, or for a 500ms timeout
            pollfd pfd;
            pfd.fd = connection->m_fd;
            pfd.events = POLLIN;
            auto res = poll(&pfd, 1, 500);
            if(res < 0){
                if(errno == EAGAIN || errno == EINTR){
                    continue;
                }
                _WARN("Failed to poll connection socket: %s", std::strerror(errno));
                error = errno;
                break;
            }
            if(res == 0){
                continue;
            }
            if(pfd.revents & POLLHUP){
                _WARN("Failed to read message: %s", "Socket disconnected!");
                std::cerr
                    << "[BlightWorker] Failed to read message: Socket disconnected!"
                    << std::endl;
                error = ECONNRESET;
                break;
            }
            if(!(pfd.revents & POLLIN)){
                _WARN("Unexpected revents: %d", pfd.revents);
                continue;
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
                case MessageType::Ack:{
#ifdef ACK_DEBUG
                    auto ackid = message->header.ackid;
                    _DEBUG("Ack recieved: %d", ackid);
#endif
                    pending.push_back(message);
                    break;
                }
                default:
                    _WARN("Unexpected message type: %d", message->header.type);
            }
        }
        _INFO("Quitting");
        ackMutex.lock();
        // Release any pending acks
        for(auto& condition : *acks){
            condition.second->notify_all();
        }
        acks->clear();
        for(auto& callback : connection->disconnectCallbacks){
            callback(error);
        }
        ackMutex.unlock();
        running = false;
    }
}
