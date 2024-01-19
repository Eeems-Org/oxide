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
        ackid_t(Connection* connection, unsigned int ackid);
        ackid_t(Connection* connection);
        ~ackid_t();
        bool wait(int timeout = 0);
    } ackid_t;
    class LIBBLIGHT_EXPORT Connection {
    public:
        Connection(int fd);
        ~Connection();
        void onDisconnect(std::function<void(int)> callback);
        message_t* read();
        int write(const message_t& message);
        ackid_t send(MessageType type, data_t data, size_t size);
        void waitFor(ackid_t ackid, int timeout = 0);
        void waitForMarker(unsigned int marker);
        ackid_t repaint(
            std::string identifier,
            int x,
            int y,
            int width,
            int height,
            unsigned int marker = 0
        );
        ackid_t move(std::string identifier, int x, int y);

        std::mutex mutex;
        std::map<unsigned int, std::condition_variable*> acks;
    private:
        std::map<unsigned int, unsigned int> markers;
        int m_fd;
        std::thread thread;
        std::atomic<bool> stop_requested;
        std::vector<std::function<void(int)>> disconnectCallbacks;
        static void run(Connection& connection);
    };
}
