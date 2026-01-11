#pragma once

#include "freertos/FreeRTOS.h"

class SemHelper {
private:
    SemaphoreHandle_t &mtx;
    BaseType_t result;
public:
    SemHelper(SemaphoreHandle_t &mtx, const uint16_t timeout);
    ~SemHelper();
    operator bool();
};