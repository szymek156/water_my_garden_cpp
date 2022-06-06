#pragma once

#include "service_base.hpp"
#include "socket.hpp"

#include <freertos/semphr.h>
#include <i2cdev.h>

class Clock : public ServiceBase {
 public:
    Clock(SockPtr requestor, SockPtr web);
    void run_service();

 private:
    static void int_handler(void* arg);

    esp_err_t init_rtc();
    std::string get_status();
    void adjust_system_time();

    i2c_dev_t dev_;

    SockPtr watering_;
    SockPtr web_;

    SemaphoreHandle_t interrupt_arrived_;
};