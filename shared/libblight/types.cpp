#include "types.h"
#include <unistd.h>

size_t Blight::buf_t::size(){
    return static_cast<size_t>(stride) * height;
}

int Blight::buf_t::close(){
    if(data != nullptr){
        munmap(data, size());
        data = nullptr;
    }
    if(fd != -1){
        int res = ::close(fd);
        fd = -1;
        return res;
    }
    return 0;
}

Blight::buf_t::~buf_t(){ close(); }

Blight::shared_buf_t Blight::buf_t::new_ptr(){
    return shared_buf_t(new buf_t{
        .fd = -1,
        .x = 0,
        .y = 0,
        .width = 0,
        .height = 0,
        .stride = 0,
        .format = Format::Format_Invalid,
        .data = nullptr,
        .uuid = ""
    });
}

Blight::header_t Blight::header_t::from_data(data_t data){
    header_t header;
    memcpy(&header, data, sizeof(header_t));
    return header;
}

Blight::header_t Blight::header_t::from_data(char* data){
    return from_data(reinterpret_cast<data_t>(data));
}

Blight::header_t Blight::header_t::from_data(void* data){
    return from_data(reinterpret_cast<data_t>(data));
}

Blight::header_t Blight::header_t::new_invalid(){
    return header_t{
        .type = MessageType::Invalid,
        .ackid = 0,
        .size = 0,
    };
}

Blight::message_t Blight::message_t::from_data(data_t _data){
    message_t message;
    message.header = header_t::from_data(_data);
    message.data = shared_data_t(new unsigned char[message.header.size]);
    memcpy(message.data.get(), &_data[sizeof(message.header)], message.header.size);
    return message;
}

Blight::message_t Blight::message_t::from_data(char* data){
    return from_data(reinterpret_cast<data_t>(data));
}

Blight::message_t Blight::message_t::from_data(void* data){
    return from_data(reinterpret_cast<data_t>(data));
}

Blight::data_t Blight::message_t::create_ack(message_t* message, size_t size){
    return create_ack(*message, size);
}

Blight::data_t Blight::message_t::create_ack(const message_t& message, size_t size){
    return reinterpret_cast<data_t>(new header_t{
        .type = MessageType::Ack,
        .ackid = message.header.ackid,
        .size = size
    });
}

Blight::message_ptr_t Blight::message_t::from_socket(int fd){
    auto message = message_t::new_ptr();
    ssize_t res = -1;
    while(res < 0){
        res = ::recv(fd, &message->header, sizeof(header_t), 0);
        if(res > 0){
            break;
        }
        if(res > 0){
            errno = EMSGSIZE;
        }
        if(errno == EAGAIN){
            return message;
        }
        if(errno == EINTR){
            timespec remaining;
            timespec requested{
                .tv_sec = 0,
                .tv_nsec = 5000
            };
            nanosleep(&requested, &remaining);
            continue;
        }
        std::cerr
            << "Failed to read connection message header: "
            << std::strerror(errno)
            << std::endl
            << "socket: "
            << std::to_string(fd)
            << std::endl;
        return message;
    }
    if(!message->header.size){
        return message;
    }
    // TODO - create data buffer
    res = -1;
    auto data = new unsigned char[message->header.size];
    // Handle the
    while(res < 0){
        auto res = ::recv(fd, data, message->header.size, 0);
        if((size_t)res == message->header.size){
            break;
        }
        if(res > 0){
            errno = EMSGSIZE;
        }
        if(errno != EAGAIN && errno != EINTR){
            std::cerr
                << "Failed to read connection message body: "
                << std::to_string(errno)
                << " "
                << std::strerror(errno)
                << std::endl
                << "socket="
                << std::to_string(fd)
                << ", ackid="
                << std::to_string(message->header.ackid)
                << ", type="
                << std::to_string(message->header.type)
                << ", size="
                << std::to_string(message->header.size)
                << std::endl;
            return message;
        }
        // TODO use poll
        timespec remaining;
        timespec requested{
            .tv_sec = 0,
            .tv_nsec = 5000
        };
        nanosleep(&requested, &remaining);
    }
    message->data = shared_data_t(data);
    return message;
}

Blight::message_ptr_t Blight::message_t::new_ptr(){
    return message_ptr_t(new message_t{
        .header = header_t::new_invalid()
    });
}

Blight::repaint_header_t Blight::repaint_header_t::from_data(data_t data){
    repaint_header_t header;
    memcpy(&header, data, sizeof(repaint_header_t));
    return header;
}

Blight::repaint_t Blight::repaint_t::from_message(const message_t* message){
    const data_t data = message->data.get();
    repaint_t repaint;
    repaint.header = repaint_header_t::from_data(data);
    repaint.identifier.assign(
        reinterpret_cast<char*>(&data[sizeof(repaint.header)]),
        repaint.header.identifier_len
    );
    return repaint;
}

Blight::move_header_t Blight::move_header_t::from_data(data_t data){
    move_header_t header;
    memcpy(&header, data, sizeof(move_header_t));
    return header;
}

Blight::move_t Blight::move_t::from_message(const message_t* message){
    const data_t data = message->data.get();
    move_t repaint;
    repaint.header = move_header_t::from_data(data);
    repaint.identifier.assign(
        reinterpret_cast<char*>(&data[sizeof(repaint.header)]),
        repaint.header.identifier_len
    );
    return repaint;
}

Blight::surface_info_t Blight::surface_info_t::from_data(data_t data){
    surface_info_t header;
    memcpy(&header, data, sizeof(surface_info_t));
    return header;
}
