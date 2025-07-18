#pragma once

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

extern const char *MQTTSETVAR_OUTSIDETEMP PROGMEM;
extern const char *MQTTSETVAR_DHWSETTEMP PROGMEM;
extern const char *MQTTSETVAR_CHSETTEMP1 PROGMEM;
extern const char *MQTTSETVAR_CHSETTEMP2 PROGMEM;
extern const char *MQTTSETVAR_CHMODE1 PROGMEM;
extern const char *MQTTSETVAR_CHMODE2 PROGMEM;
extern const char *MQTTSETVAR_ROOMTEMP1 PROGMEM;
extern const char *MQTTSETVAR_ROOMTEMP2 PROGMEM;
extern const char *MQTTSETVAR_ROOMSETPOINT1 PROGMEM;
extern const char *MQTTSETVAR_ROOMSETPOINT2 PROGMEM;

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
    String baseTopic;
    String statusTopic;
public:
    Mqtt();
    void begin();
    void loop();
    bool connected();
    void setConfig(const MqttConfig conf);
    bool publish(String topic, JsonDocument &payload, const bool retain);
    void onMessage(const char *topic, String &payload);
    String getVarSetTopic(const char *str);
    String getBaseTopic();
};

extern Mqtt mqtt;