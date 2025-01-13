#include "mqtt.h"
#include <WiFi.h>
#include <devstatus.h>
#include "HADiscLocal.h"
#include "otcontrol.h"
#include "outsidetemp.h"

const char *MQTTSETVAR_OUTSIDETEMP PROGMEM = "outsideTemp";
const char *MQTTSETVAR_DHWSETTEMP PROGMEM = "dwhSetTemp";

Mqtt mqtt;
static WiFiClient espClient;

void mqttConnectCb(bool sessionPresent) {
    mqtt.onConnect();
}

static void mqttMessageReceived(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    mqtt.onMessage(topic, payload, total);
}

Mqtt::Mqtt():
        lastConTry(0),
        lastStatus(0),
        configSet(false),
        newConnection(false) {
    String shortMac = WiFi.macAddress();
    shortMac.remove(0, 9);
    int idx;
    while ( (idx = shortMac.indexOf(':')) >= 0)
        shortMac.remove(idx, 1);
    baseTopic = F("otthing/");
    baseTopic += shortMac;
}

void Mqtt::begin() {
    cli.onConnect(mqttConnectCb);
}

bool Mqtt::connected() {
    return cli.connected();
}

void Mqtt::setConfig(const MqttConfig conf) {
    config = conf;
    configSet = true;
    lastConTry = -10000;
    if (cli.connected())
        cli.disconnect();
}

void Mqtt::loop() {
    if (!cli.connected() && ((millis() - lastConTry) > 10000) && WiFi.isConnected() && configSet) {
        lastConTry = millis();

        cli.setServer(config.host.c_str(), config.port);
        cli.connect();
        newConnection = true;
        haDisc.defaultStateTopic = baseTopic + F("/state");
    }

    if (cli.connected()) {
        if (newConnection) {
            newConnection = false;

            String statusTopic = baseTopic + ("/status");
            cli.setWill(statusTopic.c_str(), 0, false, "offline");
            cli.publish(statusTopic.c_str(), 0, false, "online");

            otcontrol.resetDiscovery();

            cli.onMessage(mqttMessageReceived);
            String topic = baseTopic + F("/+/set");
            cli.subscribe(topic.c_str(), 0);
        }

        if ((millis() - lastStatus) > 5000) {
            lastStatus = millis();
            cli.publish(haDisc.defaultStateTopic.c_str(), 0, false, devstatus.getJson().c_str());
        }
    }
}

bool Mqtt::publish(String topic, JsonDocument &payload) {
    if (!cli.connected())
        return false;

    String ps;
    serializeJson(payload, ps);
    cli.publish(topic.c_str(), 0, true, ps.c_str());
    return true;
}

void Mqtt::onMessage(const char *topic, const char *payload, const size_t size) {
    Serial.println("Subscription received");

    String topicStr = topic;
    topicStr.remove(0, baseTopic.length() + 1);
    topicStr.remove(topicStr.length() - 4, 4);
    Serial.println(topicStr);

    String tmp = FPSTR(MQTTSETVAR_OUTSIDETEMP);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        outsideTemp.temp = d;
        return;
    }

    tmp = FPSTR(MQTTSETVAR_DHWSETTEMP);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        Serial.println(d);
        otcontrol.setDhwTemp(d);
        return;
    }
}