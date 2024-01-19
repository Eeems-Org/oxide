#include "connection.h"

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
    ackid_t::ackid_t(Connection* connection, unsigned int ackid)
    : ackid(ackid),
      connection(connection),
      waited(false)
    {}

    ackid_t::ackid_t(Connection* connection)
    : ackid_t(connection, 0)
    {}

    ackid_t::~ackid_t(){
        if(ackid && !waited){
            connection->mutex.lock();
            connection->acks[ackid] = nullptr;
            connection->mutex.unlock();
        }
    }
    bool ackid_t::wait(int timeout){
        waited = true;
        std::unique_lock lock(connection->mutex);
        if(connection->acks.contains(ackid)){
            auto condition = connection->acks[ackid];
            if(timeout <= 0){
                condition->wait(lock);
                return true;
            }
            return condition->wait_for(lock, timeout * 1ms) == std::cv_status::timeout;
        }
        std::condition_variable condition;
        connection->acks[ackid] = &condition;
        if(timeout <= 0){
            condition.wait(lock);
            return true;
        }
        return condition.wait_for(lock, timeout * 1ms) == std::cv_status::timeout;
    }
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

    message_t* Connection::read(){ return message_t::from_socket(m_fd); }
    int Connection::write(const message_t& message){
        auto res = ::send(m_fd, &message.header, sizeof(header_t), MSG_EOR);
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
        res = ::send(m_fd, message.data, message.header.size, MSG_EOR);
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
    ackid_t Connection::send(MessageType type, data_t data, size_t size){
        if(!thread.joinable()){
            std::cerr
                << "Failed to write connection message: BlightWorker not running"
                << std::endl;
            return ackid_t(this, 0);
        }
        auto _ackid = ++ackid;
        if(write(message_t{
            .header = {
                .type = type,
                .ackid = _ackid,
                .size = size
            },
            .data = data
        }) < 0){
            std::cerr
                << "Failed to write connection message: "
                << std::strerror(errno)
                << std::endl;
            return ackid_t(this, 0);
        }
        // std::cerr
        //     << "Sent: "
        //     << std::to_string(_ackid)
        //     << " "
        //     << std::to_string(type)
        //     << std::endl;
        return ackid_t(this, _ackid);
    }
    void Connection::waitFor(ackid_t ackid, int timeout){
        if(!ackid.ackid){
            std::cerr
                << "Null ackid provided"
                << std::endl;
            return;
        }
        // std::cerr
        //     << "Waiting for: "
        //     << std::to_string(ackid)
        //     << std::endl;
        std::unique_lock lock(mutex);
        std::condition_variable condition;
        acks[ackid.ackid] = &condition;
        if(timeout <= 0){
            condition.wait(lock);
        }else{
            auto res = condition.wait_for(lock, timeout * 1ms);
            if(res == std::cv_status::timeout){
                mutex.lock();
                acks.erase(ackid.ackid);
                mutex.unlock();
                return;
            }
        }
        if(!ackid.wait(timeout)){
            std::cerr
                << "Timed out waiting for: "
                << std::to_string(ackid.ackid)
                << std::endl;
            return;
        }
        // std::cerr
        //     << "Completed: "
        //     << std::to_string(ackid)
        //     << std::endl;
        return;
    }

    void Connection::waitForMarker(unsigned int marker){
        std::unique_lock lock(mutex);
        if(!markers.contains(marker)){
            lock.unlock();
            return;
        }
        auto ackid = markers[marker];
        markers.erase(marker);
        if(!acks.contains(ackid)){
            return;
        }
        auto condition = acks[ackid];
        if(condition != nullptr){
            condition->wait(lock);
            return;
        }
        {
            std::condition_variable condition;
            acks[ackid] = &condition;
            condition.wait(lock);
        }
    }
    ackid_t Connection::repaint(
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
        auto ackid = send(MessageType::Repaint, reinterpret_cast<data_t>(&buf), sizeof(buf));
        if(marker){
            markers[marker] = ackid.ackid;
        }
        return ackid;
    }
    ackid_t Connection::move(std::string identifier, int x, int y){
        unsigned char buf[sizeof(move_header_t) + identifier.size()];
        move_header_t header{
            .x = x,
            .y = y,
            .identifier_len = identifier.size()
        };
        memcpy(buf, &header, sizeof(header));
        memcpy(&buf[sizeof(header)], identifier.data(), identifier.size());
        return send(MessageType::Move, reinterpret_cast<data_t>(&buf), sizeof(buf));
    }

    void Connection::run(Connection& connection){
        std::cerr
            << "[BlightWorker] Starting"
            << std::endl;
        prctl(PR_SET_NAME,"BlightWorker\0", 0, 0, 0);
        std::vector<unsigned int> pending;
        int error = 0;
        while(!connection.stop_requested){
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
                auto condition = connection.acks[ackid];
                if(condition != nullptr){
                    condition->notify_all();
                }
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
            if(message == nullptr){
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
                    auto ackid = message->header.ackid;
                    // std::cerr
                    //     << "[BlightWorker] Ack received: "
                    //     << std::to_string(ackid)
                    //     << std::endl;
                    pending.push_back(ackid);
                    break;
                }
                default:
                    std::cerr
                        << "[BlightWorker] Unexpected message type: "
                        << std::to_string(message->header.type)
                        << std::endl;
            }
            if(message->data != nullptr){
                delete message->data;
                message->data = nullptr;
            }
            delete message;
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
