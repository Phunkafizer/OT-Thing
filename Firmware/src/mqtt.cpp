#include "mqtt.h"
#include <WiFi.h>
#include <devstatus.h>
#include "HADiscLocal.h"
#include "portal.h"
#include "sensors.h"
#include "HADiscLocal.h"
#include "hwdef.h"

static struct {
    Mqtt::MqttTopic topic;
    const char *str;
} topicList[] PROGMEM = {
    {Mqtt::TOPIC_OUTSIDETEMP, "outsideTemp"},
    {Mqtt::TOPIC_DHWSETTEMP, "dwhSetTemp"},
    {Mqtt::TOPIC_CHSETTEMP1, "chSetTemp1"},
    {Mqtt::TOPIC_CHSETTEMP2, "chSetTemp2"},
    {Mqtt::TOPIC_DHWMODE, "dhwMode"},
    {Mqtt::TOPIC_CHMODE1, "chMode1"},
    {Mqtt::TOPIC_CHMODE2, "chMode2"},
    {Mqtt::TOPIC_ROOMTEMP1, "roomTemp1"},
    {Mqtt::TOPIC_ROOMTEMP2, "roomTemp2"},
    {Mqtt::TOPIC_ROOMSETPOINT1, "roomSetpoint1"},
    {Mqtt::TOPIC_ROOMSETPOINT2, "roomSetpoint2"},
    {Mqtt::TOPIC_OVERRIDECH1, "overrideCh1"},
    {Mqtt::TOPIC_OVERRIDECH2, "overrideCh2"},
    {Mqtt::TOPIC_OVERRIDEDHW, "overrideDhw"},
    {Mqtt::TOPIC_VENTSETPOINT, "ventSetpoint"},
    {Mqtt::TOPIC_VENTENABLE, "ventEnable"},
    {Mqtt::TOPIC_OPENBYPASS, "openBypass"},
    {Mqtt::TOPIC_AUTOBYPASS, "autoBypass"},
    {Mqtt::TOPIC_FREEVENTENABLE, "freeVentEnable"}
};

Mqtt mqtt;
static WiFiClient espClient;

void mqttConnectCb(bool sessionPresent) {
    mqtt.onConnect();
}

void mqttDisconnectCb(AsyncMqttClientDisconnectReason reason) {
}

static void mqttMessageReceived(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    String payloadStr(payload, len);
    mqtt.onMessage(topic, payloadStr);
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
    statusTopic = baseTopic + F("/status");
}

void Mqtt::onConnect() {
    portal.textAll(F("MQTT connected"));

    cli.setWill(statusTopic.c_str(), 0, true, "offline");

    String topic = baseTopic + F("/+/set");
    cli.subscribe(topic.c_str(), 0);

    discFlag = false;
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

String Mqtt::getCmdTopic(const MqttTopic topic) {
    String result = baseTopic + '/';
    result += getTopicString(topic);
    result += "/set";
    return result;
}

String Mqtt::getBaseTopic() {
    return baseTopic;
}

void Mqtt::loop() {
    if (!cli.connected() && ((millis() - lastConTry) > 10000) && WiFi.isConnected() && configSet) {
        lastConTry = millis();
        cli.connect();
        haDisc.defaultStateTopic = baseTopic + F("/state");
    }

    if (cli.connected()) {
        if (!discFlag) {
            discFlag = true;
            discFlag &= otcontrol.sendDiscovery();
            discFlag &= OneWireNode::sendDiscovery();
        }

        if ((millis() - lastStatus) > 5000) {
            lastStatus = millis();
            String payload;
            devstatus.lock();
            devstatus.getJson(payload);
            devstatus.unlock();
            cli.publish(haDisc.defaultStateTopic.c_str(), 0, false, payload.c_str());
            cli.publish(statusTopic.c_str(), 0, false, "online");
        }
    }
}

OTControl::CtrlMode Mqtt::strToCtrlMode(String &str) {
    if (str.compareTo("heat") == 0)
        return OTControl::CTRLMODE_ON;
    if (str.compareTo("auto") == 0)
        return OTControl::CTRLMODE_AUTO;
    if (str.compareTo("off") == 0)
        return OTControl::CTRLMODE_OFF;
    return OTControl::CTRLMODE_UNKNOWN;
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

void Mqtt::onMessage(const char *topic, String &payload) {
    String topicStr = topic;
    topicStr.remove(0, baseTopic.length() + 1);
    topicStr.remove(topicStr.length() - 4, 4);

    String log = F("MQTT: ");
    log += topic;
    log += " ";
    log += payload;
    portal.textAll(log);

    enum MqttTopic etop = TOPIC_UNKNOWN;
    for (int i=0; i<sizeof(topicList) / sizeof(topicList[0]); i++)
        if (topicStr.compareTo(FPSTR(topicList[i].str)) == 0) {
            etop = topicList[i].topic;
            break;
        }

    switch (etop) {
    case TOPIC_OUTSIDETEMP: {
        double d = payload.toFloat();
        outsideTemp.set(d, Sensor::SOURCE_MQTT);
        break;
    }

    case TOPIC_DHWSETTEMP: {
        double d = payload.toFloat();
        otcontrol.setDhwTemp(d);
        break;
    }

    case TOPIC_CHSETTEMP1: {
        double d = payload.toFloat();
        otcontrol.setChTemp(d, 0);
        break;
    }

    case TOPIC_CHMODE1: {
        OTControl::CtrlMode mode = strToCtrlMode(payload);
        if (mode != OTControl::CTRLMODE_UNKNOWN)
            otcontrol.setChCtrlMode(mode, 0);
        break;
    }

    case TOPIC_ROOMTEMP1: {
        double d = payload.toFloat();
        roomTemp[0].set(d, Sensor::SOURCE_MQTT);
        otcontrol.forceFlowCalc(0);
        break;
    }

    case TOPIC_ROOMTEMP2: {
        double d = payload.toFloat();
        roomTemp[1].set(d, Sensor::SOURCE_MQTT);
        otcontrol.forceFlowCalc(1);
        break;
    }

    case TOPIC_ROOMSETPOINT1: {
        double d = payload.toFloat();
        roomSetPoint[0].set(d, Sensor::SOURCE_MQTT);
        otcontrol.forceFlowCalc(0);
        break;
    }

    case TOPIC_ROOMSETPOINT2: {
        double d = payload.toFloat();
        roomSetPoint[1].set(d, Sensor::SOURCE_MQTT);
        otcontrol.forceFlowCalc(1);
        break;
    }

    case TOPIC_OVERRIDECH1:
        otcontrol.setOverrideCh(payload == F("ON"), 0);
        break;

    case TOPIC_OVERRIDECH2:
        otcontrol.setOverrideCh(payload == F("ON"), 1);
        break;

    case TOPIC_OVERRIDEDHW:
        otcontrol.setOverrideDhw(payload == F("ON"));
        break;

    case TOPIC_VENTSETPOINT: {
        uint8_t val = payload.toInt();
        otcontrol.setVentSetpoint(val);
        break;
    }

    case TOPIC_VENTENABLE:
        otcontrol.setVentEnable(payload == F("ON"));
        break;

    case TOPIC_OPENBYPASS:
        break;

    case TOPIC_AUTOBYPASS:
        break;

    case TOPIC_FREEVENTENABLE:
        break;

    default:
        break;
    }
}

String Mqtt::getTopicString(const MqttTopic topic) {
    for (int i=0; i<sizeof(topicList) / sizeof(topicList[0]); i++)
        if (topicList[i].topic == topic)
            return FPSTR(topicList[i].str);
    return "";
}
