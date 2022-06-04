#include "application.h"

#include "clock_service.hpp"
#include "ds3231.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "moisture_service.hpp"
#include "socket.hpp"
#include "watering_service.hpp"
#include "wifi_service.hpp"

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
    ESP_LOGI(TAG, R"foo(
.--.      .--.    ____     ,---------.      .-''-.   .-------.
|  |_     |  |  .'  __ `.  \          \   .'_ _   \  |  _ _   \
| _( )_   |  | /   '  \  \  `--.  ,---'  / ( ` )   ' | ( ' )  |
|(_ o _)  |  | |___|  /  |     |   \    . (_ o _)  | |(_ o _) /
| (_,_) \ |  |    _.-`   |     :_ _:    |  (_,_)___| | (_,_).' __
|  |/    \|  | .'   _    |     (_I_)    '  \   .---. |  |\ \  |  |
|  '  /\  `  | |  _( )_  |    (_(=)_)    \  `-'    / |  | \ `'   /
|    /  \    | \ (_ o _) /     (_I_)      \       /  |  |  \    /
`---'    `---`  '.(_,_).'      '---'       `'-..-'   ''-'   `'-'

                ,---.    ,---.    ____     __
                |    \  /    |    \   \   /  /
                |  ,  \/  ,  |     \  _. /  '
                |  |\_   /|  |      _( )_ .'
                |  _( )_/ |  |  ___(_ o _)'
                | (_ o _) |  | |   |(_,_)'
                |  (_,_)  |  | |   `-'  /
                |  |      |  |  \      /
                '--'      '--'   `-..-'

                          .-_'''-.       ____     .-------.      ______          .-''-.   ,---.   .--.
                         '_( )_   \    .'  __ `.  |  _ _   \    |    _ `''.    .'_ _   \  |    \  |  |
                        |(_ o _)|  '  /   '  \  \ | ( ' )  |    | _ | ) _  \  / ( ` )   ' |  ,  \ |  |
                        . (_,_)/___|  |___|  /  | |(_ o _) /    |( ''_'  ) | . (_ o _)  | |  |\_ \|  |
                        |  |  .-----.    _.-`   | | (_,_).' __  | . (_) `. | |  (_,_)___| |  _( )_\  |
                        '  \  '-   .' .'   _    | |  |\ \  |  | |(_    ._) ' '  \   .---. | (_ o _)  |
                         \  `-'`   |  |  _( )_  | |  | \ `'   / |  (_.\.' /   \  `-'    / |  (_,_)\  |
                          \        /  \ (_ o _) / |  |  \    /  |       .'     \       /  |  |    |  |
                           `'-...-'    '.(_,_).'  ''-'   `'-'   '-----'`        `'-..-'   '--'    '--'
)foo");

    start_wifi();

    auto watering_clock = SockPtr(new Socket(1));
    auto watering_moisture = SockPtr(new Socket(1));

    Clock clock(watering_clock->connect());
    Moisture moisture(watering_moisture->connect());
    Watering watering(std::move(watering_clock), std::move(watering_moisture));

    xTaskCreate(service<Clock>, "clock", 1024 * 4, &clock, 2, NULL);
    xTaskCreate(service<Moisture>, "moisture", 1024 * 4, &moisture, 2, NULL);
    xTaskCreate(service<Watering>, "watering", 1024 * 4, &watering, 2, NULL);

    for (int i = 0;; i++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}