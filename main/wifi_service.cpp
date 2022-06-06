#include "wifi_service.hpp"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <nvs_flash.h>
#include <string.h>

// Hides complexity of wifi configuration,
// code stolen from first found example

#define GARDEN_WIFI_SSID "smart.garden"
#define GARDEN_WIFI_PASS "garden.smart"
#define GARDEN_WIFI_CHANNEL 1
#define MAX_STA_CONN 4

static const char* TAG = "WIFI soft AP";

static void wifi_event_handler(void* arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {};
    // wifi_config.ap.ssid = GARDEN_WIFI_SSID;
    memcpy(wifi_config.ap.ssid, GARDEN_WIFI_SSID, strlen(GARDEN_WIFI_SSID));

    wifi_config.ap.ssid_len = strlen(GARDEN_WIFI_SSID);
    wifi_config.ap.channel = GARDEN_WIFI_CHANNEL;
    // wifi_config.ap.password = (uint8_t *)GARDEN_WIFI_PASS;
    memcpy(wifi_config.ap.password, GARDEN_WIFI_PASS, strlen(GARDEN_WIFI_PASS));

    wifi_config.ap.max_connection = MAX_STA_CONN;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG,
             "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             GARDEN_WIFI_SSID,
             GARDEN_WIFI_PASS,
             GARDEN_WIFI_CHANNEL);
}

void start_wifi() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
}