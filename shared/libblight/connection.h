#pragma once
#include "libblight_global.h"
#include "types.h"

#include <sys/socket.h>
#include <vector>
#include <thread>
#include <map>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace Blight {
    class Connection;
    typedef struct ackid_t{
        unsigned int ackid;
        Connection* connection;
        bool waited;
        unsigned int data_size;
        shared_data_t data;
        std::condition_variable condition;
        ackid_t(
            Connection* connection,
            unsigned int ackid = 0,
            unsigned int data_size = 0,
            data_t data = nullptr
        );
        bool wait(int timeout = 0);
        void notify_all();
    } ackid_t;
    typedef std::shared_ptr<ackid_t> ackid_ptr_t;
    class LIBBLIGHT_EXPORT Connection {
    public:
        Connection(int fd);
        ~Connection();
        void onDisconnect(std::function<void(int)> callback);
        message_ptr_t read();
        ackid_ptr_t send(MessageType type, data_t data, size_t size);
        void waitForMarker(unsigned int marker);
        ackid_ptr_t repaint(
            std::string identifier,
            int x,
            int y,
            int width,
            int height,
            unsigned int marker = 0
        );
        ackid_ptr_t move(std::string identifier, int x, int y);
        shared_buf_t getBuffer(std::string identifier);
        std::vector<shared_buf_t> buffers();

        std::mutex mutex;
        std::map<unsigned int, ackid_ptr_t> acks;
    private:
        std::map<unsigned int, unsigned int> markers;
        int m_fd;
        std::thread thread;
        std::atomic<bool> stop_requested;
        std::vector<std::function<void(int)>> disconnectCallbacks;
        static void run(Connection& connection);
    };
}
