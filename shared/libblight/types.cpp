#include "types.h"

#include <unistd.h>
#include <random>
#include <sstream>

#include "debug.h"
#include "libblight.h"
#include "socket.h"

std::string generate_uuid_v4(){
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    };
    return ss.str();
}

Blight::size_t Blight::buf_t::size(){
    return static_cast<size_t>(stride) * static_cast<size_t>(height);
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

std::optional<Blight::shared_buf_t> Blight::buf_t::clone(){
    auto res = Blight::createBuffer(x, y, width, height, stride, format);
    if(!res.has_value()){
        return {};
    }
    auto buf = res.value();
    memcpy(buf->data, data, size());
    return buf;
}

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
        .uuid = new_uuid(),
        .surface = 0
    });
}

std::string Blight::buf_t::new_uuid(){ return generate_uuid_v4(); }

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
        {
            .type = MessageType::Invalid,
            .ackid = 0,
            .size = 0,
        }
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

Blight::header_t Blight::message_t::create_ack(message_t* message, size_t size){
    return create_ack(*message, size);
}

Blight::header_t Blight::message_t::create_ack(const message_t& message, size_t size){
    return header_t{
        {
            .type = MessageType::Ack,
            .ackid = message.header.ackid,
            .size = size
        }
    };
}

Blight::message_ptr_t Blight::message_t::from_socket(int fd){
    auto message = Blight::message_t::new_ptr();
    BlightProtocol::blight_message_t* m;
    int res = blight_message_from_socket(fd, &m);
    if(res < 0){
        return message;
    }
    memcpy(&message->header, &m->header, sizeof(BlightProtocol::blight_header_t));
    message->data = shared_data_t(m->data);
    delete m;
    return message;
}

Blight::message_ptr_t Blight::message_t::new_ptr(){
    return message_ptr_t(new message_t{
        .header = header_t::new_invalid(),
        .data = shared_data_t()
    });
}

Blight::repaint_t Blight::repaint_t::from_message(const message_t* message){
    const data_t data = message->data.get();
    repaint_t repaint;
    memcpy(&repaint, data, sizeof(repaint));
    return repaint;
}

Blight::move_t Blight::move_t::from_message(const message_t* message){
    move_t move;
    memcpy(&move, message->data.get(), sizeof(move_t));
    return move;
}

Blight::surface_info_t Blight::surface_info_t::from_data(data_t data){
    surface_info_t header;
    memcpy(&header, data, sizeof(surface_info_t));
    return header;
}

Blight::clipboard_t::clipboard_t(const std::string name, data_t data, size_t size)
: data{data},
  size{size},
  name{name}
{}

const std::string Blight::clipboard_t::to_string(){
    std::string res;
    if(data != nullptr && size){
        res.assign(reinterpret_cast<char*>(data.get()), size);
    }
    return res;
}

bool Blight::clipboard_t::update(){ return Blight::updateClipboard(*this); }

bool Blight::clipboard_t::set(shared_data_t data, size_t size){
    this->data = data;
    this->size = size;
    return Blight::setClipboard(*this);
}
