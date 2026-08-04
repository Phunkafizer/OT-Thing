#pragma once

#include "HADiscovery.h"
#include "mqtt.h"

class OTThingHADiscovery: public HADiscovery {
public:
    OTThingHADiscovery();
    void begin();
    using HADiscovery::createSwitch;
    void createSwitch(String name, Mqtt::MqttTopic topic);
    using HADiscovery::publish;
    bool publish(const bool avail = true);
    HADiscovery::ClimateAction calcAction(const bool active, const bool enabled, const HADiscovery::ClimateAction actAction = HADiscovery::ACTION_HEATING);
};

extern OTThingHADiscovery haDisc;