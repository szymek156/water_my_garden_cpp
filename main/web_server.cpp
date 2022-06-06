#include "web_server.hpp"

#include <esp_http_server.h>
#include <esp_system.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>
static const char* TAG = "Webserver";

/* version GET handler */
static esp_err_t version_get_handler(httpd_req_t* req) {
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    auto* ctx = (WebServer*)req->user_ctx;

    auto resp_str = ctx->get_version();

    httpd_resp_send(req, resp_str.c_str(), HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* status GET handler */
static esp_err_t status_get_handler(httpd_req_t* req) {
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    auto* ctx = (WebServer*)req->user_ctx;

    ESP_LOGD(TAG, "Get clock status...");
    auto resp = ctx->get_clock_status();

    ESP_LOGD(TAG, "Get moisture status...");
    resp += ctx->get_moisture_status();

    ESP_LOGD(TAG, "Get watering status...");
    resp += ctx->get_watering_status();

    ESP_LOGD(TAG, "system status response %s", resp.c_str());

    // TODO: be a json someday
    httpd_resp_send(req, resp.c_str(), HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

WebServer::WebServer(SockPtr clock, SockPtr moisture, SockPtr watering)
    : clock_(std::move(clock)),
      moisture_(std::move(moisture)),
      watering_(std::move(watering)) {
}

std::string WebServer::get_version() {
    return "Water my garden version 0.0.1";
}

std::string WebServer::get_clock_status() {
    auto msg = Message{};
    msg.type = Message::Type::Status;

    clock_->send(msg);

    if (auto data = moisture_->rcv(pdMS_TO_TICKS(500))) {
        auto msg = *data;
        return msg.status;
    }

    return "Failed to get clock status";
}

std::string WebServer::get_moisture_status() {
    auto msg = Message{};
    msg.type = Message::Type::Status;

    moisture_->send(msg);

    if (auto data = moisture_->rcv(pdMS_TO_TICKS(500))) {
        auto msg = *data;
        return msg.status;
    }

    return "Failed to get moisture status";
}

std::string WebServer::get_watering_status() {
    auto msg = Message{};
    msg.type = Message::Type::Status;

    watering_->send(msg);

    if (auto data = moisture_->rcv(pdMS_TO_TICKS(500))) {
        auto msg = *data;
        return msg.status;
    }

    return "Failed to get watering status";
}

httpd_handle_t WebServer::start_webserver() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    httpd_uri_t version = {.uri = "/version",
                           .method = HTTP_GET,
                           .handler = version_get_handler,
                           /* Let's pass response string in user
                            * context to demonstrate it's usage */
                           .user_ctx = this};

    httpd_uri_t status = {
        .uri = "/status", .method = HTTP_GET, .handler = status_get_handler, .user_ctx = this};

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &version);
        httpd_register_uri_handler(server, &status);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}
