#pragma once
#include "service_base.hpp"
#include <driver/gpio.h>
class Watering : public ServiceBase{
 public:
    Watering(SockPtr clock, SockPtr moisture);

    void run_service();

 private:
    static const gpio_num_t SECTION_ONE = (gpio_num_t)14;
    static const gpio_num_t SECTION_TWO = (gpio_num_t)27;
    static const gpio_num_t SECTION_THREE = (gpio_num_t)26;
    static const gpio_num_t SECTION_FOUR = (gpio_num_t)33;

    // TODO: add pulldown resistor?
    static const int TURN_ON = 0;
    static const int TURN_OFF = 1;

    void say_hello();

    void setup_gpio();

    SockPtr clock_;
    SockPtr moisture_;

};