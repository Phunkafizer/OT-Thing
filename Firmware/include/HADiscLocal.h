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
};

extern OTThingHADiscovery haDisc;