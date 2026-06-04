#pragma once

#include "ArduinoJson.h"
#include "heatingcurve.h"
#include "sensors.h"
#include "HADiscLocal.h"
#include <vector>

template <typename T1>
class ChannelOverride {
public:
    bool active; // overriding activated
    T1 value; // overriden value
};

class CHcontrol {
private:
    bool roomCompEnabled() const;
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
        HADiscovery::ClimateMode mode {HADiscovery::MODE_AUTO};
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
    bool outSuspended {false};
    HeatingCurve curve;
    struct SchedulerEntry{
        uint8_t days;
        uint16_t time; // minutes after midnight
        double temp;
    };
    std::vector<SchedulerEntry> schedule;
    int8_t lastSchudleIdx { -1 };
    bool scheduleActive {false};
    int8_t getCurrentScheduleIdx() const;
public:
    CHcontrol(const uint8_t channel);
    void setConfig(JsonObject &obj, const bool init);
    void getJson(JsonObject &obj);
    double getFlow();
    bool getChOn();
    double getFlowMax() const;
    bool suspendEnabled() const;
    void loop();
    void loopRoomComp();
    void loopReturnLimit();
    void setMode(const HADiscovery::ClimateMode mode);
    void setRoomComp(const HADiscovery::ClimateMode mode);
    double flowTemp;
    double flowMin;
    ChannelOverride<bool> ovrdOn;
    ChannelOverride<double> ovrdTemp;
    HADiscovery::ClimateMode mode {HADiscovery::MODE_AUTO};
    static bool overrideEnabled; // set if otMode is master && enableSlave
};