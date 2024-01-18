#include "types.h"
#include <unistd.h>

size_t Blight::buf_t::size(){ return width * stride * height; }

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

Blight::message_t Blight::message_t::from_data(data_t _data){
    return message_t{
        .header = header_t::from_data(_data),
        .data = &_data[sizeof(header)]
    };
}

Blight::message_t Blight::message_t::from_data(char* data){
    return from_data(reinterpret_cast<data_t>(data));
}

Blight::message_t Blight::message_t::from_data(void* data){
    return from_data(reinterpret_cast<data_t>(data));
}

Blight::data_t Blight::message_t::create_ack(message_t* message){ return create_ack(*message); }

Blight::data_t Blight::message_t::create_ack(const message_t& message){
    return reinterpret_cast<data_t>(new header_t{
        .type = MessageType::Ack,
        .ackid = message.header.ackid,
        .size = 0
    });
}

Blight::repaint_header_t Blight::repaint_header_t::from_data(data_t data){
    repaint_header_t header;
    memcpy(&header, data, sizeof(repaint_header_t));
    return header;
}

Blight::repaint_t Blight::repaint_t::from_message(const message_t* message){
    repaint_t repaint;
    repaint.header = repaint_header_t::from_data(message->data);
    repaint.identifier.assign(
        reinterpret_cast<char*>(&message->data[sizeof(repaint.header)]),
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
    move_t repaint;
    repaint.header = move_header_t::from_data(message->data);
    repaint.identifier.assign(
        reinterpret_cast<char*>(&message->data[sizeof(repaint.header)]),
        repaint.header.identifier_len
    );
    return repaint;
}
