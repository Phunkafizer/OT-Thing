#pragma once

#include "ArduinoJson.h"
#include "heatingcurve.h"
#include "sensors.h"

template <typename T1>
class ChannelOverride {
public:
    bool active; // overriding activated
    T1 value; // overriden value
};

enum ChannelControlMode: int8_t {
    CTRLMODE_UNKNOWN = -1,
    CTRLMODE_OFF = 0,
    CTRLMODE_ON = 1,
    CTRLMODE_AUTO = 2
};

class CHcontrol {
private:
    const uint8_t channel;
    struct {
        double roomSet; // default room set point
        double flow; // default flow temperature 
        struct {
            bool enabled; // suspend on roomsetpoint
            double hysteresis;
            double offset;
        } roomSuspend;
        struct {
            bool enabled;
            double p; // Kp K/K
            double i; // Ki 1/h
            double boost; // Kb K/K
        } roomComp;
        bool minSuspend;
    } config;
    struct PiCtrl {
        bool enabled; // enables PI controller
        bool init { false };
        double rspPrev; // previous room setpoint
        double integState {0}; // state of integrator / K
        double deltaT {0}; // result of PI controller
    } roomComp;
    bool roomSuspended {false};
    struct {
        double integState {0}; // state of integrator / K
        double reduction {0}; // result of PI controller
    } retLimit;
    bool minSuspended {false};
    HeatingCurve curve;
public:
    CHcontrol(const uint8_t channel);
    void setConfig(JsonObject &obj, const bool init);
    void getJson(JsonObject &obj);
    double getFlow();
    bool getChOn();
    double getFlowMax() const;
    bool roomCompEnabled() const;
    bool suspendEnabled() const;
    void loop();
    void loopRoomComp();
    void loopReturnLimit();
    void setMode(const ChannelControlMode mode);
    void setRoomCompEnabled(const bool en);
    bool getRoomCompEnabled() const;
    double flowTemp;
    double flowMin;
    ChannelOverride<bool> ovrdOn;
    ChannelOverride<double> ovrdTemp;
    ChannelControlMode mode {CTRLMODE_AUTO};
    static bool overrideEnabled; // set if otMode is master
};