#pragma once

#include <i2cdev.h>
#include "service_base.hpp"

class Clock : public ServiceBase {
 public:
    Clock();
    void run_service();

 private:
    esp_err_t init_rtc();

    i2c_dev_t dev;


};