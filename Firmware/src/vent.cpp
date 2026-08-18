#include "vent.h"
#include "otvalues.h"
#include "otcontrol.h"
#include "devstatus.h"

void VentControl::setConfig(JsonObject &config) {
    ventEnable = config["ventEnable"] | false;
    openBypass = config["openBypass"] | false;
    autoBypass = config["autoBypass"] | false;
    freeVentEnable = config["freeVentEnable"] | false;
    setpoint = config["setpoint"] | 0;
    setVentSetpointRequest.force();
}

void VentControl::getJson(JsonObject &obj) const {
    obj[FPSTR(STR_STATKEY_ENABLE)] = ventEnable;
    obj[FPSTR(STR_STATKEY_OPENBYPASS)] = openBypass;
    obj[FPSTR(STR_STATKEY_AUTOBYPASS)] = autoBypass;
    obj[FPSTR(STR_STATKEY_FREEVENTENABLE)] = freeVentEnable;
    obj[FPSTR(STR_STATKEY_SETPOINT)] = setpoint;
}

bool VentControl::sendDiscoveries(const bool en) {
    haDisc.createNumber(F("ventilation set point"), Mqtt::getTopicString(Mqtt::TOPIC_VENTSETPOINT), mqtt.getCmdTopic(Mqtt::TOPIC_VENTSETPOINT));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_VENT, STR_STATKEY_SETPOINT));
    haDisc.setMinMax(0, 100, 1);
    haDisc.setOptimistic(true);
    haDisc.setRetain(true);
    if (!haDisc.publish(en))
        return false;

    haDisc.createSwitch(F("ventilation enable"), Mqtt::TOPIC_VENTENABLE);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_VENT, STR_STATKEY_ENABLE));
    if (!haDisc.publish(en))
        return false;

    haDisc.createSwitch(F("enable free vent."), Mqtt::TOPIC_FREEVENTENABLE);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_VENT, STR_STATKEY_FREEVENTENABLE));
    if (!haDisc.publish(en))
        return false;

    return true;
}

bool VentControl::sendCapDiscoveries() {
    haDisc.createSwitch(F("open bypass"), Mqtt::TOPIC_OPENBYPASS);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_VENT, STR_STATKEY_OPENBYPASS));
    if (!haDisc.publish(OTValue::ventSlaveConfig->hasBypass()))
        return false;

    haDisc.createSwitch(F("auto bypass"), Mqtt::TOPIC_AUTOBYPASS);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_VENT, STR_STATKEY_AUTOBYPASS));
    if (!haDisc.publish(OTValue::ventSlaveConfig->hasBypass()))
        return false;

    return true;
}

bool VentControl::loop() {
    if (setVentSetpointRequest) {
        setVentSetpointRequest.send(setpoint);
        return true;
    }

    if (millis() > lastVentStatus + 800) {
        lastVentStatus = millis();
        uint16_t data = getMasterStatus();
        unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::StatusVentilationHeatRecovery, data);
        otcontrol.sendRequest('T', req);
        return true;
    }
    
    return false;
}

void VentControl::setVentSetpoint(const uint8_t v) {
    setpoint = v;
    setVentSetpointRequest.force();
}

void VentControl::setVentEnable(const bool en) {
    ventEnable = en;
    setVentSetpointRequest.force();
}

void VentControl::setOpenBypass(const bool open) {
    openBypass = open;
    setVentSetpointRequest.force();
}
    
void VentControl::setAutoBypass(const bool autoBypass) {
    this->autoBypass = autoBypass;
    setVentSetpointRequest.force();
}

void VentControl::setFreeVentEnable(const bool en) {
    freeVentEnable = en;
    setVentSetpointRequest.force();
}

uint8_t VentControl::getSetpoint() const {
    return setpoint;
}

uint16_t VentControl::getMasterStatus() const {
    uint16_t data = 0;

    if (ventEnable)
        data |= 1<<OTValueVentMasterStatus::BIT_VENT_ENABLE;
    if (openBypass)
        data |= 1<<OTValueVentMasterStatus::BIT_OPEN_BYPASS;
    if (autoBypass)
        data |= 1<<OTValueVentMasterStatus::BIT_AUTO_BYPASS;
    if (freeVentEnable)
        data |= 1<<OTValueVentMasterStatus::BIT_FREE_VENT_ENABLE;

    return data;
}