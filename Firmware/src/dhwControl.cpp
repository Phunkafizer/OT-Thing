#include "dhwControl.h"
#include "devstatus.h"
#include "otvalues.h"
#include "devconfig.h"

bool DHWControl::loop() {
    if (schedule.getSetpoint(setpoint)) {
        mqtt.sendValue(Mqtt::TOPIC_DHWSETTEMP, String(setpoint, 0));
        return true;
    }

    return false;
}

void DHWControl::setConfig(JsonObject &obj) {
    on = obj[F("dhwOn")];
    setpoint = obj[F("dhwTemperature")] | 45;
    ovrd.active = obj[F("overrideDhw")] | false;
    schedule.setConfig(obj[F("dhwSchedule")]);
}

bool DHWControl::getOn() const {
    return ovrd.active ? ovrd.on : on;
}

void DHWControl::setOn(const bool on) {
    this->on = on;
}

double DHWControl::getTemp() {
    if (devconfig.overrideEnabled && ovrd.active)
        return ovrd.temp;
    else
        return setpoint;
}

void DHWControl::setTemp(const double temp) {
    setpoint = temp;
}

bool DHWControl::getDhwActive() const {
    OTValueStatus *ots = static_cast<OTValueStatus*>(OTValue::getSlaveValue(OpenThermMessageID::Status));
    if (ots)
        return ots->getDhwActive();

    return false;
}

void DHWControl::getJson(JsonObject &obj) {
    obj[FPSTR(STR_STATKEY_OVERRIDE)] = ovrd.active;
    obj[FPSTR(STR_STATKEY_CTRLMODE)] = haDisc.getClimateModeStr(on ? HADiscovery::MODE_HEAT : HADiscovery::MODE_OFF);
    obj[FPSTR(STR_STATKEY_SETPOINT)] = ovrd.active ? ovrd.temp : setpoint;

    const HADiscovery::ClimateAction action = haDisc.calcAction(getDhwActive(), getOn());
    obj[FPSTR(STR_STATKEY_ACTION)] = haDisc.getClimateActionStr(action);
}