#include "CHcontrol.h"
#include "flamestats.h"
#include "devstatus.h"
#include "otcontrol.h"

bool CHcontrol::overrideEnabled; // set if otMode is master

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

    modeStr = haDisc.getClimateModeStr(roomComp.mode);
    if (modeStr != nullptr)
        obj[FPSTR(STR_STATKEY_ROOMMODE)] = FPSTR(modeStr);

    HADiscovery::ClimateAction action;
    if (getChOn())
        if (otcontrol.getFlame() && otcontrol.getChActive(channel))
            action = HADiscovery::ACTION_HEATING;
        else
            action = HADiscovery::ACTION_IDLE;
    else
        action = HADiscovery::ACTION_OFF;
    obj[FPSTR(STR_STATKEY_ACTION)] = haDisc.getClimateActionStr(action);

    obj[FPSTR(STR_STATKEY_SUSPENDED)] = roomSuspended || minSuspended || outSuspended;

    double d;
    if (returnTemp[channel].get(d)) {
        obj[F("returnTemp")] = d;
        obj[F("reduction")] = round(retLimit.reduction * 10) / 10.0;
    }

    // calculate roomaction
    HADiscovery::ClimateAction roomAction;
    if (overrideEnabled && ovrdOn.active)
        roomAction = ovrdOn.value ? HADiscovery::ACTION_HEATING : HADiscovery::ACTION_OFF;
    else
        if (config.roomSuspend.enabled && roomSuspended)
            roomAction = HADiscovery::ACTION_IDLE;
        else
            if (otcontrol.getChActive(channel))
                roomAction = HADiscovery::ACTION_HEATING;
            else
                roomAction = HADiscovery::ACTION_OFF;
    obj[FPSTR(STR_STATKEY_ROOMACTION)] = haDisc.getClimateActionStr(roomAction);
}

double CHcontrol::getFlow() {
    double result = config.flow;

    if (overrideEnabled && ovrdTemp.active) {
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
    return result;
}

bool CHcontrol::getChOn() {
    if (overrideEnabled && ovrdOn.active)
        return ovrdOn.value;

    if ( (mode == HADiscovery::MODE_OFF) || (roomComp.mode == HADiscovery::MODE_OFF) || (getFlow() == 0) )
        return false;

    if (config.roomSuspend.enabled && roomSuspended)
        return false;

    if (config.minSuspend && minSuspended)
        return false;

    if (outSuspended)
        return false;

    return true;
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

void CHcontrol::loop() {
    double rt, rsp;

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