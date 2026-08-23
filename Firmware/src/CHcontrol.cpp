#include "CHcontrol.h"
#include "flamestats.h"
#include "devstatus.h"
#include "otcontrol.h"
#include "auxInput.h"
#include "otvalues.h"

CHcontrol::CHcontrol(const uint8_t channel):
        channel(channel) {
}

void CHcontrol::setConfig(JsonObject &obj, const bool init) {
    curve.setConfig(obj);

    bool chOn = obj[F("chOn")];
    mode = chOn ? HADiscovery::MODE_AUTO : HADiscovery::MODE_OFF;

    config.roomSet = obj[F("roomsetpoint")][F("temp")] | 21.0; // default room set point
    config.flow = obj[F("flow")] | 35;

    JsonObject rc = obj[F("roomComp")];
    config.roomComp.enabled = rc[F("enabled")] | false;
    config.roomComp.p = rc[F("p")] | 0.0;
    config.roomComp.i = rc[F("i")] | 0.0;
    config.roomComp.boost = rc[F("boost")] | 3.0;
    
    config.roomSuspend.hysteresis = obj[F("hysteresis")] | 0.1;
    config.roomSuspend.offset = obj[F("suspOffset")] | 0.0;
    config.roomSuspend.enabled = obj[F("enableHyst")] | false;
    config.minSuspend = obj[F("minSuspend")] | false;

    flowTemp = config.flow;
    flowMin = obj[F("flowMin")] | 20;

    ovrdTemp.active = obj[F("overrideFlow")] | false;
    ovrdOn.active = obj[F("overrideOn")] | false;
    if (!init) {
        ovrdTemp.value = flowTemp;
        ovrdOn.value = chOn;
    }

    roomComp.mode = config.roomComp.enabled ? HADiscovery::ClimateMode::MODE_AUTO : HADiscovery::MODE_HEAT;
    if (config.roomComp.i == 0)
        roomComp.integState = 0;
    
    if (!roomSetPoint[channel])
        roomSetPoint[channel].set(config.roomSet, Sensor::SOURCE_NA);

    schedule.setConfig(obj[F("schedule")]);
}

void CHcontrol::getJson(JsonObject &obj) {
    obj[FPSTR(STR_STATKEY_OVERRIDE_TEMP)] = ovrdTemp.active;
    obj[FPSTR(STR_STATKEY_OVERRIDE_ON)] = ovrdOn.active;

    PGM_P modeStr = haDisc.getClimateModeStr(mode);
    if (modeStr != nullptr)
        obj[FPSTR(STR_STATKEY_CTRLMODE)] = FPSTR(modeStr);

    obj[FPSTR(STR_STATKEY_ROOMCOMPINTEGRATOR)] = round(roomComp.integState * 10) / 10.0;
    obj[FPSTR(STR_STATKEY_RETURNLIMITINTEGRATOR)] = round(retLimit.integState * 10) / 10.0;
    obj[FPSTR(STR_STATKEY_FLOWMIN)] = flowMin;

    switch (mode) {
    case HADiscovery::MODE_HEAT:
        obj[FPSTR(STR_STATKEY_FLOWSETPOINT)] = flowTemp;
        break;
    case HADiscovery::MODE_AUTO:
        obj[FPSTR(STR_STATKEY_FLOWSETPOINT)] = getFlow();
        break;
    default:
        break;
    }

    modeStr = haDisc.getClimateModeStr(roomComp.mode);
    if (modeStr != nullptr)
        obj[FPSTR(STR_STATKEY_ROOMMODE)] = FPSTR(modeStr);

    const HADiscovery::ClimateAction action = haDisc.calcAction(otcontrol.getFlame() && getChActive(), getChOn());
    obj[FPSTR(STR_STATKEY_ACTION)] = haDisc.getClimateActionStr(action);

    obj[FPSTR(STR_STATKEY_SUSPENDED)] = roomSuspended || minSuspended || outSuspended;

    double d;
    if (returnTemp[channel].get(d)) {
        obj[F("returnTemp")] = d;
        obj[F("reduction")] = round(retLimit.reduction * 10) / 10.0;
    }

    // calculate roomaction
    HADiscovery::ClimateAction roomAction;
    if (devconfig.overrideEnabled && ovrdOn.active)
        roomAction = ovrdOn.value ? HADiscovery::ACTION_HEATING : HADiscovery::ACTION_OFF;
    else
        roomAction = haDisc.calcAction(getChActive(), config.roomSuspend.enabled && roomSuspended);
    obj[FPSTR(STR_STATKEY_ROOMACTION)] = haDisc.getClimateActionStr(roomAction);
}

double CHcontrol::getFlow() {
    double result = config.flow;

    if (devconfig.overrideEnabled && ovrdTemp.active) {
        if (ovrdTemp.value <= 0)
            return 0;
        return ovrdTemp.value;
    }

    switch (mode) {
    case HADiscovery::MODE_HEAT:
        result = flowTemp;
        break;

    case HADiscovery::MODE_AUTO: {
        double rsp = config.roomSet; // default room set point
        result = 0.0;
        if (roomSetPoint[channel].get(rsp))
            result = curve.getFlowTemp(rsp);

        if (std::isnan(result) || result < 0.0)
            result = 0.0;
        else
            if (result == 0.0)
                result = flowTemp;
        break;
    }

    case HADiscovery::MODE_OFF:
        return 0;
    
    default:
        break;
    }

    result += retLimit.reduction;

    if (roomCompEnabled()) {
        // room temperature compensation
        result += roomComp.deltaT;
    }

    if (config.minSuspend) {
        if (result > (flowMin + 0.2))
            minSuspended = false;

        if (result < (flowMin - 0.2))
            minSuspended = true;
    }
    else
        minSuspended = false;

    double ost;
    if (outsideTemp.get(ost)) {
        if (result > (ost + 0.2))
            outSuspended = false;

        if (result < (ost - 0.2))
            outSuspended = true;
    }
    else
        outSuspended = false;

    clip(result, flowMin, curve.getFlowMax());
    return round(result * 10) / 10.0;
}

bool CHcontrol::getChOn() {
    if (devconfig.overrideEnabled && ovrdOn.active)
        return ovrdOn.value;

    if (AuxInput::hasChDisable(channel))
        return false;

    if (AuxInput::hasChDemand(channel))
        return true;

    // Startup guard: in AUTO mode keep CH off for first 5 minutes when no
    // outside temperature is available to avoid unnecessary heating at startup.
    if ((mode == HADiscovery::MODE_AUTO) && (millis() < 300000UL) && !outsideTemp)
        return false;

    if ( (mode == HADiscovery::MODE_OFF) || (roomComp.mode == HADiscovery::MODE_OFF) || (getFlow() == 0.0) )
        return false;

    if (config.roomSuspend.enabled && roomSuspended)
        return false;

    if (config.minSuspend && minSuspended)
        return false;

    if (outSuspended)
        return false;

    return true;
}

bool CHcontrol::getChActive() const {
    return OTValue::status->getChActive(channel);
}

void CHcontrol::setMode(const HADiscovery::ClimateMode mode) {
    this->mode = mode;
}

void CHcontrol::setRoomComp(const HADiscovery::ClimateMode mode) {
    roomComp.mode = mode;
    if (!roomCompEnabled()) {
        roomComp.integState = 0;
        roomComp.deltaT = 0;
    }   
}

double CHcontrol::getFlowMax() const {
    return curve.getFlowMax();
}

bool CHcontrol::roomCompEnabled() const {
    return roomComp.mode == HADiscovery::MODE_AUTO;
}

bool CHcontrol::suspendEnabled() const {
    return config.roomSuspend.enabled || config.minSuspend;
}

bool CHcontrol::loop() {
    double schedTemp;
    double rt, rsp;
    bool res = false;

    if (schedule.getSetpoint(schedTemp)) {
        roomSetPoint[channel].set(schedTemp, Sensor::SOURCE_NA);
        mqtt.sendValue(static_cast<Mqtt::MqttTopic>(Mqtt::TOPIC_ROOMSETPOINT1 + channel), String(schedTemp, 1));
        res = true;
    }

    if (roomTemp[channel].get(rt) && roomSetPoint[channel].get(rsp) && config.roomSuspend.enabled) {
        if (roomSuspended) {
            if (rt < rsp - config.roomSuspend.hysteresis + config.roomSuspend.offset)
                roomSuspended = false;
        }
        else {
            if (rt > rsp + config.roomSuspend.hysteresis + config.roomSuspend.offset)
                roomSuspended = true;
        }
    }
    else
        roomSuspended = false;

    return res;
}

void CHcontrol::loopRoomComp() {
    double rt, rsp; // roomtemp, roomsetpoint
    
    roomComp.deltaT = 0;

    if (!roomTemp[channel].get(rt) || !roomSetPoint[channel].get(rsp))
        return;

    if (!roomComp.init) {
        roomComp.rspPrev = rsp;
        roomComp.init = true;
    }

    if (roomComp.mode != HADiscovery::MODE_AUTO)
        return;

    double e = rsp - rt; // error
    if ((e > -0.2) && (e < 0.2)) // deadband
        e = 0;

    // proportional part of PI controller
    double p = config.roomComp.p * e; // Kp * e
    
    // integral part of PI controller
    roomComp.integState += rsp - roomComp.rspPrev;
    roomComp.rspPrev = rsp;
    if (getChOn()) {
        if (e > 0)
            roomComp.integState += config.roomComp.i * e * PI_INTERVAL / 3600.0; // Ki * e * ts, ts = 30 s
        else
            roomComp.integState += config.roomComp.i * e * 0.3 * PI_INTERVAL / 3600.0; // slower as cooling takes more time
    }
    else
        roomComp.integState = roomComp.integState * 0.95; // decay

    // anti windup
    clip(roomComp.integState, -5, 5);

    double boost = 0;
    if (e > 1.0)
        boost = e * config.roomComp.boost; // e * Kb

    roomComp.deltaT = p + roomComp.integState + boost;

    // clipping
    clip(roomComp.deltaT, -5, 12);
}

void CHcontrol::loopReturnLimit() {
    retLimit.reduction = 0;

    double ret;
    if (!returnTemp[channel].get(ret))
        return;

    double roomSet = config.roomSet; // default room set point
    roomSetPoint[channel].get(roomSet);

    double rl = curve.getReturnLimit(roomSet);
    if (rl == 0.0)
        return;

    double e = rl - ret;

    if (e < 0) {
        // return temp. too high
        const double Kp = 1.0;
        retLimit.reduction = e * Kp;

        if (flameStats.getCurrentOnTime() >= 60) {
            const double Ki = 1.0; // /h
            retLimit.integState += Ki * e * PI_INTERVAL / 3600.0; // Ki * e * ts, ts = 30 s
            clip(retLimit.integState, -5, 0);
        }
        else
            retLimit.integState *= 0.95;
    }
    else {
        retLimit.integState *= 0.95;
    }

    retLimit.reduction += retLimit.integState;
}

bool CHcontrol::sendDiscoveries(const bool en) {
    auto replace = [](const char *str, const uint8_t val, const uint8_t ommit = -1) {
        String result = FPSTR(str);
        if (val == ommit)
            result.replace("#", "");
        else
            result.replace("#", String(val));

        result.trim();
        return result;
    };

    auto topic = [](const Mqtt::MqttTopic topic, const uint8_t ch) {
        return (Mqtt::MqttTopic) ((int) topic + ch);
    };

    String str = replace(PSTR("flow temperature #"), channel + 1, 1);
    Mqtt::MqttTopic tp = topic(Mqtt::TOPIC_CHSETTEMP1, channel);
    haDisc.createClima(str, Mqtt::getTopicString(tp), mqtt.getCmdTopic(tp));
    haDisc.setMinMaxTemp(20, getFlowMax(), 0.5);
    haDisc.setCurrentTemperatureTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_SLAVE, PSTR("flow_t#"), channel + 1, 1));
    haDisc.setInitial(35);
    haDisc.setModeCommandTopic(mqtt.getCmdTopic(topic(Mqtt::TOPIC_CHMODE1, channel)));
    haDisc.setTemperatureStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_FLOWSETPOINT, channel));
    haDisc.setModeStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_CTRLMODE, channel));
    haDisc.setActionTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ACTION, channel));
    haDisc.setOptimistic(true);
    haDisc.setIcon(F("mdi:heating-coil"));
    haDisc.setRetain(true);
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("room temperature #"), channel + 1, 1);
    tp = topic(Mqtt::TOPIC_ROOMSETPOINT1, channel);
    haDisc.createClima(str, Mqtt::getTopicString(tp), mqtt.getCmdTopic(tp));
    haDisc.setMinMaxTemp(10, 30, 0.5);
    haDisc.setCurrentTemperatureTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMTEMP, channel));
    haDisc.setModeCommandTopic(mqtt.getCmdTopic(topic(Mqtt::TOPIC_ROOMMODE1, channel)));
    haDisc.setModeStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMMODE, channel));
    haDisc.setInitial(20);
    haDisc.setTemperatureStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMSETPOINT, channel));
    haDisc.setActionTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMACTION, channel));
    haDisc.setOptimistic(true);
    haDisc.setRetain(true);
    haDisc.setModes(0x00);
    if (!haDisc.publish(roomSetPoint[channel].isMqttSource() && en))
        return false;

    str = replace(PSTR("room setpoint #"), channel + 1, 1);
    tp = topic(Mqtt::TOPIC_ROOMSETPOINT1, channel);
    haDisc.createTempSensor(str, Mqtt::getTopicString(tp));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMSETPOINT, channel));
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("room temperature #"), channel + 1, 1);
    tp = topic(Mqtt::TOPIC_ROOMTEMP1, channel);
    haDisc.createNumber(str, Mqtt::getTopicString(tp), mqtt.getCmdTopic(tp));
    haDisc.setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    haDisc.setUnit(FPSTR(HA_UNIT_CELSIUS));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMTEMP, channel));
    haDisc.setMinMax(0, 30, 0.1);
    if (!haDisc.publish(roomSetPoint[channel].isMqttSource() && en))
        return false;

    str = replace(PSTR("room temperature #"), channel + 1, 1);
    String id = replace(PSTR("current_room_temp#"), channel + 1);
    haDisc.createTempSensor(str, id);
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMTEMP, channel));
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("roomcomp. integrator #"), channel + 1, 1);
    id = replace(PSTR("roomcomp_integ#"), channel + 1);
    haDisc.createSensor(str, id);
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMCOMPINTEGRATOR, channel));
    haDisc.setUnit(FPSTR(HA_UNIT_KELVIN));
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("ret. limit integrator #"), channel + 1, 1);
    id = replace(PSTR("retlimit_integ#"), channel + 1);
    haDisc.createSensor(str, id);
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_RETURNLIMITINTEGRATOR, channel));
    haDisc.setUnit(FPSTR(HA_UNIT_KELVIN));
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("suspend CH #"), channel + 1, 1);
    id = replace(PSTR("ch_susp#"), channel + 1, 1);
    haDisc.createBinarySensor(str, id, "");
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_HEATING_CIRCUIT, PSTR("suspended"), channel));
    if (!haDisc.publish(suspendEnabled() && en))
        return false;

    str = replace(PSTR("min. flow temperature #"), channel + 1, 1);
    tp = topic(Mqtt::TOPIC_CHMINTEMP1, channel);
    haDisc.createNumber(str, Mqtt::getTopicString(tp), mqtt.getCmdTopic(tp));
    haDisc.setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    haDisc.setUnit(FPSTR(HA_UNIT_CELSIUS));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_FLOWMIN, channel));
    haDisc.setMinMax(10, 50, 1);
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("override CH on #"), channel + 1, 1);
    tp = topic(Mqtt::TOPIC_OVERRIDECHON1, channel);
    haDisc.createSwitch(str, tp);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_OVERRIDE_ON, channel));
    if (!haDisc.publish(devconfig.overrideEnabled && en))
        return false;

    str = replace(PSTR("override CH flow #"), channel + 1, 1);
    tp = topic(Mqtt::TOPIC_OVERRIDECHFLOW1, channel);
    haDisc.createSwitch(str, tp);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_OVERRIDE_TEMP, channel));
    if (!haDisc.publish(devconfig.overrideEnabled && en))
        return false;

    return true;
}