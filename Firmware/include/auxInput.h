#pragma once

#include <ArduinoJson.h>
#include "devconfig.h"

class AuxInput {
private:
    bool state;
    const char* name;
    const uint8_t gpio;
public:
    AuxInput(const char *name, const uint8_t gpio);
    enum Mode {
        MODE_UNUSED = 0,
        MODE_BINARY = 1,
        MODE_COUNTER = 2,
        MODE_ANALOG = 3,
        MODE_1WIRE = 4,
    } mode;
    enum DigitalRole {
        DROLE_NONE = 0,
        DROLE_DEMAND_CH1 = 1,  // contact closed OR-s into CH1 enable
        DROLE_DEMAND_CH2 = 2,  // contact closed OR-s into CH2 enable
        DROLE_ENABLE_CH1 = 3,  // contact state AND-s with CH1 enable
        DROLE_ENABLE_CH2 = 4,  // contact state AND-s with CH2 enable
    } digitalRole;

    void setup();
    void loop();
    void setConfig(JsonObject cfg);
    void getJson(JsonDocument &doc) const;
    bool sendDiscovery();
    static bool hasEnableRole(uint8_t channel);
};

extern AuxInput auxInput[2];