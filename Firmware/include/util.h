#pragma once

#include <Arduino.h>
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

extern void clip(double &d, const double min, const double max);
extern String getShortMac();
extern void publishMdnsServices();