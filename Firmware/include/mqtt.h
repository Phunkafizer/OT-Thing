#pragma once

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include "otcontrol.h"

struct MqttConfig {
    String host;
    uint16_t port;
    bool tls;
    String user;
    String pass;
    uint16_t keepAlive;
};

class Mqtt {
private:
    void onConnect();
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    friend void mqttConnectCb(bool sessionPresent);
    friend void mqttDisconnectCb(AsyncMqttClientDisconnectReason reason);
    AsyncMqttClient cli;
    uint32_t lastConTry;
    uint32_t lastStatus;
    MqttConfig config;
    bool configSet;
    String baseTopic;
    String statusTopic;
    bool discFlag {false}; // discovery flag; set after MQTT (re-) connect
    bool conFlag;
    OTControl::CtrlMode strToCtrlMode(String &str);
public:
    enum MqttTopic {
        TOPIC_UNKNOWN = -1,
        TOPIC_OUTSIDETEMP,
        TOPIC_DHWSETTEMP,
        TOPIC_CHSETTEMP1,
        TOPIC_CHSETTEMP2,
        TOPIC_DHWMODE,
        TOPIC_CHMODE1,
        TOPIC_CHMODE2,
        TOPIC_ROOMTEMP1,
        TOPIC_ROOMTEMP2,
        TOPIC_ROOMSETPOINT1,
        TOPIC_ROOMSETPOINT2,
        TOPIC_ROOMCOMP1,
        TOPIC_ROOMCOMP2,
        TOPIC_OVERRIDECH1,
        TOPIC_OVERRIDECH2,
        TOPIC_OVERRIDEDHW,
        TOPIC_VENTSETPOINT,
        TOPIC_VENTENABLE,
        TOPIC_OPENBYPASS,
        TOPIC_AUTOBYPASS,
        TOPIC_FREEVENTENABLE,
        TOPIC_MAXMODULATION
    };
    Mqtt();
    void begin();
    void loop();
    bool connected();
    void setConfig(const MqttConfig conf);
    bool publish(String topic, JsonDocument &payload, const bool retain);
    void onMessage(const char *topic, String &payload);
    String getBaseTopic();
    static String getTopicString(const MqttTopic topic);
    String getCmdTopic(const MqttTopic topic);
    uint32_t getNumDisc() const;
};

extern Mqtt mqtt;