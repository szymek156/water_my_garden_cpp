#pragma once

#include "service_base.hpp"
#include "socket.hpp"

#include <freertos/semphr.h>
#include <i2cdev.h>

class Clock : public ServiceBase {
 public:
    Clock(SockPtr requestor);
    void run_service();

 private:
    static void int_handler(void* arg);

    esp_err_t init_rtc();
    void print_status();


    i2c_dev_t dev_;

    SockPtr watering_;
    SemaphoreHandle_t interrupt_arrived_;
};