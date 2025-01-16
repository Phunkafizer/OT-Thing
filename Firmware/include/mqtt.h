#pragma once

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

extern const char *MQTTSETVAR_OUTSIDETEMP PROGMEM;
extern const char *MQTTSETVAR_DHWSETTEMP PROGMEM;
extern const char *MQTTSETVAR_CHSETTEMP PROGMEM;
extern const char *MQTTSETVAR_CHMODE PROGMEM;

struct MqttConfig {
    String host;
    uint16_t port;
    bool tls;
    String user;
    String pass;
};

class Mqtt {
private:
    void onConnect();
    friend void mqttConnectCb(bool sessionPresent);
    AsyncMqttClient cli;
    uint32_t lastConTry;
    uint32_t lastStatus;
    MqttConfig config;
    bool configSet;
    bool newConnection;
    String baseTopic;
public:
    Mqtt();
    void begin();
    void loop();
    bool connected();
    void setConfig(const MqttConfig conf);
    bool publish(String topic, JsonDocument &payload);
    void onMessage(const char *topic, const char *payload, const size_t size);
    String getVarSetTopic(const char *str);
};

extern Mqtt mqtt;