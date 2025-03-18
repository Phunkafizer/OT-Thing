#include "mqtt.h"
#include <WiFi.h>
#include <devstatus.h>
#include "HADiscLocal.h"
#include "otcontrol.h"
#include "portal.h"
#include "sensors.h"
#include "HADiscLocal.h"
#include "hwdef.h"

const char *MQTTSETVAR_OUTSIDETEMP PROGMEM = "outsideTemp";
const char *MQTTSETVAR_DHWSETTEMP PROGMEM = "dwhSetTemp";
const char *MQTTSETVAR_CHSETTEMP1 PROGMEM = "chSetTemp1";
const char *MQTTSETVAR_CHSETTEMP2 PROGMEM = "chSetTemp2";
const char *MQTTSETVAR_CHMODE1 PROGMEM = "chMode1";
const char *MQTTSETVAR_CHMODE2 PROGMEM = "chMode2";
const char *MQTTSETVAR_ROOMTEMP1 PROGMEM = "roomTemp1";
const char *MQTTSETVAR_ROOMTEMP2 PROGMEM = "roomTemp2";
const char *MQTTSETVAR_ROOMSETPOINT1 PROGMEM = "roomSetpoint1";
const char *MQTTSETVAR_ROOMSETPOINT2 PROGMEM = "roomSetpoint2";

Mqtt mqtt;
static WiFiClient espClient;

void mqttConnectCb(bool sessionPresent) {
    Serial.println("MQTT connect cb");
    mqtt.onConnect();
}

void mqttDisconnectCb(AsyncMqttClientDisconnectReason reason) {
    Serial.print("MQTT Disc ");
    Serial.println((int) reason);
}

static void mqttMessageReceived(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    mqtt.onMessage(topic, payload, total);
}

Mqtt::Mqtt():
        lastConTry(0),
        lastStatus(0),
        configSet(false) {
    cli.onConnect(mqttConnectCb);
    cli.onDisconnect(mqttDisconnectCb);
    cli.onMessage(mqttMessageReceived);
}

void Mqtt::begin() {
    String shortMac = WiFi.macAddress();
    shortMac.remove(0, 9);
    int idx;
    while ( (idx = shortMac.indexOf(':')) >= 0)
        shortMac.remove(idx, 1);
    baseTopic = F("otthing/");
    baseTopic += shortMac;
}

void Mqtt::onConnect() {
    portal.textAll(F("MQTT connected"));
    Serial.println(F("MQTT connected"));

    String statusTopic = baseTopic + F("/status");
    cli.setWill(statusTopic.c_str(), 0, false, "offline");
    cli.publish(statusTopic.c_str(), 0, false, "online");

    String topic = baseTopic + F("/+/set");
    cli.subscribe(topic.c_str(), 0);
    otcontrol.resetDiscovery();
}

bool Mqtt::connected() {
    return cli.connected();
}

void Mqtt::setConfig(const MqttConfig conf) {
    config = conf;
    lastConTry = millis() - 8000;
    cli.disconnect(false);
    cli.setServer(config.host.c_str(), config.port);
    cli.setCredentials(config.user.c_str(), config.pass.c_str());
    configSet = !config.host.isEmpty();
}

String Mqtt::getVarSetTopic(const char *str) {
    String result = baseTopic + '/';
    result += FPSTR(str);
    result += "/set";
    return result;
}

String Mqtt::getBaseTopic() {
    return baseTopic;
}

void Mqtt::loop() {
    if (!cli.connected() && ((millis() - lastConTry) > 10000) && WiFi.isConnected() && configSet) {
        Serial.println("Connecting MQTT...");
        lastConTry = millis();
        cli.connect();
        haDisc.defaultStateTopic = baseTopic + F("/state");
    }

    if (cli.connected()) {
        if ((millis() - lastStatus) > 5000) {
            lastStatus = millis();
            String payload;
            devstatus.lock();
            devstatus.getJson(payload);
            devstatus.unlock();
            cli.publish(haDisc.defaultStateTopic.c_str(), 0, false, payload.c_str());
        }
    }
}

bool Mqtt::publish(String topic, JsonDocument &payload, const bool retain) {
    if (!cli.connected())
        return false;

    String ps;
    if (!payload.isNull())
        serializeJson(payload, ps);
    cli.publish(topic.c_str(), 0, retain, ps.c_str());
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
        outsideTemp.set(d, OutsideTemp::SOURCE_MQTT);
        return;
    }

    tmp = FPSTR(MQTTSETVAR_DHWSETTEMP);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        otcontrol.setDhwTemp(d);
        return;
    }

    tmp = FPSTR(MQTTSETVAR_CHSETTEMP1);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        otcontrol.setChTemp(d, 0);
        return;
    }

    tmp = FPSTR(MQTTSETVAR_CHMODE1);
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

        otcontrol.setChCtrlMode(mode, 0);
        return;
    }

    tmp = FPSTR(MQTTSETVAR_ROOMTEMP1);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        roomTemp[0].set(d, Sensor::SOURCE_MQTT);
        return;
    }

    tmp = FPSTR(MQTTSETVAR_ROOMTEMP2);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        roomTemp[1].set(d, Sensor::SOURCE_MQTT);
        return;
    }

    tmp = FPSTR(MQTTSETVAR_ROOMSETPOINT1);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        roomSetPoint[0].set(d, Sensor::SOURCE_MQTT);
        return;
    }

    tmp = FPSTR(MQTTSETVAR_ROOMSETPOINT2);
    if (topicStr.compareTo(tmp) == 0) {
        double d = String(payload).toFloat();
        roomSetPoint[1].set(d, Sensor::SOURCE_MQTT);
        return;
    }
}
