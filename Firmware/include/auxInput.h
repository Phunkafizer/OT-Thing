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
        MODE_1WIRE = 4
    } mode;

    void setup();
    void loop();
    void setConfig(JsonObject cfg);
    void getJson(JsonDocument &doc) const;
    bool sendDiscovery();
};

extern AuxInput auxInput[2];