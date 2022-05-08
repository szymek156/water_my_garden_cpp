#include "application.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <utility>

#include <esp_log.h>
static const char* TAG = "Application";

void StartApplication() {
    ESP_LOGI(TAG, "Hello from cpp world");

    // Testing cpp 17
    auto [a, b] = std::make_pair(1, "asdf");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}