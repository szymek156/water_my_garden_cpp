#include "clock_service.hpp"

#include "ds3231.h"

#include <ctime>

#include <esp_check.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

static const char* TAG = "Clock";
static const gpio_num_t RTC_SDA = (gpio_num_t)21;
static const gpio_num_t RTC_SCL = (gpio_num_t)22;
static const gpio_num_t INT_PIN = (gpio_num_t)23;

static std::string date_to_str(struct tm date_tm) {
    date_tm.tm_year -= 1900;

    std::string buf;
    buf.reserve(128);
    strftime(buf.data(), 128, "%F %T", &date_tm);

    return buf;
}

Clock::Clock(SockPtr watering, SockPtr web)
    : watering_(std::move(watering)),
      web_(std::move(web)),
      interrupt_arrived_(xSemaphoreCreateBinary()) {
    xQueueAddToSet(interrupt_arrived_, queues_);
    xQueueAddToSet(watering_->get_rx(), queues_);
    xQueueAddToSet(web_->get_rx(), queues_);
}

void Clock::run_service() {
    ESP_LOGI(TAG, "Starting service");

    esp_err_t ret = init_rtc();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Clock service is not operational! Err code %s", esp_err_to_name(ret));
    }

    ESP_LOGD(TAG, "%s", get_status().get());
    adjust_system_time();

    while (1) {
        QueueSetMemberHandle_t active_member =
            xQueueSelectFromSet(queues_, pdMS_TO_TICKS(10 * 60 * 1000));

        if (active_member == nullptr) {
            ESP_LOGD(TAG, "%s", get_status().get());

            adjust_system_time();
        }

        if (active_member == interrupt_arrived_) {
            xSemaphoreTake(interrupt_arrived_, 0);

            ESP_LOGI(TAG, "interrupt arrived %d", ret);

            ds3231_alarm_t alarms = DS3231_ALARM_NONE;
            ds3231_get_alarm_flags(&dev_, &alarms);
            ESP_LOGI(TAG, "alarm elapsed 0x%02x", alarms);

            // Alarm flag is set regardless of the state of interrupt
            // So additional check for alarm int status must be done
            uint8_t control = 0;
            ds3231_get_control(&dev_, &control);

            if ((alarms & 0x1) && (control & 0x1)) {
                auto msg = Message{};
                msg.type = Message::Type::Alarm1Expired;

                watering_->send(msg);
            }

            if ((alarms & 0x2) && (control & 0x2)) {
                auto msg = Message{};
                msg.type = Message::Type::Alarm2Expired;

                watering_->send(msg);
            }

            // Clearing alarm puts INT pin back to high
            ds3231_clear_alarm_flags(&dev_, alarms);
        }

        if (active_member == watering_->get_rx()) {
            if (auto data = watering_->rcv(0)) {
                auto msg = *data;

                switch (msg.type) {
                    case Message::Type::SetAlarm1: {
                        ESP_LOGI(TAG, "SetAlarm1! %s", date_to_str(msg.alarm_tm).c_str());

                        ds3231_enable_alarm_ints(&dev_, DS3231_ALARM_1);
                        ds3231_set_alarm(&dev_,
                                         DS3231_ALARM_1,
                                         &msg.alarm_tm,
                                         DS3231_ALARM1_MATCH_SECMINHOUR,
                                         nullptr,
                                         (ds3231_alarm2_rate_t)0);
                        break;
                    }

                    case Message::Type::SetAlarm2: {
                        ESP_LOGI(TAG, "SetAlarm2! %s", date_to_str(msg.alarm_tm).c_str());

                        // Enable interrupt, might be disabled in previous clear call
                        ds3231_enable_alarm_ints(&dev_, DS3231_ALARM_2);
                        ds3231_set_alarm(&dev_,
                                         DS3231_ALARM_2,
                                         nullptr,
                                         (ds3231_alarm1_rate_t)0,
                                         &msg.alarm_tm,
                                         DS3231_ALARM2_MATCH_MINHOUR);
                        break;
                    }
                    case Message::Type::ClearAlarm1: {
                        ESP_LOGI(TAG, "ClearAlarm1!");
                        ds3231_disable_alarm_ints(&dev_, DS3231_ALARM_1);
                        break;
                    }
                    case Message::Type::ClearAlarm2: {
                        ESP_LOGI(TAG, "ClearAlarm2!");
                        ds3231_disable_alarm_ints(&dev_, DS3231_ALARM_2);
                        break;
                    }
                    default:
                        ESP_LOGE(TAG, "Unexpected msg %d from watering service!", (int)msg.type);
                }
            }
        }

        if (active_member == web_->get_rx()) {
            if (auto data = web_->rcv(0)) {
                auto msg = *data;

                switch (msg.type) {
                    case Message::Type::Status: {
                        ESP_LOGI(TAG, "Status request from the web");
                        Message resp = {};
                        resp.type = Message::Type::Status;
                        resp.status = get_status().release();

                        web_->send(resp);
                    }
                    default:
                        break;
                }
            }
        }
    }
}

void Clock::int_handler(void* arg) {
    auto* that = (Clock*)arg;

    ESP_DRAM_LOGE(DRAM_STR("CLOCK ISR"), "interrupt! handle");
    BaseType_t higher_prio_was_woken = false;

    xSemaphoreGiveFromISR(that->interrupt_arrived_, &higher_prio_was_woken);

    // It is possible (although unlikely, and dependent on the semaphore type) that a semaphore
    // will have one or more tasks blocked on it waiting to give the semaphore. Calling
    // xSemaphoreTakeFromISR() will make a task that was blocked waiting to give the semaphore
    // leave the Blocked state. If calling the API function causes a task to leave the Blocked
    // state, and the unblocked task has a priority equal to or higher than the currently
    // executing task (the task that was interrupted), then, internally, the API function will
    // set *pxHigherPriorityTaskWoken to pdTRUE.

    // If xSemaphoreTakeFromISR() sets *pxHigherPriorityTaskWoken to pdTRUE, then a context
    // switch should be performed before the interrupt is exited. This will ensure that the
    // interrupt returns directly to the highest priority Ready state task
    portYIELD_FROM_ISR(higher_prio_was_woken);
}

esp_err_t Clock::init_rtc() {
    esp_err_t ret = ESP_OK;
    gpio_config_t io_conf = {};

    // INT pin on RTC is high by default, listen on falling edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_SEL_23;

    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    // Initialize i2c device
    ESP_GOTO_ON_ERROR(
        ds3231_init_desc(&dev_, I2C_NUM_0, RTC_SDA, RTC_SCL), err, TAG, "Failed to init!");

    ESP_GOTO_ON_ERROR(
        ds3231_clear_alarm_flags(&dev_, DS3231_ALARM_BOTH), err, TAG, "Failed to clear flags");

    // Switch sqw/int flag to alarm INT mode, but do not arm any alarms yet.
    ESP_GOTO_ON_ERROR(
        ds3231_enable_alarm_ints(&dev_, DS3231_ALARM_NONE), err, TAG, "Failed to enable interrupt");

    // TODO: There is a bug in call above, ALARM_NONE is 0, and setting 0 bits is ignored, so
    // disable ints manually
    ESP_GOTO_ON_ERROR(ds3231_disable_alarm_ints(&dev_, DS3231_ALARM_BOTH),
                      err,
                      TAG,
                      "Failed to disable alarm ints");

    // Setup GPIO pin to be ready to receive interrupts
    ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "Failed to setup gpio");

    ESP_GOTO_ON_ERROR(
        gpio_install_isr_service(0), err, TAG, "Failed to install interrupt service!");

    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(INT_PIN, int_handler, this),
                      err,
                      TAG,
                      "Failed to add ISR on INT_PIN!");

err:
    return ret;
}

std::unique_ptr<char[]> Clock::get_status() {
    float temp;
    struct tm rtcinfo;

    // C++ string formatting using streams is so ugly I cannot stand it,
    // on the other hand std::format is not there, so only option left
    // is to use old C API
    // const int RES_SIZE = 1024;
    // char res[RES_SIZE];

    if (ds3231_get_temp_float(&dev_, &temp) != ESP_OK) {
        char* err = nullptr;
        asprintf(&err, "%s", "Could not get temperature.");
        ESP_LOGE(TAG, "%s", err);

        // Argument template deduction failed, that's so pathetic!
        return std::unique_ptr<char[]>(err);
    }

    if (ds3231_get_time(&dev_, &rtcinfo) != ESP_OK) {
        char* err = nullptr;
        asprintf(&err, "%s", "Could not get time.");

        ESP_LOGE(TAG, "%s", err);
        return std::unique_ptr<char[]>(err);
    }

    //     7        6    5    4        3            2       1             0
    // osc status | NA | NA | NA | sqw status | busy | alarm 2 set | alarm 1 set
    uint8_t status = 0;
    ds3231_get_status(&dev_, &status);

    //  7        6           5             4           3            2                    1 0
    // osc en| sqw en | convert temp | sqw rate2 | sqw rate 1 | INT/SQW switch | alarm 2 int
    // enable | alarm 1 int enable
    uint8_t ctrl;
    ds3231_get_control(&dev_, &ctrl);

    // ESP_LOGD(TAG, "Status reg 0x%02X ctrl 0x%02X", status, ctrl);
    // ESP_LOGD(TAG,
    //          "Alarm1 expired: %s Alarm1 int enabled %s",
    //          status & 0x1 ? "true" : "false",
    //          ctrl & 0x1 ? "true" : "false");
    // ESP_LOGD(TAG,
    //          "Alarm2 expired: %s Alarm2 int enabled %s",
    //          status & 0x2 ? "true" : "false",
    //          ctrl & 0x2 ? "true" : "false");

    // ESP_LOGD(pcTaskGetName(0), "%s, %.2f deg Cel", date_to_str(rtcinfo).c_str(), temp);

    char* res = nullptr;
    asprintf(&res,
             "CLOCK:\n"
             "Status reg 0x%02X ctrl 0x%02X\n"
             "Alarm1 expired: %s Alarm1 int enabled %s\n"
             "Alarm2 expired: %s Alarm2 int enabled %s\n"
             "%s, %.2f deg Cel\n",
             status,
             ctrl,
             status & 0x1 ? "true" : "false",
             ctrl & 0x1 ? "true" : "false",
             status & 0x2 ? "true" : "false",
             ctrl & 0x2 ? "true" : "false",
             date_to_str(rtcinfo).c_str(),
             temp);

    return std::unique_ptr<char[]>(res);
}

// @brief Use RTC time and hammer system clock to it
void Clock::adjust_system_time() {
    // TODO: use 32kHz RTC pin to feed esp32 32K_XN pin
    // In the future apply NTP server sync

    struct tm rtcinfo = {};

    if (ds3231_get_time(&dev_, &rtcinfo) != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "Could not get time.");
        return;
    }

    rtcinfo.tm_year -= 1900;
    // UTC
    std::time_t secs_from_epoch = mktime(&rtcinfo);

    timeval t_val = {.tv_sec = secs_from_epoch, .tv_usec = 0};

    time_t now;

    time(&now);

    ESP_LOGI(TAG, "Time sync: diff %ld", now - secs_from_epoch);

    settimeofday(&t_val, nullptr);

    // Lets use CET
    // setenv("TZ", "CET2CEST0", 1);
    // tzset();

    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    timeinfo.tm_year += 1900;

    ESP_LOGI(TAG, "Time sync: %s", date_to_str(timeinfo).c_str());
}