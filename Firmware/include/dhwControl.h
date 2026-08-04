#pragma once
#include <ArduinoJson.h>
#include "scheduler.h"

class DHWControl {
private:
    Scheduler schedule;
    bool on;
    double setpoint;
    bool getDhwActive() const;
public:
    bool loop();
    void setConfig(JsonObject &obj);
    bool getOn() const;
    void setOn(const bool on);
    double getTemp();
    void setTemp(const double temp);
    void getJson(JsonObject &obj);
    struct {
        bool active;
        double temp;
        bool on;
    } ovrd;
};