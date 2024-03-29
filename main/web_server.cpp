#include "web_server.hpp"

#include <esp_http_server.h>
#include <esp_system.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>
#include "cJSON.h"
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

    ESP_LOGD(TAG, "system status response '%s'", resp.c_str());

    // TODO: be a json someday
    httpd_resp_send(req, resp.c_str(), HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/* configuration GET handler */
static esp_err_t configuration_get_handler(httpd_req_t* req) {
    /* Send response with custom headers and body set as the
     * string passed in user context*/
    auto* ctx = (WebServer*)req->user_ctx;

    ESP_LOGD(TAG, "Get watering configuration...");
    auto resp = ctx->get_watering_configuration();

    ESP_LOGD(TAG, "get configuration response '%s'", resp.c_str());

    // TODO: be a json someday
    httpd_resp_send(req, resp.c_str(), HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


/* configuration POST handler */
static esp_err_t configuration_post_handler(httpd_req_t* req) {

    char content[512] = {};
    size_t recv_size = std::min(req->content_len, sizeof(content));
    int ret = httpd_req_recv(req, content, recv_size);

    if (ret <= 0) {
        ESP_LOGD(TAG, "Failed to fetch request from configuration POST handler, status %d", ret);
        return ESP_FAIL;
    }



    /* Send response with custom headers and body set as the
     * string passed in user context*/
    auto* ctx = (WebServer*)req->user_ctx;

    ESP_LOGD(TAG, "Get watering configuration...");
    auto resp = ctx->set_watering_configuration(std::string(content));

    ESP_LOGD(TAG, "get configuration response '%s'", resp.c_str());

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

    if (auto data = clock_->rcv(pdMS_TO_TICKS(500))) {
        auto msg = *data;
        auto guard = std::unique_ptr<char []>(msg.status);

        return guard.get();
    }

    return "Failed to get clock status";
}

std::string WebServer::get_moisture_status() {
    auto msg = Message{};
    msg.type = Message::Type::Status;

    moisture_->send(msg);

    if (auto data = moisture_->rcv(pdMS_TO_TICKS(500))) {
        auto msg = *data;
        // Some crazy/ugly stuff is going there.
        // In union there cannot be anything that has constructor/dtor
        // so only option is to pass char * that is allocated.
        // To avoid memleaks I need to wrap it in unique_ptr
        // Returning data() will call std::string ctor that will copy the
        // content, and return. Shortly after guard gets deleted.
        // All of that could be avoided if move semantics would be implicit
        // As it is in... ekhm Rust, ekhm.
        auto guard = std::unique_ptr<char []>(msg.status);
        return guard.get();
    }

    return "Failed to get moisture status";
}

std::string WebServer::get_watering_status() {
    auto msg = Message{};
    msg.type = Message::Type::Status;

    watering_->send(msg);

    if (auto data = watering_->rcv(pdMS_TO_TICKS(500))) {
        auto msg = *data;
        auto guard = std::unique_ptr<char []>(msg.status);

        return guard.get();
    }

    return "Failed to get watering status";
}

std::string WebServer::get_watering_configuration() {
    auto msg = Message{};
    msg.type = Message::Type::GetConfiguration;

    watering_->send(msg);

    if (auto data = watering_->rcv(pdMS_TO_TICKS(500))) {
        auto msg = *data;
        auto guard = std::unique_ptr<char []>(msg.status);

        return guard.get();
    }

    return "Failed to get watering configuration";
}

std::string WebServer::set_watering_configuration(std::string payload) {
    ESP_LOGI(TAG, "set configuration=%s", payload.c_str());

    auto msg = Message{};
    msg.type = Message::Type::SetConfiguration;

    cJSON *conf = cJSON_Parse(payload.c_str());

    if (conf == nullptr) {
        return "failed to parse JSON";
    }

	if (cJSON_GetObjectItem(conf, "section_name")) {
        strncpy(msg.section_name, cJSON_GetObjectItem(conf, "section_name")->valuestring, 32);
	} else {
        cJSON_Delete(conf);
        return "missing section_name field";
    }

	if (cJSON_GetObjectItem(conf, "enabled")) {
		msg.enabled = cJSON_GetObjectItem(conf,"enabled")->valueint;
	} else {
        cJSON_Delete(conf);
        return "missing enabled field";
    }

    if (cJSON_GetObjectItem(conf, "duration_seconds")) {
		msg.duration_seconds = cJSON_GetObjectItem(conf,"duration_seconds")->valueint;
	} else {
        cJSON_Delete(conf);
        return "missing duration_seconds field";
    }

    if (cJSON_GetObjectItem(conf, "wet_threshold")) {
		msg.wet_threshold = cJSON_GetObjectItem(conf,"wet_threshold")->valuedouble;
	} else {
        cJSON_Delete(conf);
        return "missing wet_threshold field";
    }

    cJSON_Delete(conf);

    watering_->send(msg);

    if (auto data = watering_->rcv(pdMS_TO_TICKS(500))) {
        auto msg = *data;
        auto guard = std::unique_ptr<char []>(msg.status);

        return guard.get();
    }

    return "Failed to set watering configuration";
}

httpd_handle_t WebServer::start_webserver() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    httpd_uri_t version = {.uri = "/version",
                           .method = HTTP_GET,
                           .handler = version_get_handler,
                           .user_ctx = this};

    httpd_uri_t status = {
        .uri = "/status", .method = HTTP_GET, .handler = status_get_handler, .user_ctx = this};

    httpd_uri_t get_configuration = {
        .uri = "/configuration", .method = HTTP_GET, .handler = configuration_get_handler, .user_ctx = this};

    httpd_uri_t set_configuration = {
        .uri = "/configuration", .method = HTTP_POST, .handler = configuration_post_handler, .user_ctx = this};

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &version);
        httpd_register_uri_handler(server, &status);
        httpd_register_uri_handler(server, &get_configuration);
        httpd_register_uri_handler(server, &set_configuration);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}
