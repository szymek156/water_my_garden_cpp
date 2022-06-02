#include "watering_service.hpp"

#include <cmath>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "Watering";

// If defined sets short intervals for each section, and arms timer 1 to fire immediately
#define TESTING 1

Watering::Watering(SockPtr clock, SockPtr moisture)
    : current_section_(0),
      watering_in_progress_(false),
      clock_(std::move(clock)),
      moisture_(std::move(moisture)),
      moisture_monitor_(xTimerCreate("moist_monit", pdMS_TO_TICKS(1000), false, this, moisture_monitor_cb)) {
    xQueueAddToSet(clock_->get_rx(), queues_);
    xQueueAddToSet(moisture_->get_rx(), queues_);
}


void Watering::run_service() {
    ESP_LOGI(TAG, "Service started");

    setup_gpio();
    say_hello();

    struct tm alarm_tm = {};

    #if TESTING
        time_t now;
        time(&now);

        // Start in next 5 seconds
        now += 5;

        localtime_r(&now, &alarm_tm);
    #else
        alarm_tm.tm_hour = 21;
        alarm_tm.tm_min = 55;
        alarm_tm.tm_sec = 00;
    #endif

        set_the_alarm(alarm_tm);

    while (1) {
        QueueSetMemberHandle_t active_member = xQueueSelectFromSet(queues_, pdMS_TO_TICKS(-1));

        std::optional<Message> data;

        if (active_member == moisture_->get_rx()) {
            data = moisture_->rcv(0);
        } else if (active_member == clock_->get_rx()) {
            data = clock_->rcv(0);
        }

        if (data) {
            auto msg = *data;
            handle_watering(msg);
        }
    }
}

void Watering::handle_watering(const Message& msg) {
    switch (msg.type) {
        case Message::Type::Alarm1Expired: {
            turn_off_valves();

            // Check if section requires watering
            auto req = Message{};
            req.type = Message::Type::MoistureReq;
            req.section = current_section_;

            moisture_->send(req);

            return;
        }
        case Message::Type::Alarm2Expired: {
            ESP_LOGI(TAG, "Time expired for section %d", current_section_);
            switch_to_next_section();
            return;
        }
        case Message::Type::MoistureRes:
            ESP_LOGI(
                TAG, "Got moisture res for channel %u, moisture %f", msg.section_r, msg.moisture);

            // TODO: define threshold
            if (msg.moisture >= 0.5) {
                // Feels wet enough
                ESP_LOGI(TAG, "Section %d is wet enough!", current_section_);

                switch_to_next_section();
            } else {
                // Keep watering
                if (!watering_in_progress_) {
                    ESP_LOGI(TAG, "Watering section %u", current_section_);
                    gpio_set_level(sections_[current_section_], TURN_ON);
                    // schedule this watering to last for that period of time
                    // it can finish earlier, if sensor says so
                    struct tm alarm_tm = {};

                    time_t now;
                    time(&now);

                    #if TESTING
                        // Alarm 2 has minute resolution, add residual to make sure, at least one minute passed
                        now += 60 + now % 60;
                    #else
                        now += 15 * 60;
                    #endif

                    localtime_r(&now, &alarm_tm);

                    switch_section(alarm_tm);
                    watering_in_progress_ = true;
                }

                if (!std::isnan(msg.moisture)) {
                    // There is measurement available, monitor it during the watering process
                    xTimerStart(moisture_monitor_, 0);
                }
            }

            return;

        default:
            ESP_LOGE(TAG, "Unexpected msg %d in watering state!", (int)msg.type);
            return;
    }
}

void Watering::setup_gpio() {
    // zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_SEL_14;
    // disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    // disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    // configure GPIO with the given settings
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(SECTION_ONE, TURN_OFF));

    io_conf.pin_bit_mask = GPIO_SEL_27;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(SECTION_TWO, TURN_OFF));

    io_conf.pin_bit_mask = GPIO_SEL_26;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(SECTION_THREE, TURN_OFF));

    io_conf.pin_bit_mask = GPIO_SEL_33;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(SECTION_FOUR, TURN_OFF));
}

void Watering::say_hello() {
    for (auto section : sections_) {
        gpio_set_level(section, TURN_ON);

        vTaskDelay(pdMS_TO_TICKS(300));

        gpio_set_level(section, TURN_OFF);
    }
}

void Watering::set_the_alarm(struct tm alarm_tm) {
    alarm_tm.tm_year += 1900;

    auto msg = Message{};
    msg.type = Message::Type::SetAlarm1;
    msg.alarm_tm = alarm_tm;

    clock_->send(msg);
}

void Watering::switch_section(struct tm alarm_tm) {
    alarm_tm.tm_year += 1900;

    auto msg = Message{};
    msg.type = Message::Type::SetAlarm2;
    msg.alarm_tm = alarm_tm;

    clock_->send(msg);
}


void Watering::turn_off_valves() {
    ESP_LOGI(TAG, "Disabling valves");
    for (auto section : sections_) {
        gpio_set_level(section, TURN_OFF);
    }
}

void Watering::on_moisture_monitor_expire() {
    // Check if section still requires watering
    auto req = Message{};
    req.type = Message::Type::MoistureReq;
    req.section = current_section_;

    moisture_->send(req);
}

void Watering::moisture_monitor_cb(TimerHandle_t timer) {
    ESP_LOGI("moisture_timer_cb", "Timer expired!");
    // Naming is unfortunate, we need to live with it
    auto service = (Watering *)pvTimerGetTimerID(timer);

    service->on_moisture_monitor_expire();
}

void Watering::switch_to_next_section() {
    xTimerStop(moisture_monitor_, 0);
    turn_off_valves();
    watering_in_progress_ = false;
    current_section_++;

    if (current_section_ < SECTION_SIZE) {
        ESP_LOGI(TAG, "Switch to next section %d", current_section_);

        // Check if next section requires watering
        auto req = Message{};
        req.type = Message::Type::MoistureReq;
        req.section = current_section_;

        moisture_->send(req);
    } else {
        ESP_LOGI(TAG, "Watering finished");

        current_section_ = 0;

        #if TESTING
            struct tm alarm_tm = {};
            time_t now;
            time(&now);

            // Start in next 5 seconds
            now += 5;

            localtime_r(&now, &alarm_tm);
            set_the_alarm(alarm_tm);
        #endif
    }

    auto clear_alarm = Message{};
    clear_alarm.type = Message::Type::ClearAlarm2;
    clock_->send(clear_alarm);
}