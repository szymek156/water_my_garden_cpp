#pragma once
#include "socket.hpp"

#include <memory>
#include <unordered_map>

#include <freertos/queue.h>
#include <i2cdev.h>


class Clock {
 public:
    Clock();
    void run_service();

   // TODO: assuming connections are established before service starts
    template <typename Msg>
    void add_connection(std::unique_ptr<Socket<Msg>> sock) {
        assert(connections_.find(IndexOf<Msg>()) != connections_.cend());


      // TODO: is it task safe?
        xQueueAddToSet(sock->get_rx(), queues_);
        connections_.insert({IndexOf<Msg>(), std::move(sock)});
    }

 private:
    esp_err_t init_rtc();

    i2c_dev_t dev;

    QueueSetHandle_t queues_;

    std::unordered_map<TypeIndex, std::unique_ptr<ISocket>> connections_;
};