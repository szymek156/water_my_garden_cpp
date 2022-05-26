#pragma once

#include <memory>
#include <optional>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// TODO: find better place
struct Message {
    enum class Type {
        StartWatering,
        SetAlarm,

    } type;

    union {
        // StartWatering
        struct {};

        // SetAlarm
        struct {
            struct tm alarm_tm;
        };
    };
};

/// 1-1 bidirectional communication
template <typename T>
class Socket {
 public:
    using SockPtr = std::unique_ptr<Socket<T>>;

    Socket(size_t queue_depth) : connected_(false) {
        rx_ = xQueueCreate(queue_depth, sizeof(T));
        configASSERT(rx_);

        tx_ = xQueueCreate(queue_depth, sizeof(T));
        configASSERT(tx_);
    }

    ~Socket() {
        vQueueDelete(rx_);
        vQueueDelete(tx_);
    }

    SockPtr connect() {
        if (connected_) {
            // ESP_LOGE(TAG, "Cannot connect to already connected socket");
            abort();
        }

        return SockPtr(new Socket<T>(tx_, rx_));
    }

    BaseType_t send(T req) {
        return xQueueSendToBack(tx_, &req, 0);
    }

    std::optional<T> rcv(int timeout) {
        T res;
        if (xQueueReceive(rx_, &res, timeout) == pdPASS) {
            return std::optional(res);
        } else {
            return std::nullopt;
        }
    }

 private:
    static constexpr const char *TAG = "Socket";
    bool connected_;
    QueueHandle_t rx_;
    QueueHandle_t tx_;

    Socket(QueueHandle_t rx, QueueHandle_t tx) : connected_(true), rx_(rx), tx_(tx) {
        configASSERT(tx_);
        configASSERT(rx_);
    }

    Socket(const Socket &) = delete;
    Socket operator=(const Socket &) = delete;
};