#include "watering_service.hpp"

#include <cmath>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "Watering";

Watering::Watering(SockPtr clock, SockPtr moisture)
    : current_state_(Idle),
      current_section_(0),
      clock_(std::move(clock)),
      moisture_(std::move(moisture)) {
    xQueueAddToSet(clock_->get_rx(), queues_);
    xQueueAddToSet(moisture_->get_rx(), queues_);
}

void Watering::run_service() {
    ESP_LOGI(TAG, "Service started");

    setup_gpio();
    say_hello();

    struct tm alarm_tm = {};

    alarm_tm.tm_hour = 20;
    alarm_tm.tm_min = 00;
    alarm_tm.tm_sec = 00;
    set_the_alarm(alarm_tm);

    while (1) {
        QueueSetMemberHandle_t active_member = xQueueSelectFromSet(queues_, pdMS_TO_TICKS(-1));

        if (active_member == moisture_->get_rx()) {
            if (auto data = moisture_->rcv(0)) {
                auto msg = *data;

                update_state(msg);
            }
        }

        if (active_member == clock_->get_rx()) {
            if (auto data = clock_->rcv(0)) {
                auto msg = *data;

                update_state(msg);
            }
        }
    }
}

// TODO: that could be a state machine if becomes too complex
void Watering::update_state(const Message& msg) {
    while (1) {
        switch (current_state_) {
            case Idle: {
                auto new_state = handle_idle(msg);
                if (new_state != current_state_) {
                    current_state_ = new_state;
                    continue;
                }
                return;
            }

            case WateringSection: {
                current_state_ = handle_watering(msg);
                return;
            }

            default:
                return;
        }
    }
}

Watering::CurrentState Watering::handle_idle(const Message& msg) {
    switch (msg.type) {
        case Message::Type::StartWatering:
            ESP_LOGI(TAG, "Starting watering procedure!");
            return WateringSection;

        default:
            ESP_LOGE(TAG, "Unexpected msg %d in idle state!", (int)msg.type);
            return current_state_;
    }
}

Watering::CurrentState Watering::handle_watering(const Message& msg) {
    switch (msg.type) {
        case Message::Type::StartWatering: {
            // Firstly check if section requires watering
            auto req = Message{};
            req.type = Message::Type::MoistureReq;
            req.section = current_section_;

            moisture_->send(req);

            // arm timer
            return current_state_;
        }
        case Message::Type::MoistureRes:
            ESP_LOGI(
                TAG, "Got moisture res for channel %u, moisture %f", msg.section_r, msg.moisture);

            // TODO: define threshold
            if (msg.moisture > 0.9 || msg.moisture == nanf("")) {
                // Feels wet enough, or there is no reading

                current_section_++;
            }

            // Reschedule next measurement
            return current_state_;

        default:
            ESP_LOGE(TAG, "Unexpected msg %d in watering state!", (int)msg.type);
            return current_state_;
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
    const int SECTION_SIZE = 4;
    gpio_num_t sections[SECTION_SIZE] = {SECTION_ONE, SECTION_TWO, SECTION_THREE, SECTION_FOUR};

    for (auto section : sections) {
        gpio_set_level(section, TURN_ON);

        vTaskDelay(pdMS_TO_TICKS(300));

        gpio_set_level(section, TURN_OFF);
    }
}

void Watering::set_the_alarm(const struct tm& alarm_tm) {
    auto msg = Message{};
    msg.type = Message::Type::SetAlarm;
    msg.alarm_tm = alarm_tm;

    clock_->send(msg);
}