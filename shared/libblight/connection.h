#pragma once
#include "libblight_global.h"
#include "types.h"

#include <sys/socket.h>
#include <vector>
#include <thread>
#include <map>
#include <condition_variable>

namespace Blight {
    class LIBBLIGHT_EXPORT Connection {
    public:
        Connection(int fd);
        ~Connection();
        const message_t* read();
        int write(const message_t& message);
        bool send(MessageType type, data_t data, size_t size);
        void repaint(std::string identifier, int x, int y, int width, int height);
        void move(std::string identifier, int x, int y);

    private:
        int m_fd;
        std::thread thread;
        std::stop_source stop_source;
        std::mutex mutex;
        std::map<int, std::condition_variable*> acks;
        static void run(Connection& connection);
    };
}
