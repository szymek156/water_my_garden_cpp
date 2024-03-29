#pragma once
#include "freertos/FreeRTOS.h"
#include "service_base.hpp"
#include "socket.hpp"

#include <cstdint>
#include <memory>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <freertos/queue.h>

struct MoistureMessage {
    float moisture;
    int channel;
};

class Moisture : public ServiceBase {
 public:
    Moisture(SockPtr requestor, SockPtr web);
    void run_service();

    // TODO: add fourth for terrace?
    static const int CHANNELS_SIZE = 3;
 private:
    static const int DEFAULT_VREF = 1100;  // Use adc2_vref_to_gpio() to obtain a better estimate
    static const int NO_OF_SAMPLES = 64;   // Multisampling

    static constexpr adc1_channel_t CHANNELS[CHANNELS_SIZE] = {
                                                               (adc1_channel_t)ADC_CHANNEL_7 /* TBD but currently occupies VEGS*/,
                                                               (adc1_channel_t)ADC_CHANNEL_6 /* FLOWERS*/,
                                                               (adc1_channel_t)ADC_CHANNEL_4};

    struct ChannelReading {
        uint32_t raw;
        uint32_t voltage;
    };

    ChannelReading read_channel(adc1_channel_t channel);
    float calc_moisture(int adc_raw);

    std::unique_ptr<char[]> get_status();
    esp_adc_cal_characteristics_t *_adc_chars;
    adc_bits_width_t width_;
    adc_atten_t atten_;

    SockPtr requestor_;
    SockPtr web_;
};