#include "dhwControl.h"
#include "devstatus.h"
#include "otvalues.h"
#include "devconfig.h"

void DHWControl::loop() {
    double tmp;
    if (schedule.getSetpoint(tmp)) {
        if (setSetpoint(tmp))
            mqtt.sendValue(Mqtt::TOPIC_DHWSETTEMP, String(setpoint, 0));
    }
}

void DHWControl::setConfig(JsonObject &obj) {
    on = obj[F("dhwOn")];
    setpoint = obj[F("dhwTemperature")] | 45;
    ctrlSource = (CtrlSource) (obj[F("dhwCtrlSource")] | SOURCE_AUTO);
    schedule.setConfig(obj[F("dhwSchedule")]);
    setpointRUReadback = 0; // in order to force roomunit to send setpoint
    setDhwRequest.force();
}

bool DHWControl::getOn() const {
    return on;
}

void DHWControl::setOn(const bool on) {
    this->on = on;
}

void DHWControl::setOnRU(const bool on) {
    if (this->onRU != on) {
        this->onRU = on;
        if ((ctrlSource == SOURCE_AUTO) || (ctrlSource == SOURCE_ROOMUNIT))
            this->on = on;
    }
}

double DHWControl::getTemp() {
    return setpoint;
}

bool DHWControl::getDhwActive() const {
    OTValueStatus *ots = static_cast<OTValueStatus*>(OTValue::getSlaveValue(OpenThermMessageID::Status));
    if (ots)
        return ots->getDhwActive();

    return false;
}

void DHWControl::getJson(JsonObject &obj) {
    obj[FPSTR(STR_STATKEY_CTRLMODE)] = haDisc.getClimateModeStr(on ? HADiscovery::MODE_HEAT : HADiscovery::MODE_OFF);
    obj[FPSTR(STR_STATKEY_SETPOINT)] = setpoint;

    const HADiscovery::ClimateAction action = haDisc.calcAction(getDhwActive(), getOn());
    obj[FPSTR(STR_STATKEY_ACTION)] = haDisc.getClimateActionStr(action);
}

/**
 * sets setpoint changed from HTTP, MQTT or timer
 */
bool DHWControl::setSetpoint(const double temp) {
    if (temp != setpoint) {
        if ((ctrlSource == SOURCE_AUTO) || (ctrlSource == SOURCE_OTTHING)) {
            setpoint = temp;
            setpointRUReadback = temp;
            setDhwRequest.force();
            return true;
        }
    }
    return false;
}

/**
 * sets setpoint written from roomunit (dhw_set_t)
 */
void DHWControl::setSetpointRU(const double temp) {
    setpointRUReadback = temp;
    if (temp != setpointRU) {
        setpointRU = temp;
        if ((ctrlSource == SOURCE_AUTO) || (ctrlSource == SOURCE_ROOMUNIT)) {
            setpoint = temp;
            setDhwRequest.force();
        }
    }
}

double DHWControl::getSetpointRU() const {
    return setpointRUReadback;
}

bool DHWControl::sendDiscoveries(const bool en) {
    haDisc.createClima(F("DHW"), Mqtt::getTopicString(Mqtt::TOPIC_DHWSETTEMP), mqtt.getCmdTopic(Mqtt::TOPIC_DHWSETTEMP));
    haDisc.setMinMaxTemp(5, 65, 1);
    haDisc.setCurrentTemperatureTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_SLAVE, PSTR("dhw_t")));
    haDisc.setInitial(45);
    haDisc.setModeCommandTopic(mqtt.getCmdTopic(Mqtt::TOPIC_DHWMODE));
    haDisc.setTemperatureStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_DHW, STR_STATKEY_SETPOINT));
    haDisc.setModeStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_DHW, STR_STATKEY_CTRLMODE));
    haDisc.setActionTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_DHW, STR_STATKEY_ACTION));
    haDisc.setOptimistic(true);
    haDisc.setIcon(F("mdi:water-heater"));
    haDisc.setRetain(true);
    haDisc.setModes(0x03);
    if (!haDisc.publish(en))
        return false;

    haDisc.createSwitch(F("DHW blocking"), Mqtt::TOPIC_DHWBLOCKING);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_ROOT, STR_STATKEY_DHWBLOCKING));
    if (!haDisc.publish(en))
        return false;

    return true;
}