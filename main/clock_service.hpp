#pragma once
#include <i2cdev.h>

class Clock {
 public:
    void run_service();

 private:
    esp_err_t init_rtc();

    i2c_dev_t dev;
};