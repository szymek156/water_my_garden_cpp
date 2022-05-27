#include "watering_service.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "Watering";

Watering::Watering(SockPtr clock, SockPtr moisture)
    : clock_(std::move(clock)),
      moisture_(std::move(moisture)) {
    xQueueAddToSet(clock_->get_rx(), queues_);
    xQueueAddToSet(moisture_->get_rx(), queues_);
}

void Watering::run_service() {
    ESP_LOGI(TAG, "Service started");

    setup_gpio();
    say_hello();


    // auto msg = Message{};
    // msg.type = Message::Type::SetAlarm;
    // msg.alarm_tm.tm_hour = 20;
    // msg.alarm_tm.tm_min = 00;
    // msg.alarm_tm.tm_sec = 00;

    // clock_->send(msg);

    while (1) {
        QueueSetMemberHandle_t active_member = xQueueSelectFromSet(queues_, pdMS_TO_TICKS(1000));

        if (active_member == nullptr) {
            moisture_->send(Message{.type = Message::Type::MoistureReq, {.section = 0}});
        }

        if (active_member == moisture_->get_rx()) {
            if (auto data = moisture_->rcv(0)) {
                auto msg = *data;
                switch (msg.type) {
                    case Message::Type::MoistureRes:
                        ESP_LOGI(TAG,
                                 "Got moisture res for channel %d, moisture %f",
                                 msg.section_r,
                                 msg.moisture);
                        break;

                    default:
                        break;
                }
            }
        }

        if (active_member == clock_->get_rx()) {
        }

        // if (xQueueReceive(keypad_q_, &keypad_data, TIMEOUT) == pdPASS) {
        //     ESP_LOGV(TAG, "Got Keypad event");
        //     notifyKeypad(keypad_data);
        // }

        // if (auto res = sock_->rcv(-1)) {
        //     switch ((*res).type) {
        //         case Message::Type::StartWatering:
        //             ESP_LOGI(TAG, "Starting watering!");

        //             sock_->send(Message{.type = Message::Type::SetAlarm, {.alarm_tm = {}}});
        //             break;

        //         default:
        //             break;
        //     }
        // }
        vTaskDelay(pdMS_TO_TICKS(1000));
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