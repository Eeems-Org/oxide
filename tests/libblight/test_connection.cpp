#include "test_connection.h"

#include <libblight/types.h>

test_Connection::test_Connection() {}
test_Connection::~test_Connection() {}

void
test_Connection::test_surfaces_data_parsing() {
  // Reproduce the parsing logic from Connection::surfaces().
  // Create raw buffer with surface_id_t values > 255 to catch the old
  // unsigned-char-pointer-arithmetic bug (reading single bytes instead of pairs).
  Blight::surface_id_t input[] = {1, 1000, 65535};
  size_t data_size = sizeof(input);

  auto* raw = new unsigned char[data_size];
  memcpy(raw, input, data_size);
  Blight::shared_data_t data(raw);

  // This is exactly what surfaces() does after ack->wait():
  auto count = data_size / sizeof(Blight::surface_id_t);
  auto* buf = reinterpret_cast<Blight::surface_id_t*>(data.get());
  std::vector<Blight::surface_id_t> result(buf, buf + count);

  QCOMPARE(result.size(), 3);
  QCOMPARE(result[0], 1);
  QCOMPARE(result[1], 1000);
  QCOMPARE(result[2], 65535);
}

DECLARE_TEST(test_Connection)
