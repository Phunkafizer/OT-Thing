#include "CHcontrol.h"
#include "flamestats.h"
#include "devstatus.h"
#include "otcontrol.h"
#include "auxInput.h"
#include "otvalues.h"

template <typename T1>
bool ChannelOverride<T1>::isMasterOverride() const {
    return devconfig.masterOvrdEnabled && active;
}


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
    
    config.roomSuspend.hysteresis = obj[FPSTR(STR_CONFKEY_HYSTERESIS)] | 0.2;
    config.roomSuspend.offset = obj[F("suspOffset")] | 0.0;
    config.roomSuspend.enabled = obj[F("enableHyst")] | false;
    config.minSuspend = obj[F("minSuspend")] | false;
    config.minSuspendHyst = obj[F("minSuspHyst")] | 0.2;

    JsonObject os = obj[F("outsideSuspend")];
    config.outsideSuspend.type = static_cast<decltype(config.outsideSuspend.type)>(os[F("type")] | Config::OutsideSuspend::OUTSIDE_SUSPEND_CURRENT);
    config.outsideSuspend.hysteresis = os[FPSTR(STR_CONFKEY_HYSTERESIS)] | 0.2;
    config.outsideSuspend.offset = os[F("offset")] | 0.0;

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

    obj[FPSTR(STR_STATKEY_ROOMCOMPINTEGRATOR)] = round(roomComp.integState * 100) / 100.0;
    obj[FPSTR(STR_STATKEY_RETURNLIMITINTEGRATOR)] = round(retLimit.integState * 100) / 100.0;
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

    JsonObject jSuspended = obj[F("suspItems")].to<JsonObject>();
    jSuspended[F("room")] = roomSuspended;
    jSuspended[F("minFlow")] = minSuspended;
    jSuspended[F("outside")] = outSuspended;
    obj[FPSTR(STR_STATKEY_SUSPENDED)] = roomSuspended || minSuspended || outSuspended;

    double rsp, rt;
    if (roomSetPoint[channel].get(rsp))
        obj[FPSTR(STR_STATKEY_ROOMSETPOINT)] = rsp;
        
    if (roomTemp[channel].get(rt))
        obj[FPSTR(STR_STATKEY_ROOMTEMP)] = rt;

    if (roomSetPoint[channel].get(rsp) && roomTemp[channel].get(rt))
        obj[FPSTR(STR_STATKEY_PIERROR)] = ((rsp - rt) * 10) / 10.0;

    double d;
    if (returnTemp[channel].get(d)) {
        obj[F("returnTemp")] = d;
        obj[F("reduction")] = round(retLimit.reduction * 10) / 10.0;
    }

    // calculate roomaction
    HADiscovery::ClimateAction roomAction;
    if (devconfig.masterOvrdEnabled && ovrdOn.active)
        roomAction = ovrdOn.value ? HADiscovery::ACTION_HEATING : HADiscovery::ACTION_OFF;
    else
        roomAction = haDisc.calcAction(getChActive(), config.roomSuspend.enabled && roomSuspended);
    obj[FPSTR(STR_STATKEY_ROOMACTION)] = haDisc.getClimateActionStr(roomAction);

    if (lastFlowTempSrc != Sensor::SOURCE_NA)
        obj[FPSTR(STR_STATKEY_LASTSETPOINTSRC)] = (int) lastFlowTempSrc;

    if (turbo.endTime > time(nullptr)) {
        JsonObject turboObj = obj[F("turbo")].to<JsonObject>();
        turboObj[F("shift")] = turbo.shift;
        turboObj[F("duration")] = (uint32_t) ((turbo.endTime - time(nullptr) + 59) / 60);
    }

    if (flowStats.init) {
        JsonObject jfs = obj[F("flowStats")].to<JsonObject>();
        jfs[F("min")] = flowStats.min;
        jfs[F("max")] = flowStats.max;
    }
}

double CHcontrol::getFlow() {
    if (devconfig.masterOvrdEnabled && ovrdTemp.active) {
        // we are master/test, override value with roomunit value
        if (ovrdTemp.value <= 0)
            return 0;
        return ovrdTemp.value;
    }

    double result = config.flow;

    switch (mode) {
    case HADiscovery::MODE_HEAT:
        result = flowTemp;
        break;

    case HADiscovery::MODE_AUTO: {
        double rsp = config.roomSet; // default room set point
        roomSetPoint[channel].get(rsp);
        result = curve.getFlowTemp(rsp);

        if (std::isnan(result)) // happens when outside > roomset
            result = flowMin;
        if (result <= 0.0)
            result = flowMin;
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
        if (result > (flowMin + config.minSuspendHyst))
            minSuspended = false;

        if (result < (flowMin - config.minSuspendHyst))
            minSuspended = true;
    }
    else
        minSuspended = false;

    if (config.outsideSuspend.type == Config::OutsideSuspend::OUTSIDE_SUSPEND_DISABLED) {
        outSuspended = false;
    }
    else {
        double ost, rsp;
        bool calcOutsideSuspend = outsideTemp.get(ost) && roomSetPoint[channel].get(rsp);
        if (config.outsideSuspend.type == Config::OutsideSuspend::OUTSIDE_SUSPEND_AVERAGE)
            ost = outsideTemp.getAvg();

        if (calcOutsideSuspend) {
            if (ost > (rsp + config.outsideSuspend.offset + config.outsideSuspend.hysteresis))
                outSuspended = true;

            if (ost < (rsp + config.outsideSuspend.offset - config.outsideSuspend.hysteresis))
                outSuspended = false;
        }
        else
            outSuspended = false;
    }

    time_t now = time(nullptr);
    if (turbo.endTime > now)
        result += turbo.shift;
    
    clip(result, flowMin, curve.getFlowMax());
    return round(result * 10) / 10.0;
}

bool CHcontrol::getChOn() {
    if (devconfig.masterOvrdEnabled && ovrdOn.active)
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

bool CHcontrol::getCurrentFlow(double &d) const {
    OTValueFloatTemp* ft = nullptr;
    if (channel == 0)
        ft = static_cast<OTValueFloatTemp*>(OTValue::getSlaveValue(OpenThermMessageID::Tboiler));
    else
        ft = static_cast<OTValueFloatTemp*>(OTValue::getSlaveValue(OpenThermMessageID::TflowCH2));
    if (ft && ft->isSet()) {
        d = ft->getValue();
        return true;
    }
    return false;
}

void CHcontrol::setMode(const HADiscovery::ClimateMode mode) {
    this->mode = mode;
}

void CHcontrol::setFlowTemp(const double temp, const Sensor::Source src) {
    flowTemp = temp;
    lastFlowTempSrc = src;
}

void CHcontrol::flameChange(const bool newFlame) {
    flameStats.flameChange(newFlame);

    double d;
    if (getCurrentFlow(d)) {
        if (newFlame)
            flowStats.min = d;
        else
            flowStats.max = d;
    }
}

void CHcontrol::setRoomComp(const HADiscovery::ClimateMode mode) {
    const bool oldEnabled = roomCompEnabled();
    roomComp.mode = mode;
    if (oldEnabled != roomCompEnabled()) {
        roomComp.integState = 0;
        roomComp.deltaT = 0;
        if (!oldEnabled)
            loopRoomComp();
    }   
}

double CHcontrol::getFlowMax() const {
    return curve.getFlowMax();
}

bool CHcontrol::roomCompEnabled() const {
    return roomComp.mode == HADiscovery::MODE_AUTO;
}

bool CHcontrol::suspendEnabled() const {
    return config.roomSuspend.enabled
     || config.minSuspend 
     || (config.outsideSuspend.type != Config::OutsideSuspend::OUTSIDE_SUSPEND_DISABLED);
}

bool CHcontrol::loop() {
    double schedTemp;
    double rt, rsp;
    bool res = false;
    double flow;

    if (getCurrentFlow(flow)) {
        if (!flowStats.init) {
            flowStats.min = flow;
            flowStats.max = flow;
            flowStats.init = true;
        }

        flowStats.min = std::min(flowStats.min, flow);
        flowStats.max = std::max(flowStats.max, flow);
    }

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
    haDisc.createClima(str, topic(Mqtt::TOPIC_CHSETTEMP1, channel));
    haDisc.setMinMaxTemp(20, getFlowMax(), 0.5);
    haDisc.setCurrentTemperatureTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_SLAVE, PSTR("flow_t#"), channel + 1, 1));
    haDisc.setInitial(35);
    haDisc.setModeCommandTopic(mqtt.getCmdTopic(topic(Mqtt::TOPIC_CHMODE1, channel)));
    haDisc.setTemperatureStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_FLOWSETPOINT, channel));
    haDisc.setModeStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_CTRLMODE, channel));
    haDisc.setActionTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ACTION, channel));
    haDisc.setOptimistic(true);
    haDisc.setIcon(F("mdi:heating-coil"));
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("room temperature #"), channel + 1, 1);
    haDisc.createClima(str, topic(Mqtt::TOPIC_ROOMSETPOINT1, channel));
    haDisc.setMinMaxTemp(10, 30, 0.5);
    haDisc.setCurrentTemperatureTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMTEMP, channel));
    haDisc.setModeCommandTopic(mqtt.getCmdTopic(topic(Mqtt::TOPIC_ROOMMODE1, channel)));
    haDisc.setModeStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMMODE, channel));
    haDisc.setInitial(20);
    haDisc.setTemperatureStateTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMSETPOINT, channel));
    haDisc.setActionTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMACTION, channel));
    haDisc.setOptimistic(true);
    haDisc.setModes(0x00);
    if (!haDisc.publish(roomSetPoint[channel].isMqttSource() && en))
        return false;

    str = replace(PSTR("room setpoint #"), channel + 1, 1);
    haDisc.createTempSensor(str, topic(Mqtt::TOPIC_ROOMSETPOINT1, channel));
    if (!haDisc.publish(en, Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMSETPOINT, channel))
        return false;

    str = replace(PSTR("room temperature #"), channel + 1, 1);
    haDisc.createNumber(str, topic(Mqtt::TOPIC_ROOMTEMP1, channel));
    haDisc.setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    haDisc.setUnit(FPSTR(HA_UNIT_CELSIUS));
    haDisc.setMinMax(0, 30, 0.1);
    if (!haDisc.publish(roomSetPoint[channel].isMqttSource() && en, Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMTEMP, channel))
        return false;

    str = replace(PSTR("room temperature #"), channel + 1, 1);
    String id = replace(PSTR("current_room_temp#"), channel + 1);
    haDisc.createTempSensor(str, id);
    if (!haDisc.publish(en, Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMTEMP, channel))
        return false;

    str = replace(PSTR("roomcomp. integrator #"), channel + 1, 1);
    id = replace(PSTR("roomcomp_integ#"), channel + 1);
    haDisc.createSensor(str, id);
    haDisc.setUnit(FPSTR(HA_UNIT_KELVIN));
    if (!haDisc.publish(en, Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_ROOMCOMPINTEGRATOR, channel))
        return false;

    str = replace(PSTR("ret. limit integrator #"), channel + 1, 1);
    id = replace(PSTR("retlimit_integ#"), channel + 1);
    haDisc.createSensor(str, id);
    haDisc.setUnit(FPSTR(HA_UNIT_KELVIN));
    if (!haDisc.publish(en, Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_RETURNLIMITINTEGRATOR, channel))
        return false;

    str = replace(PSTR("CH suspended #"), channel + 1, 1);
    id = replace(PSTR("ch_susp#"), channel + 1);
    haDisc.createBinarySensor(str, id, "");
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_SUSPENDED, channel));
    if (!haDisc.publish(suspendEnabled() && en))
        return false;

    str = replace(PSTR("min. flow temperature #"), channel + 1, 1);
    haDisc.createNumber(str, topic(Mqtt::TOPIC_CHMINTEMP1, channel));
    haDisc.setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    haDisc.setUnit(FPSTR(HA_UNIT_CELSIUS));
    haDisc.setMinMax(10, 50, 1);
    if (!haDisc.publish(en, Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_FLOWMIN, channel))
        return false;

    str = replace(PSTR("override CH on #"), channel + 1, 1);
    haDisc.createSwitch(str, topic(Mqtt::TOPIC_OVERRIDECHON1, channel));
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_OVERRIDE_ON, channel));
    if (!haDisc.publish(otcontrol.getOverrideEnabled() && en))
        return false;

    str = replace(PSTR("override CH flow #"), channel + 1, 1);
    haDisc.createSwitch(str, topic(Mqtt::TOPIC_OVERRIDECHFLOW1, channel));
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_HEATING_CIRCUIT, STR_STATKEY_OVERRIDE_TEMP, channel));
    if (!haDisc.publish(otcontrol.getOverrideEnabled() && en))
        return false;

    return true;
}