#pragma once

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

struct MqttConfig {
    String host;
    uint16_t port;
    bool tls;
};

class Mqtt {
private:
    AsyncMqttClient cli;
    uint32_t lastConTry;
    uint32_t lastStatus;
    MqttConfig config;
    bool configSet;
    bool newConnection;
    String stateTopic;
public:
    Mqtt();
    void begin();
    void loop();
    bool connected();
    void setConfig(const MqttConfig conf);
    bool publish(String topic, JsonDocument &payload);
    void onMessage(const char *payload, const size_t size);
};

extern Mqtt mqtt;