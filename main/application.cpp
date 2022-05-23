// RTC handling taken from https://github.com/nopnop2002/esp-idf-ds3231
// Repo claims code was forked from:
// https://github.com/UncleRus/esp-idf-lib/tree/master/components/ds3231

#include "application.h"

#include "ds3231.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <driver/adc.h>
#include <driver/gpio.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>
static const char* TAG = "Application";

static const gpio_num_t SECTION_ONE = (gpio_num_t)14;
static const gpio_num_t SECTION_TWO = (gpio_num_t)27;
static const gpio_num_t SECTION_THREE = (gpio_num_t)26;
static const gpio_num_t SECTION_FOUR = (gpio_num_t)33;

// TODO: add pulldown resistor?
static const int TURN_ON = 0;
static const int TURN_OFF = 1;

void setup_gpio() {
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
    gpio_config(&io_conf);
    gpio_set_level(SECTION_ONE, TURN_OFF);

    io_conf.pin_bit_mask = GPIO_SEL_27;
    gpio_config(&io_conf);
    gpio_set_level(SECTION_TWO, TURN_OFF);

    io_conf.pin_bit_mask = GPIO_SEL_26;
    gpio_config(&io_conf);
    gpio_set_level(SECTION_THREE, TURN_OFF);

    io_conf.pin_bit_mask = GPIO_SEL_33;
    gpio_config(&io_conf);
    gpio_set_level(SECTION_FOUR, TURN_OFF);
}

static const adc1_channel_t MOISTURE_CHANNEL = ADC_CHANNEL_4; //GPIO32
static const int AIR_DRY = 2590;
static const int SOAKED_SOIL = 1200;

void setup_adc() {
    // ADC2 config
    ESP_ERROR_CHECK(adc1_config_channel_atten(MOISTURE_CHANNEL, ADC_ATTEN_DB_11));
}

float calc_moisture(int adc_raw) {
    if (adc_raw < SOAKED_SOIL * 0.8f || adc_raw > AIR_DRY * 1.2f) {
        return nanf("");
    }

    float denominator = (AIR_DRY - SOAKED_SOIL);
    float res = (adc_raw - SOAKED_SOIL) / denominator;

    // consider readings outside the range, but reasonable skewed as valid
    res = 1.0f - std::max(std::min(res, 1.0f), 0.0f);

    return res;
}

void read_moisture(void* pvParameters) {
    while (1) {
        esp_err_t ret = ESP_OK;
        static int adc_raw;

        do {
            ret = adc1_get_raw(MOISTURE_CHANNEL, (adc_bits_width_t)ADC_WIDTH_BIT_DEFAULT, &adc_raw);
        } while (ret == ESP_ERR_INVALID_STATE);
        ESP_ERROR_CHECK(ret);

        ESP_LOGI(TAG, "raw  data: %d, moisture %f%%", adc_raw, calc_moisture(adc_raw));

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

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
        vTaskDelayUntil(&xLastWakeTime, 1000);
    }
}
void StartApplication() {
    ESP_LOGI(TAG, "Hello from cpp world");

    setup_gpio();

    setup_adc();

    xTaskCreate(get_clock, "get_clock", 1024 * 4, NULL, 2, NULL);

    xTaskCreate(read_moisture, "read_moisture", 1024 * 4, NULL, 2, NULL);

    // read_moisture();

    gpio_num_t arr[4] = {SECTION_ONE, SECTION_TWO, SECTION_THREE, SECTION_FOUR};

    for (int i = 0;; i++) {
        gpio_set_level(arr[i % 4], TURN_ON);

        vTaskDelay(pdMS_TO_TICKS(2000));

        gpio_set_level(arr[i % 4], TURN_OFF);
    }
}