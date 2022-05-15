#include "application.h"

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

static const gpio_num_t SECTION_ONE = (gpio_num_t)21;
static const gpio_num_t SECTION_TWO = (gpio_num_t)19;
static const gpio_num_t SECTION_THREE = (gpio_num_t)18;
static const gpio_num_t SECTION_FOUR = (gpio_num_t)5;

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
    io_conf.pin_bit_mask = GPIO_SEL_21;
    // disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    // disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    // configure GPIO with the given settings
    gpio_config(&io_conf);
    gpio_set_level(SECTION_ONE, TURN_OFF);

    io_conf.pin_bit_mask = GPIO_SEL_19;
    gpio_config(&io_conf);
    gpio_set_level(SECTION_TWO, TURN_OFF);

    io_conf.pin_bit_mask = GPIO_SEL_18;
    gpio_config(&io_conf);
    gpio_set_level(SECTION_THREE, TURN_OFF);

    io_conf.pin_bit_mask = GPIO_SEL_5;
    gpio_config(&io_conf);
    gpio_set_level(SECTION_FOUR, TURN_OFF);
}

static const adc2_channel_t MOISTURE_CHANNEL = ADC2_CHANNEL_3;
static const int AIR_DRY = 2590;
static const int SOAKED_SOIL = 1200;
// 1457
void setup_adc() {
    // ADC2 config
    ESP_ERROR_CHECK(adc2_config_channel_atten(MOISTURE_CHANNEL, ADC_ATTEN_DB_11));
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

void read_moisture() {
    esp_err_t ret = ESP_OK;
    static int adc_raw;

    do {
        ret = adc2_get_raw(MOISTURE_CHANNEL, (adc_bits_width_t)ADC_WIDTH_BIT_DEFAULT, &adc_raw);
    } while (ret == ESP_ERR_INVALID_STATE);
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "raw  data: %d, moisture %f%%", adc_raw, calc_moisture(adc_raw));
}

void StartApplication() {
    ESP_LOGI(TAG, "Hello from cpp world");

    setup_gpio();

    setup_adc();

    read_moisture();

    gpio_num_t arr[4] = {SECTION_ONE, SECTION_TWO, SECTION_THREE, SECTION_FOUR};

    for (int i = 0;; i++) {
        // gpio_set_level(arr[i % 4], TURN_ON);

        read_moisture();
        vTaskDelay(pdMS_TO_TICKS(2000));

        // gpio_set_level(arr[i % 4], TURN_OFF);
    }
}