#pragma once
#include "autotest.h"

class test_Types : public QObject{
    Q_OBJECT

public:
   test_Types();
    ~test_Types();

private slots:
    void test_clipboard_t();
    void test_buf_t();
    void test_header_t();
    void test_message_t();
    void test_repaint_t();
    void test_move_t();
    void test_surface_info_t();
};
