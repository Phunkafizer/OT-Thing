#include "mqtt.h"
#include <WiFi.h>
#include <devstatus.h>
#include "HADiscLocal.h"
#include "portal.h"
#include "sensors.h"
#include "HADiscLocal.h"
#include "hwdef.h"
#include "auxInput.h"

static struct {
    Mqtt::MqttTopic topic;
    const char *str;
} topicList[] PROGMEM = {
    {Mqtt::TOPIC_OUTSIDETEMP, "outsideTemp"},
    {Mqtt::TOPIC_DHWSETTEMP, "dwhSetTemp"},
    {Mqtt::TOPIC_CHSETTEMP1, "chSetTemp1"},
    {Mqtt::TOPIC_CHSETTEMP2, "chSetTemp2"},
    {Mqtt::TOPIC_CHMINTEMP1, "chMinTemp1"},
    {Mqtt::TOPIC_CHMINTEMP2, "chMinTemp2"},
    {Mqtt::TOPIC_DHWMODE, "dhwMode"},
    {Mqtt::TOPIC_CHMODE1, "chMode1"},
    {Mqtt::TOPIC_CHMODE2, "chMode2"},
    {Mqtt::TOPIC_ROOMTEMP1, "roomTemp1"},
    {Mqtt::TOPIC_ROOMTEMP2, "roomTemp2"},
    {Mqtt::TOPIC_ROOMCOMP1, "roomComp1"},
    {Mqtt::TOPIC_ROOMCOMP2, "roomComp2"},
    {Mqtt::TOPIC_ROOMSETPOINT1, "roomSetpoint1"},
    {Mqtt::TOPIC_ROOMSETPOINT2, "roomSetpoint2"},
    {Mqtt::TOPIC_OVERRIDECH1, "overrideCh1"},
    {Mqtt::TOPIC_OVERRIDECH2, "overrideCh2"},
    {Mqtt::TOPIC_OVERRIDEDHW, "overrideDhw"},
    {Mqtt::TOPIC_VENTSETPOINT, "ventSetpoint"},
    {Mqtt::TOPIC_VENTENABLE, "ventEnable"},
    {Mqtt::TOPIC_OPENBYPASS, "openBypass"},
    {Mqtt::TOPIC_AUTOBYPASS, "autoBypass"},
    {Mqtt::TOPIC_FREEVENTENABLE, "freeVentEnable"},
    {Mqtt::TOPIC_MAXMODULATION, "maxModulation"},
    {Mqtt::TOPIC_BYPASS, "bypass"}
};

Mqtt mqtt;
static uint32_t numDisc = 0;
static WiFiClient espClient;
static char statBuf[4096];

void mqttConnectCb(bool sessionPresent) {
    mqtt.onConnect();
}

void mqttDisconnectCb(AsyncMqttClientDisconnectReason reason) {
    mqtt.onDisconnect(reason);
}

static void mqttMessageReceived(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    String payloadStr(payload, len);
    mqtt.onMessage(topic, payloadStr);
}

Mqtt::Mqtt():
        lastConTry(0),
        lastStatus(0),
        configSet(false),
        conFlag(false) {
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
    conFlag = true;
}

void Mqtt::onDisconnect(AsyncMqttClientDisconnectReason reason) {
    if (conFlag) {
        String msg = F("MQTT disconnected ");
        msg += String((int) reason);
        portal.textAll(msg);
        conFlag = false;
        numDisc++;
    }
}

bool Mqtt::connected() {
    return cli.connected();
}

void Mqtt::setConfig(const MqttConfig conf) {
    lastConTry = millis() - 8000;
    cli.disconnect(false);
    config = conf;
    cli.setServer(config.host.c_str(), conf.port);
    cli.setKeepAlive(config.keepAlive);
    cli.setCredentials(config.user.c_str(), config.pass.c_str());
    configSet = !conf.host.isEmpty();
    numDisc = 0;
}

uint32_t Mqtt::getNumDisc() const {
    return numDisc;
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
            discFlag &= OneWireNode::sendDiscoveryAll();
            discFlag &= BLESensor::sendDiscoveryAll();
            for (int i=0; i<sizeof(auxInput) / sizeof(auxInput[0]); i++)
                discFlag &= auxInput[i].sendDiscovery();
        }

        if ((millis() - lastStatus) > 5000) {
            lastStatus = millis();
            JsonDocument doc;
            devstatus.buildDoc(doc);
            serializeJson(doc, statBuf);
            cli.publish(haDisc.defaultStateTopic.c_str(), 0, false, statBuf);
            cli.publish(statusTopic.c_str(), 0, false, PSTR("online"));
        }
    }
}

OTControl::CtrlMode Mqtt::strToCtrlMode(const String &str) {
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

    setValue(topicStr, payload);
}

bool Mqtt::setValue(const String &key, const String &value) {
    enum MqttTopic etop = TOPIC_UNKNOWN;
    for (int i=0; i<sizeof(topicList) / sizeof(topicList[0]); i++)
        if (key.compareTo(FPSTR(topicList[i].str)) == 0) {
            etop = topicList[i].topic;
            break;
        }

    if (etop == TOPIC_UNKNOWN) {
        String log = F("MQTT unknown topic: ");
        log += key;
        portal.textAll(log);
        return false;
    }

    switch (etop) {
    case TOPIC_OUTSIDETEMP: {
        double d = value.toFloat();
        outsideTemp.set(d, Sensor::SOURCE_MQTT);
        break;
    }

    case TOPIC_DHWSETTEMP: {
        double d = value.toFloat();
        otcontrol.setDhwTemp(d);
        break;
    }   

    case TOPIC_DHWMODE: {
        OTControl::CtrlMode mode = strToCtrlMode(value);
        if (mode != OTControl::CTRLMODE_UNKNOWN)
            otcontrol.setDhwCtrlMode(mode);
        break;
    }

    case TOPIC_CHSETTEMP1:
    case TOPIC_CHSETTEMP2: {
        double d = value.toFloat();
        otcontrol.setChTemp(d, (uint8_t) (etop - TOPIC_CHSETTEMP1));
        break;
    }

    case TOPIC_CHMINTEMP1:
    case TOPIC_CHMINTEMP2: {
        double d = value.toFloat();
        otcontrol.setFlowMin(d, (uint8_t) (etop - TOPIC_CHMINTEMP1));
        break;
    }

    case TOPIC_CHMODE1:
    case TOPIC_CHMODE2: {
        const uint8_t ch = (uint8_t) (etop - TOPIC_CHMODE1);
        OTControl::CtrlMode mode = strToCtrlMode(value);
        if (mode != OTControl::CTRLMODE_UNKNOWN)
            otcontrol.setChCtrlMode(mode, ch);
        break;
    }

    case TOPIC_ROOMTEMP1:
    case TOPIC_ROOMTEMP2: {
        const uint8_t ch = (uint8_t) (etop - TOPIC_ROOMTEMP1);
        double d = value.toFloat();
        roomTemp[ch].set(d, Sensor::SOURCE_MQTT);
        otcontrol.forceFlowCalc(ch);
        break;
    }

    case TOPIC_ROOMSETPOINT1:
    case TOPIC_ROOMSETPOINT2: {
        const uint8_t ch = (uint8_t) (etop - TOPIC_ROOMSETPOINT1);
        double d = value.toFloat();
        roomSetPoint[ch].set(d, Sensor::SOURCE_MQTT);
        otcontrol.forceFlowCalc(ch);
        break;
    }

    case TOPIC_ROOMCOMP1:
    case TOPIC_ROOMCOMP2: {
        OTControl::CtrlMode mode = strToCtrlMode(value);
        otcontrol.setRoomComp(mode == OTControl::CTRLMODE_AUTO, (uint8_t) (etop - TOPIC_ROOMCOMP1));
        break;
    }

    case TOPIC_OVERRIDECH1:
    case TOPIC_OVERRIDECH2:
        otcontrol.setOverrideCh(value == F("ON"), (uint8_t) (etop - TOPIC_OVERRIDECH1));
        break;

    case TOPIC_OVERRIDEDHW:
        otcontrol.setOverrideDhw(value == F("ON"));
        break;

    case TOPIC_VENTSETPOINT: {
        uint8_t val = value.toInt();
        otcontrol.setVentSetpoint(val);
        break;
    }

    case TOPIC_VENTENABLE:
        otcontrol.setVentEnable(value == F("ON"));
        break;

    case TOPIC_OPENBYPASS:
        break;

    case TOPIC_AUTOBYPASS:
        break;

    case TOPIC_FREEVENTENABLE:
        break;

    case TOPIC_MAXMODULATION: {
        int i = value.toInt();
        otcontrol.setMaxMod(i);
        break;
    }

    case TOPIC_BYPASS:
        otcontrol.setBypass(value == F("ON"));
        break;

    default:
        break;
    }
    return true;
}

String Mqtt::getTopicString(const MqttTopic topic) {
    for (int i=0; i<sizeof(topicList) / sizeof(topicList[0]); i++)
        if (topicList[i].topic == topic)
            return FPSTR(topicList[i].str);
    return "";
}
