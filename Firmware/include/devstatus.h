#ifndef _devstatus_h
#define _devstatus_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <mutex>

extern class DevStatus {
private:
    JsonDocument doc;
    std::mutex mutex;
public:
    DevStatus();
    void lock();
    void unlock();
    JsonDocument &buildDoc();
    void getJson(String &str);
    uint32_t numWifiDiscon;
} devstatus;

#endif