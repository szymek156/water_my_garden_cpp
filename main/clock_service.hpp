#pragma once
#include "socket.hpp"

#include <memory>

#include <i2cdev.h>
class Clock {
 public:
    Clock(Socket<Message>::SockPtr sock);
    void run_service();

 private:
    esp_err_t init_rtc();

    i2c_dev_t dev;

    Socket<Message>::SockPtr sock_;
};