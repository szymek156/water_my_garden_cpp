#pragma once
#include "service_base.hpp"

#include <driver/gpio.h>
#include <array>
#include <memory>
#include <freertos/timers.h>

class Watering : public ServiceBase {
 public:
    Watering(SockPtr clock, SockPtr moisture, SockPtr web);

    void run_service();

 private:
    static const gpio_num_t SECTION_VEGS = (gpio_num_t)14;
    static const gpio_num_t SECTION_TERRACE = (gpio_num_t)27;
    static const gpio_num_t SECTION_FLOWERS = (gpio_num_t)26;
    static const gpio_num_t SECTION_GRASS = (gpio_num_t)33;

   static const int SECTION_SIZE = 4;
   static constexpr std::array<gpio_num_t, SECTION_SIZE> sections_ = {SECTION_VEGS, SECTION_TERRACE, SECTION_FLOWERS, SECTION_GRASS};
   static constexpr std::array<const char *, SECTION_SIZE> sections_names_ = {"Vegetables", "Terrace", "Flowers", "Grass"};
   static constexpr std::array<int, SECTION_SIZE> sections_time_ = {60 * 5, 61, 10*60, 20 * 60};

   static constexpr std::array<bool, SECTION_SIZE> sections_mask_ = {false, false, true, false};


    // TODO: add pulldown resistor?
    static const int TURN_ON = 0;
    static const int TURN_OFF = 1;

    void say_hello();

    void setup_gpio();

    void update_state(const Message& msg);

    void set_the_alarm(struct tm alarm_tm);
    void turn_off_valves();
    void set_section_alarm(struct tm alarm_tm);
    void on_moisture_monitor_expire();
    void switch_to_next_section();

    static void moisture_monitor_cb(TimerHandle_t timer);

    void handle_watering(const Message& msg);

    std::unique_ptr<char[]> get_status();

    void set_next_section();

    int current_section_;
    bool watering_in_progress_;
    SockPtr clock_;
    SockPtr moisture_;
    SockPtr web_;
    TimerHandle_t moisture_monitor_;
};