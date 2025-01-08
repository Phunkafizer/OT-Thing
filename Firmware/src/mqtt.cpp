#include "mqtt.h"
#include <WiFi.h>
#include <devstatus.h>
#include "HADiscLocal.h"

Mqtt mqtt;
static WiFiClient espClient;

static void mqttConnectCb(bool sessionPresent) {
}

static void mqttMessageReceived(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    mqtt.onMessage(payload, total);
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
    stateTopic = F("home/OTThing/");
    stateTopic += shortMac;
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
        haDisc.stateTopic = stateTopic;
    }

    if (cli.connected()) {
        if (newConnection) {
            newConnection = false;

            cli.onMessage(mqttMessageReceived);
            String topic = stateTopic + F("/set");
            auto id = cli.subscribe(topic.c_str(), 0);
            Serial.print("Subscripbe ");
            Serial.print(id);
            Serial.println(topic);
        }

        if ((millis() - lastStatus) > 5000) {
            lastStatus = millis();
            cli.publish(stateTopic.c_str(), 0, false, devstatus.getJson().c_str());
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

void Mqtt::onMessage(const char *payload, const size_t size) {
    Serial.println("Payload");

    JsonDocument doc;
    deserializeJson(doc, payload, size);

    for (JsonPair p: doc.as<JsonObject>()) {
        Serial.println(p.key().c_str());
    }
}