#ifndef _devstatus_h
#define _devstatus_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "util.h"

extern class DevStatus {
friend class DevStatusLock;
private:
    SemaphoreHandle_t mutex;
public:
    DevStatus();
    bool lock();
    void unlock();
    void buildDoc(JsonDocument &doc);
    uint32_t numWifiDiscon;
} devstatus;

#endif