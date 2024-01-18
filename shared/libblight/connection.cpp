#include "connection.h"

#include <unistd.h>
#include <atomic>
#include <functional>
#include <sys/prctl.h>
#include <mutex>
#include <poll.h>
#include <sys/epoll.h>

static std::atomic<unsigned int> ackid;

namespace Blight{
    Connection::Connection(int fd)
    : m_fd(fcntl(fd, F_DUPFD_CLOEXEC, 3)),
      thread(run, std::ref(*this))
    {
    }

    Connection::~Connection(){
        stop_source.request_stop();
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

    const message_t* Connection::read(){
        auto message = new message_t;
        if(::read(m_fd, &message->header, sizeof(header_t)) < 0){
            delete message;
            return nullptr;
        }
        if(::read(m_fd, message->data, message->header.size) < 0){
            // In theory this could happen with a EAGAIN result,
            // but the server sends it all at once, so something
            // probably went wrong if this happens
            std::cerr
                << "Failed to read connection message body: "
                << std::strerror(errno)
                << std::endl;
            delete message;
            return nullptr;
        }
        return message;
    }
    int Connection::write(const message_t& message){
        auto res = ::write(m_fd, &message.header, sizeof(header_t));
        if(res < 0){
            std::cerr
                << "Failed to write connection message header: "
                << std::strerror(errno)
                << std::endl;
            return -errno;
        }
        if(res != sizeof(header_t)){
            std::cerr
                << "Failed to write connection message header: Size mismatch!"
                << std::endl;
            return -EMSGSIZE;
        }
        res = ::write(m_fd, message.data, message.header.size);
        if(res < 0){
            std::cerr
                << "Failed to write connection message data: "
                << std::strerror(errno)
                << std::endl;
            return -errno;
        }
        if(res != (ssize_t)message.header.size){
            std::cerr
                << "Failed to write connection message data: Size mismatch!"
                << std::endl;
            return EMSGSIZE;
        }
        return sizeof(header_t) + message.header.size;
    }
    bool Connection::send(MessageType type, data_t data, size_t size){
        auto _ackid = ++ackid;
        std::unique_lock lock(mutex);
        if(!thread.joinable()){
            lock.unlock();
            std::cerr
                << "Failed to write connection message: BlightWorker not running"
                << std::endl;
            return false;
        }
        std::condition_variable condition;
        acks[_ackid] = &condition;
        if(write(message_t{
            .header = {
                .type = type,
                .ackid = _ackid,
                .size = size
            },
            .data = data
        }) < 0){
            lock.unlock();
            std::cerr
                << "Failed to write connection message: "
                << std::strerror(errno)
                << std::endl;
            return false;
        }
        // Wait for ack from server
        condition.wait(lock);
        return true;
    }
    void Connection::repaint(std::string identifier, int x, int y, int width, int height){
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
        send(MessageType::Repaint, reinterpret_cast<data_t>(&buf), sizeof(buf));
    }
    void Connection::move(std::string identifier, int x, int y){
        unsigned char buf[sizeof(move_header_t) + identifier.size()];
        move_header_t header{
            .x = x,
            .y = y,
            .identifier_len = identifier.size()
        };
        memcpy(buf, &header, sizeof(header));
        memcpy(&buf[sizeof(header)], identifier.data(), identifier.size());
        send(MessageType::Move, reinterpret_cast<data_t>(&buf), sizeof(buf));
    }

    void Connection::run(Connection& connection){
        std::cerr
            << "[BlightWorker] Starting"
            << std::endl;
        prctl(PR_SET_NAME,"BlightWorker\0", 0, 0, 0);
        auto stop_token = connection.stop_source.get_token();
        std::vector<unsigned int> pending;
        int error = 0;
        pollfd pfd;
        pfd.fd = connection.m_fd;
        pfd.events = POLLIN;
        while(!stop_token.stop_requested()){
            // Handle any pending items that are now being waited on
            connection.mutex.lock();
            auto iter = pending.begin();
            while(iter != pending.end()){
                auto ackid = *iter;
                if(!connection.acks.contains(ackid)){
                    ++iter;
                    continue;
                }
                iter = pending.erase(iter);
                connection.acks[ackid]->notify_all();
                connection.acks.erase(ackid);
                std::cerr
                    << "[BlightWorker] Ack handled: "
                    << std::to_string(ackid)
                    << std::endl;
            }
            connection.mutex.unlock();
            // Commented out, as this for some reason will result in no
            // data ever being read.
            // // Wait for data on the socket, or for a 500ms timeout
            // auto res = poll(&pfd, 1, 500);
            // if(res == 0){
            //     continue;
            // }
            // if(pfd.revents & POLLHUP){
            //     std::cerr
            //         << "[BlightWorker] Failed to read message: Socket disconnected!"
            //         << std::endl;
            //     error = ECONNRESET;
            //     break;
            // }
            // if(!(pfd.revents & POLLIN)){
            //     std::cerr
            //         << "[BlightWorker] Unexpected revents:"
            //         << std::to_string(pfd.revents)
            //         << std::endl;
            //     continue;
            // }
            // TODO - see if epoll would work?
            auto message = connection.read();
            if(message == nullptr){
                if(errno == EAGAIN){
                    // Wait 10ms and try again
                    // This must use nanosleep, as using usleep
                    // results in no data ever being read.
                    timespec remaining;
                    timespec requested{
                        .tv_sec = 0,
                        .tv_nsec = 10000
                    };
                    nanosleep(&requested, &remaining);
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
                    auto ackid = message->header.ackid;
                    std::cerr
                        << "[BlightWorker] Ack received: "
                        << std::to_string(ackid)
                        << std::endl;
                    pending.push_back(ackid);
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
