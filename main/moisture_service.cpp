#include "moisture_service.hpp"

#include <driver/adc.h>
#include <driver/gpio.h>
#include <esp_adc_cal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <cmath>

#include <esp_log.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "Moisture";

// Prints status of sensors periodically
// #define TESTING 1

static void print_char_val_type(esp_adc_cal_value_t val_type) {
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGD(TAG, "Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGD(TAG, "Characterized using eFuse Vref\n");
    } else {
        ESP_LOGD(TAG, "Characterized using Default Vref\n");
    }
}

Moisture::Moisture(SockPtr requestor)
    : _adc_chars((esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t))),
      width_(ADC_WIDTH_BIT_12),
      atten_(ADC_ATTEN_DB_11),
      requestor_(std::move(requestor)) {
    xQueueAddToSet(requestor_->get_rx(), queues_);
}

void Moisture::run_service() {
    ESP_LOGI(TAG, "Service started");

    for (auto channel : CHANNELS) {
        ESP_ERROR_CHECK(adc1_config_channel_atten(channel, atten_));
    }

    // Characterize ADC
    esp_adc_cal_value_t val_type =
        esp_adc_cal_characterize(ADC_UNIT_1, atten_, width_, DEFAULT_VREF, _adc_chars);
    print_char_val_type(val_type);

    #if TESTING
        int interval = 1000;
    #else
        int interval = -1;
    #endif

    while (1) {
        QueueSetMemberHandle_t active_member = xQueueSelectFromSet(queues_, pdMS_TO_TICKS(interval));

        if (active_member == requestor_->get_rx()) {
            if (auto data = requestor_->rcv(0)) {
                auto msg = *data;
                switch (msg.type) {
                    case Message::Type::MoistureReq: {
                        ESP_LOGD(TAG, "Got moisture req for channel %d", msg.section);

                        ChannelReading reading = {};
                        if (msg.section < CHANNELS_SIZE) {
                            reading = read_channel(CHANNELS[msg.section]);
                        }

                        float moisture = calc_moisture(reading.raw);

                        ESP_LOGD(TAG,
                                "Channel %d raw: %d\tVoltage: %dmV moisture %f%%",
                                msg.section,
                                reading.raw,
                                reading.voltage,
                                moisture);

                        auto resp = Message{};
                        resp.type = Message::Type::MoistureRes;
                        resp.section_r = msg.section;
                        resp.moisture = moisture;

                        requestor_->send(resp);
                        break;
                    }

                    default:
                        break;
                }
            }
        } else {
            print_status();
        }
    }
}

Moisture::ChannelReading Moisture::read_channel(adc1_channel_t channel) {
    uint32_t adc_reading = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw(channel);
    }

    adc_reading /= NO_OF_SAMPLES;
    // Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, _adc_chars);

    return ChannelReading{.raw = adc_reading, .voltage = voltage};
}

float Moisture::calc_moisture(int adc_raw) {
    static const int AIR_DRY = 2590;
    static const int SOAKED_SOIL = 1200;

    if (adc_raw < SOAKED_SOIL * 0.8f || adc_raw > AIR_DRY * 1.2f) {
        return nanf("");
    }

    float denominator = (AIR_DRY - SOAKED_SOIL);
    float res = (adc_raw - SOAKED_SOIL) / denominator;

    // consider readings outside the range, but reasonable skewed as valid
    res = 1.0f - std::max(std::min(res, 1.0f), 0.0f);

    return res;
}


void Moisture::print_status() {
    for (auto channel : CHANNELS) {
        auto  reading = read_channel(channel);
        float moisture = calc_moisture(reading.raw);
        ESP_LOGD(TAG,
            "Channel %d raw: %d\tVoltage: %dmV moisture %f%%",
            channel,
            reading.raw,
            reading.voltage,
            moisture);
    }


}