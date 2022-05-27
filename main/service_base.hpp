#pragma once
#include "socket.hpp"

#include <algorithm>
#include <memory>
#include <set>

#include <freertos/queue.h>

class ServiceBase {
 public:
    ServiceBase() : queues_(xQueueCreateSet(10)) {
    }

 protected:
    QueueSetHandle_t queues_;
};