#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// Definition of actions that can be taken on each service
struct Message {
    enum class Type {
        MoistureReq,
        MoistureRes,
        Alarm1Expired,
        Alarm2Expired,
        SetAlarm1,
        SetAlarm2,
        ClearAlarm1,
        ClearAlarm2,
        Status

    } type;

    union {
        // MoistureReq
        struct {
            int section;
        };

        // MoistureRes
        struct {
            int section_r;
            float moisture;
        };

        // Alarm1Expired
        struct {};

        // SetAlarm1, SetAlarm2
        struct {
            struct tm alarm_tm;
        };

        // Status
        struct {
            char* status;
        };
    };
};

class Socket;
using SockPtr = std::unique_ptr<Socket>;

/// 1-1 bidirectional communication
class Socket {
 public:
    Socket(size_t queue_depth) {
        connected_ = false;

        rx_ = xQueueCreate(queue_depth, sizeof(Message));
        configASSERT(rx_);

        tx_ = xQueueCreate(queue_depth, sizeof(Message));
        configASSERT(tx_);
    }

    virtual ~Socket() {
        vQueueDelete(rx_);
        vQueueDelete(tx_);
    }

    SockPtr connect() {
        if (connected_) {
            // ESP_LOGE(TAG, "Cannot connect to already connected socket");
            abort();
        }

        return SockPtr(new Socket(tx_, rx_));
    }

    BaseType_t send(Message req) {
        return xQueueSendToBack(tx_, &req, 0);
    }

    std::optional<Message> rcv(int timeout) {
        Message res;
        if (xQueueReceive(rx_, &res, timeout) == pdPASS) {
            return std::optional(res);
        } else {
            return std::nullopt;
        }
    }

    QueueHandle_t get_rx() {
        return rx_;
    }

 private:
    Socket(QueueHandle_t rx, QueueHandle_t tx) {
        connected_ = true;
        rx_ = rx;
        tx_ = tx;

        configASSERT(tx_);
        configASSERT(rx_);
    }

    static constexpr const char *TAG = "Socket";
    bool connected_;
    QueueHandle_t rx_;
    QueueHandle_t tx_;

    Socket(const Socket &) = delete;
    Socket operator=(const Socket &) = delete;
};

