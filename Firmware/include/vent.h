#pragma once

#include "masterrequests.h"
#include <ArduinoJson.h>

class VentControl {
private:
bool ventEnable;
    bool openBypass;
    bool autoBypass;
    bool freeVentEnable;
    uint8_t setpoint;
    unsigned long lastVentStatus {0};
public:
    OTWRSetVentSetpoint setVentSetpointRequest;
    bool loop();
    void setConfig(JsonObject &config);
    void getJson(JsonObject &obj) const;
    bool sendDiscoveries(const bool en);
    bool sendCapDiscoveries();
    void setVentSetpoint(const uint8_t v);
    void setVentEnable(const bool en);
    void setOpenBypass(const bool open);
    void setAutoBypass(const bool autoBypass);
    void setFreeVentEnable(const bool en);
    uint8_t getSetpoint() const;
    uint16_t getMasterStatus() const;
};

