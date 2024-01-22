#pragma once
#include "libblight_global.h"
#include "types.h"

#include <linux/input.h>
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
        std::atomic<bool> done;
        unsigned int data_size;
        shared_data_t data;
        std::condition_variable condition;
        ackid_t(
            Connection* connection,
            unsigned int ackid = 0,
            unsigned int data_size = 0,
            data_t data = nullptr
        );
        ~ackid_t();
        bool wait(int timeout = 0);
        void notify_all();
    } ackid_t;
    typedef std::shared_ptr<ackid_t> ackid_ptr_t;
    typedef std::optional<ackid_ptr_t> maybe_ackid_ptr_t;
    class LIBBLIGHT_EXPORT Connection {
    public:
        Connection(int fd);
        ~Connection();
        int handle();
        int input_handle();
        void onDisconnect(std::function<void(int)> callback);
        message_ptr_t read();
        std::optional<event_packet_t> read_event();
        maybe_ackid_ptr_t send(MessageType type, data_t data, size_t size);
        void waitForMarker(unsigned int marker);
        maybe_ackid_ptr_t repaint(
            std::string identifier,
            int x,
            int y,
            int width,
            int height,
            WaveformMode waveform = WaveformMode::HighQualityGrayscale,
            unsigned int marker = 0
        );
        inline maybe_ackid_ptr_t repaint(
            shared_buf_t buf,
            int x,
            int y,
            int width,
            int height,
            WaveformMode waveform = WaveformMode::HighQualityGrayscale,
            unsigned int marker = 0
        ){ return repaint(buf->surface, x, y, width, height, waveform, marker); }
        void move(shared_buf_t buf, int x, int y);
        std::optional<shared_buf_t> resize(
            shared_buf_t buf,
            int width,
            int height,
            int stride,
            data_t new_data
        );
        maybe_ackid_ptr_t move(std::string identifier, int x, int y);
        std::optional<shared_buf_t> getBuffer(std::string identifier);
        std::vector<shared_buf_t> buffers();
        maybe_ackid_ptr_t remove(shared_buf_t buf);
        std::vector<std::string> surfaces();
        void focused();

        std::mutex mutex;
        std::map<unsigned int, ackid_ptr_t> acks;

    private:
        std::map<unsigned int, unsigned int> markers;
        int m_fd;
        int m_inputFd;
        std::thread thread;
        std::atomic<bool> stop_requested;
        std::vector<std::function<void(int)>> disconnectCallbacks;
        static void run(Connection& connection);
    };
}
