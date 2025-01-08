#ifndef _devstatus_h
#define _devstatus_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

extern class DevStatus {
private:
    JsonDocument doc;
public:
    DevStatus();
    String getJson();
    void getJson(AsyncResponseStream &response);
    void setOutsideTemp(double t);
} devstatus;

#endif