#pragma once
#include "libblight_global.h"
#include "types.h"

#include <sys/socket.h>
#include <vector>

namespace Blight {
    enum MessageType{
        Invalid,
        Repaint
    };
    class LIBBLIGHT_EXPORT Block{
    public:
        Block();
        Block(iovec iovec);
        void seek(unsigned int i);
        unsigned int operator[](unsigned int i);
        void setData(data_t data, size_t size);
        std::string to_string();
        data_t data();
    private:
        iovec m_iovec;
        unsigned int pointer;
    };
    class LIBBLIGHT_EXPORT Message{
    public:
        Message();
        Message(msghdr msg);
        Message(MessageType type, std::vector<iovec> data);
        Block block(unsigned int i);
        msghdr* data();
        MessageType type();
    private:
        msghdr msg;
        MessageType m_type;
    };
    class LIBBLIGHT_EXPORT Connection {
    public:
        Connection(int fd);
        ~Connection();
        Message read();
        void repaint(std::string identifier, int x, int y, int width, int height);

    private:
        int m_fd;

        int peek(msghdr& msg);

        // int send_fd(int fd){
        //     msghdr msg;
        //     memset(&msg, 0, sizeof(msg));
        //     char cmsgbuf[CMSG_SPACE(sizeof(fd))];
        //     msg.msg_control = cmsgbuf;
        //     msg.msg_controllen = sizeof(cmsgbuf);
        //     cmsghdr* cmsg;
        //     cmsg = CMSG_FIRSTHDR(&msg);
        //     cmsg->cmsg_level = SOL_SOCKET;
        //     cmsg->cmsg_type = SCM_RIGHTS;
        //     cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
        //     memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
        //     msg.msg_controllen = cmsg->cmsg_len;
        //     return ::sendmsg(m_fd, &msg, 0);
        // }
        // int recieve_fd(){
        //     msghdr msg;
        //     memset(&msg, 0, sizeof(msg));
        //     int fd;
        //     char cmsgbuf[CMSG_SPACE(sizeof(fd))];
        //     msg.msg_control = cmsgbuf;
        //     msg.msg_controllen = sizeof(cmsgbuf);
        //     int res = ::recvmsg(m_fd, &msg, 0);
        //     cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        //     if(res < 0){
        //         return res;
        //     }
        //     if(cmsg == NULL){
        //         errno = EINVAL;
        //         return -1;
        //     }
        //     memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
        //     return fd;
        // }
    };
}
