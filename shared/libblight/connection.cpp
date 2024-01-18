#include "connection.h"

namespace Blight{
    Block::Block()
    : m_iovec{nullptr, 0},
      pointer(0)
    {}

    Block::Block(iovec iovec)
    : m_iovec(iovec),
      pointer(0)
    {

    }

    void Block::seek(unsigned int i){ pointer = i > m_iovec.iov_len ? m_iovec.iov_len : i; }

    unsigned int Block::operator[](unsigned int i){
        if(i >= m_iovec.iov_len){
            return 0;
        }
        return data()[i];
    }

    void Block::setData(data_t data, size_t size){
        m_iovec.iov_base = data;
        m_iovec.iov_len = size;
    }

    std::string Block::to_string(){
        std::string str;
        str.assign(reinterpret_cast<char*>(m_iovec.iov_base), m_iovec.iov_len);
        return str;
    }

    data_t Block::data(){ return reinterpret_cast<data_t>(m_iovec.iov_base); }

    Connection::Connection(int fd)
    : m_fd(dup(fd))
    {}

    Connection::~Connection(){
        ::close(m_fd);
    }

    Message Connection::read(){
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        int size = recvmsg(m_fd, &msg, MSG_PEEK | MSG_TRUNC);
        if(size < 0){
            std::cerr
                << "Failed to recieve connection message: "
                << std::strerror(errno)
                << std::endl;
            return Message();
        }
        return Message(msg);
    }

    void Connection::repaint(std::string identifier, int x, int y, int width, int height){
        char id[identifier.size()];
        memcpy(&id, identifier.data(), identifier.size());
        repaint_t data{
            .x = x,
            .y = y,
            .width = width,
            .height = height
        };
        Message message(MessageType::Repaint, std::vector<iovec>{
          {iovec{
              .iov_base = identifier.data(),
              .iov_len = identifier.size()
          }},
          {iovec{
              .iov_base = &data,
              .iov_len = sizeof(data)
          }},
        });
        if(sendmsg(m_fd, message.data(), 0) < 0){
            std::cerr
                << "Failed to send connection message: "
                << std::strerror(errno)
                << std::endl;
        }
    }

    int Connection::peek(msghdr& msg){
        memset(&msg, 0, sizeof(msg));
        return recvmsg(m_fd, &msg, MSG_PEEK | MSG_TRUNC);
    }

    Message::Message()
    : Message(msghdr{
          .msg_name = nullptr,
          .msg_namelen = 0,
          .msg_iov = nullptr,
          .msg_iovlen = 0,
          .msg_control = nullptr,
          .msg_controllen = 0,
          .msg_flags = 0
      })
    {}

    Message::Message(msghdr msg) : msg(msg){
        m_type = static_cast<MessageType>(block(0)[0]);
    }

    Message::Message(MessageType type, std::vector<iovec> data)
    : m_type(type)
    {
        data.insert(data.begin() ,iovec{
            .iov_base = &type,
            .iov_len = 1
        });
        msg = msghdr{
            .msg_name = nullptr,
            .msg_namelen = 0,
            .msg_iov = data.data(),
            .msg_iovlen = data.size(),
            .msg_control = nullptr,
            .msg_controllen = 0,
            .msg_flags = 0
        };
    }

    Block Message::block(unsigned int i){
        if(i >= msg.msg_iovlen){
            return Block();
        }
        return Block(msg.msg_iov[i]);
    }

    msghdr* Message::data(){ return &msg; }

    MessageType Message::type(){ return m_type; }
}
