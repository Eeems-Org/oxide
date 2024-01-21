#include "connection.h"
#include "libblight.h"

#include <unistd.h>
#include <atomic>
#include <functional>
#include <sys/prctl.h>
#include <mutex>
#include <poll.h>
#include <sys/epoll.h>
#include <chrono>

using namespace std::chrono_literals;

static std::atomic<unsigned int> ackid;

namespace Blight{
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
    {}

    ackid_t::~ackid_t(){}

    bool ackid_t::wait(int timeout){
        if(!ackid){
            return true;
        }
        std::unique_lock lock(connection->mutex);
        if(done){
            lock.unlock();
            return true;
        }
        if(timeout <= 0){
            condition.wait(lock);
            return true;
        }
        return condition.wait_for(lock, timeout * 1ms) == std::cv_status::timeout;
    }

    void ackid_t::notify_all(){ condition.notify_all(); }

    Connection::Connection(int fd)
    : m_fd(fcntl(fd, F_DUPFD_CLOEXEC, 3)),
      thread(run, std::ref(*this)),
      stop_requested(false)
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

    void Connection::onDisconnect(std::function<void(int)> callback){
        mutex.lock();
        disconnectCallbacks.push_back(callback);
        mutex.unlock();
    }

    message_ptr_t Connection::read(){ return message_t::from_socket(m_fd); }

    maybe_ackid_ptr_t Connection::send(MessageType type, data_t data, size_t size){
        if(!thread.joinable()){
            std::cerr
                << "Failed to write connection message: BlightWorker not running"
                << std::endl;
            return {};
        }
        auto _ackid = ++ackid;
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
            std::cerr
                << "Failed to write connection message header: "
                << std::strerror(errno)
                << std::endl;
            return {};
        }
        if(res != sizeof(header_t)){
            std::cerr
                << "Failed to write connection message header: Size mismatch!"
                << std::endl;
            errno = EMSGSIZE;
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
            std::cerr
                << "Failed to write connection message data: "
                << std::strerror(errno)
                << std::endl;
            return {};
        }
        if(res != (ssize_t)size){
            std::cerr
                << "Failed to write connection message data: Size mismatch!"
                << std::endl;
            errno = EMSGSIZE;
            return {};
        }

        // std::cerr
        //     << "Sent: "
        //     << std::to_string(_ackid)
        //     << " "
        //     << std::to_string(type)
        //     << std::endl;
        mutex.lock();
        auto ack = ackid_ptr_t(new ackid_t(this, _ackid));
        acks[_ackid] = ack;
        mutex.unlock();
        return ack;
    }

    void Connection::waitForMarker(unsigned int marker){
        ackid_ptr_t ack;
        {
            std::scoped_lock lock(mutex);
            if(!markers.contains(marker)){
                return;
            }
            auto ackid = markers[marker];
            markers.erase(marker);
            if(!acks.contains(ackid)){
                return;
            }
            ack = acks[ackid];
        }
        ack->wait();
    }

    maybe_ackid_ptr_t Connection::repaint(
        std::string identifier,
        int x,
        int y,
        int width,
        int height,
        unsigned int marker
    ){
        unsigned char buf[sizeof(repaint_header_t) + identifier.size()];
        repaint_header_t header{
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .identifier_len = identifier.size()
        };
        memcpy(buf, &header, sizeof(header));
        memcpy(&buf[sizeof(header)], identifier.data(), identifier.size());
        auto ackid = send(MessageType::Repaint, buf, sizeof(buf));
        if(!ackid.has_value()){
            return {};
        }
        if(marker){
            markers[marker] = ackid.value()->ackid;
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
            std::cerr
                << "Failed to get buffer for surface: "
                << identifier.c_str()
                << std::endl;
            return {};
        }
        auto buf = new buf_t{
            .fd = fd,
            .x = 0,
            .y = 0,
            .width = 0,
            .height = 0,
            .stride = 0,
            .format = Format::Format_Invalid,
            .data = nullptr,
            .uuid = "",
            .surface = identifier
        };
        auto ack = send(MessageType::Info, (data_t)identifier.data(), identifier.size());
        if(!ack.has_value()){
            ::close(fd);
            std::cerr
                << "Failed to get info for surface: "
                << identifier.c_str()
                << std::endl;
            return {};
        }
        ack.value()->wait();
        if(ack.value()->data_size < sizeof(surface_info_t)){
            std::cerr
                << "Surface info size mismatch for surface: "
                << identifier.c_str()
                << std::endl;
            ::close(fd);
            return {};
        }
        auto info = reinterpret_cast<surface_info_t*>(ack.value()->data.get());
        buf->x = info->x;
        buf->y = info->y;
        buf->width = info->width;
        buf->height = info->height;
        buf->stride = info->stride;
        buf->format = info->format;
        buf->data = new unsigned char[buf->size()];
        auto res = mmap(NULL, buf->size(), PROT_READ, MAP_SHARED_VALIDATE, fd, 0);
        if(res == MAP_FAILED){
            std::cerr
                << "Failed to map buffer for surface: "
                << identifier.c_str()
                << " error: "
                << std::to_string(errno)
                << std::endl;
            ::close(fd);
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
        auto ack = send(MessageType::Delete, (data_t)buf->surface.data(), buf->surface.size());
        buf->surface = "";
        return ack;
    }

    std::vector<std::string> Connection::surfaces(){
        auto ack = send(MessageType::List, nullptr, 0);
        if(!ack.has_value()){
            return std::vector<std::string>();
        }
        ack.value()->wait();
        if(ack.value()->data == nullptr){
            std::cerr
                << "[BlightWorker] Missing list data pointer!"
                << std::endl;
            return std::vector<std::string>();
        }
        return list_t::from_data(ack.value()->data.get()).identifiers();
    }

    int Connection::open_input(){

    }

    void Connection::run(Connection& connection){
        std::cerr
            << "[BlightWorker] Starting"
            << std::endl;
        prctl(PR_SET_NAME,"BlightWorker\0", 0, 0, 0);
        std::vector<std::shared_ptr<message_t>> pending;
        int error = 0;
        while(!connection.stop_requested){
            // Handle any pending items that are now being waited on
            connection.mutex.lock();
            auto iter = pending.begin();
            while(iter != pending.end()){
                auto message = *iter;
                auto ackid = message->header.ackid;
                if(!connection.acks.contains(ackid)){
                    ++iter;
                    continue;
                }
                iter = pending.erase(iter);
                auto ack = connection.acks[ackid];
                if(message->header.size){
                    ack->data = message->data;
                    ack->data_size = message->header.size;
                }
                ack->done = true;
                ack->notify_all();
                connection.acks.erase(ackid);
                std::map<unsigned int, unsigned int>::iterator iter2 = connection.markers.begin();
                while(iter2 != connection.markers.end()){
                    if(iter2->second == ackid){
                        iter2 = connection.markers.erase(iter2);
                        continue;
                    }
                    ++iter2;
                }
                // std::cerr
                //     << "[BlightWorker] Ack handled: "
                //     << std::to_string(ackid)
                //     << std::endl;
            }
            connection.mutex.unlock();
            // Wait for data on the socket, or for a 500ms timeout
            pollfd pfd;
            pfd.fd = connection.m_fd;
            pfd.events = POLLIN;
            auto res = poll(&pfd, 1, 500);
            if(res < 0){
                if(errno == EAGAIN || errno == EINTR){
                    continue;
                }
                std::cerr
                    << "[BlightWorker] Failed to poll connection socket:"
                    << std::strerror(errno)
                    << std::endl;
                error = errno;
                break;
            }
            if(res == 0){
                continue;
            }
            if(pfd.revents & POLLHUP){
                std::cerr
                    << "[BlightWorker] Failed to read message: Socket disconnected!"
                    << std::endl;
                error = ECONNRESET;
                break;
            }
            if(!(pfd.revents & POLLIN)){
                std::cerr
                    << "[BlightWorker] Unexpected revents:"
                    << std::to_string(pfd.revents)
                    << std::endl;
                continue;
            }
            auto message = connection.read();
            if(message->header.type == MessageType::Invalid){
                if(errno == EAGAIN){
                    continue;
                }
                std::cerr
                    << "[BlightWorker] Failed to read message: "
                    << std::strerror(errno)
                    << std::endl;
                error = errno;
                break;
            }
            switch(message->header.type){
                case MessageType::Ack:{
                    // auto ackid = message->header.ackid;
                    // std::cerr
                    //     << "[BlightWorker] Ack received: "
                    //     << std::to_string(ackid)
                    //     << std::endl;
                    pending.push_back(message);
                    break;
                }
                default:
                    std::cerr
                        << "[BlightWorker] Unexpected message type: "
                        << std::to_string(message->header.type)
                        << std::endl;
            }
        }
        std::cerr
            << "[BlightWorker] Quitting"
            << std::endl;
        connection.mutex.lock();
        // Release any pending acks
        for(auto& condition : connection.acks){
            condition.second->notify_all();
        }
        connection.acks.clear();
        for(auto& callback : connection.disconnectCallbacks){
            callback(error);
        }
        connection.mutex.unlock();
    }
}
