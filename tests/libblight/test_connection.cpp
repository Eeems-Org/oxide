#include "test_connection.h"

#include <libblight/connection.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <thread>

test_Connection::test_Connection() {}
test_Connection::~test_Connection() {}

void
test_Connection::test_surfaces_data_parsing() {
  int sockets[2];
  QVERIFY(::socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

  Blight::surface_id_t expected[] = {1, 1000, 65535};
  size_t data_size = sizeof(expected);

  std::thread thread([&]() {
    Blight::header_t req;
    auto n = ::recv(sockets[1], &req, sizeof(req), MSG_WAITALL);
    assert(n == (ssize_t)sizeof(req));
    assert(req.type == Blight::MessageType::List);

    Blight::header_t resp{
      {.type = Blight::MessageType::Ack,
       .ackid = req.ackid,
       .size = (uint32_t)data_size}
    };
    ::send(sockets[1], &resp, sizeof(resp), MSG_EOR);
    ::send(sockets[1], expected, data_size, MSG_EOR);
  });
  Blight::Connection conn(sockets[0]);
  ::close(sockets[0]);
  auto result = conn.surfaces();
  thread.join();

  QCOMPARE(result.size(), 3);
  QCOMPARE(result[0], 1);
  QCOMPARE(result[1], 1000);
  QCOMPARE(result[2], 65535);
  ::close(sockets[1]);
}

DECLARE_TEST(test_Connection)
