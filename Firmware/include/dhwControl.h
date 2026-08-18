#pragma once
#include <ArduinoJson.h>
#include "scheduler.h"
#include "masterrequests.h"

class DHWControl {
private:
    Scheduler schedule;
    bool on;
    bool onRU;
    double setpoint;
    double setpointRU {0};
    double setpointRUReadback {0};
    bool getDhwActive() const;
    enum CtrlSource {
        SOURCE_OTTHING = 0,
        SOURCE_ROOMUNIT = 1,
        SOURCE_AUTO = 2
    } ctrlSource {SOURCE_AUTO};
public:
    OTWRSetDhw setDhwRequest;
    void loop();
    void setConfig(JsonObject &obj);
    bool getOn() const;
    void setOn(const bool on);
    void setOnRU(const bool on);
    double getTemp();
    void getJson(JsonObject &obj);
    bool setSetpoint(const double temp);
    void setSetpointRU(const double temp);
    double getSetpointRU() const;
    bool sendDiscoveries(const bool en);
};