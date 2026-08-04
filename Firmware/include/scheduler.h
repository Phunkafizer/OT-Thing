#pragma once

#include <vector>
#include <ArduinoJson.h>
#include <stdint.h>

class Scheduler {
private:
    struct SchedulerEntry{
        uint8_t days;
        uint16_t time; // minutes after midnight
        double temp;
    };
    std::vector<SchedulerEntry> entries;
    int8_t lastIdx { -1 };
    bool active {false};
    int8_t getCurrentIdx() const;
public:
    void setConfig(JsonObject obj);
    bool getSetpoint(double &temp);
};