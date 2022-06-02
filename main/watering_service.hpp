#pragma once
#include "service_base.hpp"

#include <driver/gpio.h>
#include <array>
#include <freertos/timers.h>

class Watering : public ServiceBase {
 public:
    Watering(SockPtr clock, SockPtr moisture);

    void run_service();

 private:
    static const gpio_num_t SECTION_ONE = (gpio_num_t)14;
    static const gpio_num_t SECTION_TWO = (gpio_num_t)27;
    static const gpio_num_t SECTION_THREE = (gpio_num_t)26;
    static const gpio_num_t SECTION_FOUR = (gpio_num_t)33;

   static const int SECTION_SIZE = 4;
   static constexpr std::array<gpio_num_t, SECTION_SIZE> sections_ = {SECTION_ONE, SECTION_TWO, SECTION_THREE, SECTION_FOUR};

    // TODO: add pulldown resistor?
    static const int TURN_ON = 0;
    static const int TURN_OFF = 1;

    void say_hello();

    void setup_gpio();

    void update_state(const Message& msg);

    void set_the_alarm(struct tm alarm_tm);
    void turn_off_valves();
    void switch_section(struct tm alarm_tm);
    void on_moisture_monitor_expire();
    void switch_to_next_section();

    static void moisture_monitor_cb(TimerHandle_t timer);

    void handle_watering(const Message& msg);

    int current_section_;
    bool watering_in_progress_;
    SockPtr clock_;
    SockPtr moisture_;
    TimerHandle_t moisture_monitor_;
};