#pragma once

#include "socket.hpp"

#include <string>

#include <esp_http_server.h>

class WebServer {
 public:
    WebServer(SockPtr clock, SockPtr moisture, SockPtr watering);

    httpd_handle_t start_webserver();

    std::string get_version();

    std::string get_clock_status();
    std::string get_moisture_status();
    std::string get_watering_status();

 private:
    SockPtr clock_;
    SockPtr moisture_;
    SockPtr watering_;
};
