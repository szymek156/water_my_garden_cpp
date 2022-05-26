#include "application.h"

#include "ds3231.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "moisture_service.hpp"
#include "watering_service.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>
static const char* TAG = "Application";

static const gpio_num_t RTC_SDA = (gpio_num_t)21;
static const gpio_num_t RTC_SCL = (gpio_num_t)22;

void get_clock(void* pvParameters) {
    // Initialize RTC
    i2c_dev_t dev;
    if (ds3231_init_desc(&dev, I2C_NUM_0, RTC_SDA, RTC_SCL) != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not init device descriptor.");
        while (1) {
            vTaskDelay(1);
        }
    }

    ds3231_clear_alarm_flags(&dev, DS3231_ALARM_BOTH);
    struct tm alarm_time;
    // 21:53
    alarm_time.tm_hour = 22;
    alarm_time.tm_min = 02;
    alarm_time.tm_sec = 40;

    ds3231_set_alarm(&dev,
                     DS3231_ALARM_BOTH,
                     &alarm_time,
                     DS3231_ALARM1_MATCH_SECMINHOUR,
                     &alarm_time,
                     DS3231_ALARM2_MATCH_MINHOUR);

    // Initialise the xLastWakeTime variable with the current time.
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // Get RTC date and time
    while (1) {
        float temp;
        struct tm rtcinfo;

        if (ds3231_get_temp_float(&dev, &temp) != ESP_OK) {
            ESP_LOGE(pcTaskGetName(0), "Could not get temperature.");
            while (1) {
                vTaskDelay(1);
            }
        }

        if (ds3231_get_time(&dev, &rtcinfo) != ESP_OK) {
            ESP_LOGE(pcTaskGetName(0), "Could not get time.");
            while (1) {
                vTaskDelay(1);
            }
        }

        ESP_LOGI(pcTaskGetName(0),
                 "%04d-%02d-%02d %02d:%02d:%02d, %.2f deg Cel",
                 rtcinfo.tm_year,
                 rtcinfo.tm_mon + 1,
                 rtcinfo.tm_mday,
                 rtcinfo.tm_hour,
                 rtcinfo.tm_min,
                 rtcinfo.tm_sec,
                 temp);

        ds3231_alarm_t alarms = DS3231_ALARM_NONE;
        ds3231_get_alarm_flags(&dev, &alarms);
        ESP_LOGI(TAG, "alarm elapsed 0x%02x", alarms);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}

template <typename T>
void service(void* param) {
    T* service = (T*)param;

    service->run_service();
}

void StartApplication() {
    ESP_LOGI(TAG, "#### WATER MY GARDEN ####");

    xTaskCreate(get_clock, "get_clock", 1024 * 4, NULL, 2, NULL);

    Moisture moisture;
    xTaskCreate(service<Moisture>, "moisture", 1024 * 4, &moisture, 2, NULL);

    Watering watering;
    xTaskCreate(service<Watering>, "watering", 1024 * 4, &watering, 2, NULL);

    for (int i = 0;; i++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}