#include "clock_service.hpp"

#include "ds3231.h"

#include <esp_log.h>

static const char* TAG = "Clock";
static const gpio_num_t RTC_SDA = (gpio_num_t)21;
static const gpio_num_t RTC_SCL = (gpio_num_t)22;
static const gpio_num_t INT_PIN = (gpio_num_t)23;
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
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void int_handler(void *arg) {
    auto *that = (Clock *)arg;
    BaseType_t higher_prio_was_woken = false;
    // TODO: filter debounce
    // Set timer for like 20ms, in callback call this:

    // TaskNotifyGive and Take are better alternatives to binary semaphore
    vTaskNotifyGiveFromISR(that->getTask(), &higher_prio_was_woken);

    // TODO: Why it's needed at all?
    portYIELD_FROM_ISR(higher_prio_was_woken);
}

esp_err_t Clock::init_rtc() {
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(
        ds3231_init_desc(&dev, I2C_NUM_0, RTC_SDA, RTC_SCL), error, TAG, "Failed to init!");

    ESP_GOTO_ON_ERROR(
        gpio_install_isr_service(0), error, TAG, "Failed to install interrupt service!");

// TODO: get current task handle
    ESP_GOTO_ON_ERROR(
        gpio_isr_handler_add(INT_PIN, int_handler, this), error, TAG, "Failed to add ISR on INT_PIN!");

error:
    return ret;
}