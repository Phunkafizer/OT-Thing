#include "mqtt.h"
#include <WiFi.h>
#include <devstatus.h>
#include "HADiscLocal.h"
#include "otcontrol.h"
#include "outsidetemp.h"
#include "portal.h"

const char *MQTTSETVAR_OUTSIDETEMP PROGMEM = "outsideTemp";
const char *MQTTSETVAR_DHWSETTEMP PROGMEM = "dwhSetTemp";
const char *MQTTSETVAR_CHSETTEMP PROGMEM = "chSetTemp";
const char *MQTTSETVAR_CHMODE PROGMEM = "chMode";

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
    lastConTry = millis() - 10000;
    cli.disconnect();
    cli.setServer(config.host.c_str(), config.port);
    cli.setCredentials(config.user.c_str(), config.pass.c_str());
}

String Mqtt::getVarSetTopic(const char *str) {
    String result = baseTopic + '/';
    result += FPSTR(str);
    result += "/set";
    return result;
}

void Mqtt::loop() {
    if (!cli.connected() && ((millis() - lastConTry) > 10000) && WiFi.isConnected() && configSet) {
        lastConTry = millis();
        cli.connect();
        newConnection = true;
        haDisc.defaultStateTopic = baseTopic + F("/state");
    }

    if (cli.connected()) {
        if (newConnection) {
            newConnection = false;

            portal.textAll(F("MQTT connected"));

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
    String topicStr = topic;
    topicStr.remove(0, baseTopic.length() + 1);
    topicStr.remove(topicStr.length() - 4, 4);
    Serial.println(topicStr);

    String log = F("MQTT: ");
    log += topic;
    log += " ";
    log += payload;
    portal.textAll(log);

    String tmp = FPSTR(MQTTSETVAR_OUTSIDETEMP);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        if (outsideTemp.source = OutsideTemp::OUTSIDETEMP_MQTT)
            outsideTemp.temp = d;
        return;
    }

    tmp = FPSTR(MQTTSETVAR_DHWSETTEMP);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        otcontrol.setDhwTemp(d);
        return;
    }

    tmp = FPSTR(MQTTSETVAR_CHSETTEMP);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        otcontrol.setChTemp(d);
        return;
    }

    tmp = FPSTR(MQTTSETVAR_CHMODE);
    if (topicStr.compareTo(tmp) == 0) {
        OTControl::CtrlMode mode;
        if (String(payload).compareTo("heat") == 0)
            mode = OTControl::CTRLMODE_ON;
        else if (String(payload).compareTo("auto") == 0)
            mode = OTControl::CTRLMODE_AUTO;
        else if (String(payload).compareTo("off") == 0)
            mode = OTControl::CTRLMODE_OFF;
        else
            return;

        otcontrol.setChCtrlMode(mode);
        return;
    }
}
