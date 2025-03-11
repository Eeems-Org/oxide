#include "test_types.h"

#include <libblight/types.h>

test_Types::test_Types(){ }
test_Types::~test_Types(){ }

void test_Types::test_clipboard_t(){
    // TODO - test to_string()
    // TODO - test update()
    // TODO - test set()
}

void test_Types::test_buf_t(){
    // TODO - test use of buffer with QImage
    // TODO - test size()
    // TODO - test close()
    // TODO - test clone()
    // TODO - test new_ptr()
    // TODO - test new_uuid()
}

void test_Types::test_header_t(){
    // TODO - test from_data()
    // TODO - test new_invalid()
}

void test_Types::test_message_t(){
    auto message = Blight::message_t::new_ptr();
    QVERIFY(message != nullptr);
    QCOMPARE(sizeof(*message.get()), sizeof(Blight::message_t));
    auto header = Blight::message_t::create_ack(message.get(), 0);
    QCOMPARE(header.size, 0);
    QCOMPARE(Blight::MessageType::Ack, header.type);
    QCOMPARE(header.ackid, message->header.ackid);
    std::string data("test");
    header = Blight::message_t::create_ack(message.get(), data.size());
    QCOMPARE(header.size, data.size());
    QCOMPARE(header.type, Blight::MessageType::Ack);
    QCOMPARE(header.ackid, message->header.ackid);
    // TODO - test from_socket()
    // TODO - test from_data()
}

void test_Types::test_repaint_t(){
    auto header = new Blight::repaint_t{
        {
            .x = 10,
            .y = 10,
            .width = 10,
            .height = 10,
            .waveform = Blight::WaveformMode::Mono,
            .marker = 1,
            .identifier = 1,
        }
    };
    Blight::shared_data_t data(reinterpret_cast<Blight::data_t>(header));
    Blight::message_t message{
        .header = {
            {
              .type = Blight::MessageType::Repaint,
              .ackid = 10,
              .size = sizeof(Blight::repaint_t)
            }
        },
        .data = data
    };
    auto header2 = Blight::repaint_t::from_message(&message);
    QCOMPARE(sizeof(header2), message.header.size);
    QCOMPARE(header2.x, header->x);
    QCOMPARE(header2.y, header->y);
    QCOMPARE(header2.width, header->width);
    QCOMPARE(header2.height, header->height);
    QCOMPARE(header2.waveform, header->waveform);
    QCOMPARE(header2.marker, header->marker);
    QCOMPARE(header2.identifier, header->identifier);
}

void test_Types::test_move_t(){
    auto header = new Blight::move_t{
      {
        .identifier = 1,
        .x = 10,
        .y = 10
      }
    };
    Blight::shared_data_t data(reinterpret_cast<Blight::data_t>(header));
    Blight::message_t message{
        .header = {
            {
              .type = Blight::MessageType::Move,
              .ackid = 10,
              .size = sizeof(Blight::move_t)
            }
        },
        .data = data
    };
    auto header2 = Blight::move_t::from_message(&message);
    QCOMPARE(sizeof(header2), message.header.size);
    QCOMPARE(header2.identifier, header->identifier);
    QCOMPARE(header2.x, header->x);
    QCOMPARE(header2.y, header->y);
}

void test_Types::test_surface_info_t(){
    Blight::surface_info_t header{
       {
         .x = 10,
         .y = 10,
         .width = 10,
         .height = 10,
         .stride = 20,
         .format = Blight::Format::Format_RGB16
       }
    };
    auto header2 = Blight::surface_info_t::from_data(
        reinterpret_cast<Blight::data_t>(&header)
    );
    QCOMPARE(sizeof(header2), sizeof(Blight::surface_info_t));
    QCOMPARE(header2.x, header.x);
    QCOMPARE(header2.y, header.y);
    QCOMPARE(header2.width, header.width);
    QCOMPARE(header2.height, header.height);
    QCOMPARE(header2.stride, header.stride);
    QCOMPARE(header2.format, header.format);
}

DECLARE_TEST(test_Types)
