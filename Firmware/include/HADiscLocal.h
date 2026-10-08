#pragma once

#include "HADiscovery.h"
#include "mqtt.h"


class OTThingHADiscovery: public HADiscovery {
public:
    OTThingHADiscovery();
    void begin();
    using HADiscovery::createSwitch;
    void createSwitch(String name, Mqtt::MqttTopic topic);
    void createNumber(String name, Mqtt::MqttTopic topic);
    void createClima(String name, Mqtt::MqttTopic topic);
    using HADiscovery::createTempSensor;
    void createTempSensor(String name, Mqtt::MqttTopic topic);
    using HADiscovery::publish;
    bool publish(const bool avail = true);
    bool publish(const bool avail, const Mqtt::ValueTemplateType vt, PGM_P field, const uint8_t ch=-1);
    HADiscovery::ClimateAction calcAction(const bool active, const bool enabled, const HADiscovery::ClimateAction actAction = HADiscovery::ACTION_HEATING);
    void setSlaveAvailability();
    void setRoomunitAvailability();
};

extern OTThingHADiscovery haDisc;