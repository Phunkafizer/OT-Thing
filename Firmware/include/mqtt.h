#ifndef _mqtt_h
#define _mqtt_h

#include <PubSubClient.h>

struct MqttConfig {
    String host;
    uint16_t port;
    bool tls;
};

class Mqtt {
private:
    PubSubClient cli;
    uint32_t lastConTry;
    uint32_t lastStatus;
    MqttConfig config;
    bool configSet;
public:
    Mqtt();
    void begin();
    void loop();
    bool connected();
    void setConfig(const MqttConfig conf);
};

extern Mqtt mqtt;

#endif