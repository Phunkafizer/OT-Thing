#ifndef _devstatus_h
#define _devstatus_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "freertos/FreeRTOS.h"

extern class DevStatus {
private:
    SemaphoreHandle_t mutex;
public:
    DevStatus();
    void lock();
    void unlock();
    JsonDocument &buildDoc();
    void getJson(String &str);
    uint32_t numWifiDiscon;
} devstatus;

#endif