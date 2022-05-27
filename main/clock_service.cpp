#include "clock_service.hpp"

#include "ds3231.h"

#include <algorithm>

#include <esp_check.h>
#include <esp_log.h>

static const char* TAG = "Clock";
static const gpio_num_t RTC_SDA = (gpio_num_t)21;
static const gpio_num_t RTC_SCL = (gpio_num_t)22;
static const gpio_num_t INT_PIN = (gpio_num_t)23;

Clock::Clock() : queues_(xQueueCreateSet(10)) {
}

// This implementation went way too far
struct Visitor {
    template <typename T>
    bool operator()(const std::unique_ptr<Socket<T>>& el) {
        return el->get_rx() == member_;
    }

    QueueSetMemberHandle_t member_;
};

void Clock::run_service() {
    ESP_LOGI(TAG, "Starting service");

    esp_err_t ret = init_rtc();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Clock service is not operational! Err code %s", esp_err_to_name(ret));

        // Go to infinite loop to avoid reset loop of the controller
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    while (1) {
        QueueSetMemberHandle_t active_member = xQueueSelectFromSet(queues_, pdMS_TO_TICKS(1000));

        auto found = std::find_if(connections_.cbegin(), connections_.cend(), [&](const auto& vt) {
            return std::visit(Visitor{.member_ = active_member}, vt.second);
        });

        switch ((*found).first) {
            case IndexOf<WateringMessage>(): {

                auto &sock = std::get<Socket<WateringMessage>::SockPtr>((*found).second);
                if (auto msg = sock->rcv(0)) {

                }
            }
        }
        // float temp;
        // struct tm rtcinfo;

        // if (ds3231_get_temp_float(&dev, &temp) != ESP_OK) {
        //     ESP_LOGE(pcTaskGetName(0), "Could not get temperature.");
        //     while (1) {
        //         vTaskDelay(1);
        //     }
        // }

        // if (ds3231_get_time(&dev, &rtcinfo) != ESP_OK) {
        //     ESP_LOGE(pcTaskGetName(0), "Could not get time.");
        //     while (1) {
        //         vTaskDelay(1);
        //     }
        // }

        // ESP_LOGI(pcTaskGetName(0),
        //          "%04d-%02d-%02d %02d:%02d:%02d, %.2f deg Cel",
        //          rtcinfo.tm_year,
        //          rtcinfo.tm_mon + 1,
        //          rtcinfo.tm_mday,
        //          rtcinfo.tm_hour,
        //          rtcinfo.tm_min,
        //          rtcinfo.tm_sec,
        //          temp);

        if (ulTaskNotifyTake(true, pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "interrupt arrived %d", ret);

            ds3231_alarm_t alarms = DS3231_ALARM_NONE;
            ds3231_get_alarm_flags(&dev, &alarms);
            ESP_LOGI(TAG, "alarm elapsed 0x%02x", alarms);

            // TODO: handle

            // Clearing alarm puts INT pin back to high
            ds3231_clear_alarm_flags(&dev, alarms);
        }
    }
}

static void int_handler(void* arg) {
    // auto *that = (Clock *)arg;
    auto task_handle = (TaskHandle_t)arg;

    ESP_DRAM_LOGE(DRAM_STR("CLOCK ISR"), "interrupt! handle %p", task_handle);
    BaseType_t higher_prio_was_woken = false;

    // TaskNotifyGive and Take are better alternatives to binary semaphore
    vTaskNotifyGiveFromISR(task_handle, &higher_prio_was_woken);

    // TODO: Why it's needed at all?
    portYIELD_FROM_ISR(higher_prio_was_woken);
}

esp_err_t Clock::init_rtc() {
    esp_err_t ret = ESP_OK;

    auto task_handle = xTaskGetCurrentTaskHandle();

    gpio_config_t io_conf = {};

    // INT pin on RTC is high by default, listen on falling edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_SEL_23;

    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    // Initialize i2c device
    ESP_GOTO_ON_ERROR(
        ds3231_init_desc(&dev, I2C_NUM_0, RTC_SDA, RTC_SCL), err, TAG, "Failed to init!");

    ESP_GOTO_ON_ERROR(
        ds3231_clear_alarm_flags(&dev, DS3231_ALARM_BOTH), err, TAG, "Failed to clear flags");

    struct tm alarm_time;
    alarm_time.tm_hour = 18;
    alarm_time.tm_min = 02;
    alarm_time.tm_sec = 40;

    ESP_GOTO_ON_ERROR(ds3231_set_alarm(&dev,
                                       DS3231_ALARM_BOTH,
                                       &alarm_time,
                                       DS3231_ALARM1_MATCH_SEC,
                                       &alarm_time,
                                       DS3231_ALARM2_MATCH_MIN),
                      err,
                      TAG,
                      "Failed to set alarm");

    // Enabling interrupts also disables SQW spam on the pin
    ESP_GOTO_ON_ERROR(
        ds3231_enable_alarm_ints(&dev, DS3231_ALARM_BOTH), err, TAG, "Failed to enable interrupt");

    // Setup GPIO pin to be ready to receive interrupts
    ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "Failed to setup gpio");

    ESP_GOTO_ON_ERROR(
        gpio_install_isr_service(0), err, TAG, "Failed to install interrupt service!");

    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(INT_PIN, int_handler, task_handle),
                      err,
                      TAG,
                      "Failed to add ISR on INT_PIN!");

err:
    return ret;
}