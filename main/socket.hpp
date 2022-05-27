#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

enum class Messages {
    Watering
};


// TODO: find better place
struct WateringMessage {
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

    QueueHandle_t get_rx() {
        return rx_;
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

// poor's man std::type_index without rtti
using TypeIndex = int;

// Declare template function, but do not define it's generic form
// In case of usage with unsupported type, like : IndexOf<std::string>
// you will get meaningful (as of c++ standards) compiler message:
// error: 'constexpr TypeIndex IndexOf() [with T = std::__cxx11::basic_string<char>; TypeIndex = int]' used before its definition
template<typename T>
constexpr TypeIndex IndexOf();

template<>
constexpr int IndexOf<WateringMessage>() {
    return 1;
}

template<>
constexpr int IndexOf<int>() {
    return 2;
}

static_assert(IndexOf<WateringMessage>() == 1);
static_assert(IndexOf<int>() == 2);


