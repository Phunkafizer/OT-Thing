#pragma once

#include <ArduinoJson.h>

class AuxInput {
private:
    bool state;
public:
    enum Mode {
        MODE_UNUSED = 0,
        MODE_BINARY = 1,
        MODE_COUNTER = 2,
        MODE_ANALOG = 3
    } mode;

    void setup();
    void loop();
    void setConfig(JsonObject cfg);
    void getJson(JsonDocument &doc) const;
    bool sendDiscovery();
};

extern AuxInput auxInput;