#include "application.h"

#include "clock_service.hpp"
#include "ds3231.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "moisture_service.hpp"
#include "socket.hpp"
#include "watering_service.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>
static const char* TAG = "Application";

template <typename T>
void service(void* param) {
    T* service = (T*)param;

    service->run_service();
}

void StartApplication() {
    ESP_LOGI(TAG, "#### WATER MY GARDEN ####");

    auto watering_sock = Socket<Message>::SockPtr(new Socket<Message>(10));

    Clock clock{watering_sock->connect()};
    xTaskCreate(service<Clock>, "clock", 1024 * 4, &clock, 2, NULL);

    Moisture moisture;
    xTaskCreate(service<Moisture>, "moisture", 1024 * 4, &moisture, 2, NULL);

    Watering watering{std::move(watering_sock)};
    xTaskCreate(service<Watering>, "watering", 1024 * 4, &watering, 2, NULL);

    for (int i = 0;; i++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}