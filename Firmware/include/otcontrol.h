#pragma once

#include <OpenTherm.h>
#include "ArduinoJson.h"
#include "freertos/FreeRTOS.h"

class OTWriteRequest {
private:
    uint32_t nextMillis {0};
    uint16_t interval;
protected:
    OpenThermMessageID id;
public:
    OTWriteRequest(OpenThermMessageID id, uint16_t intervalS);
    void send(const uint16_t data);
    void sendTemp(const double temp);
    void force();
    operator bool();
};

class OTWRSetDhw: public OTWriteRequest {
public:
    OTWRSetDhw();
};

class OTWRSetBoilerTemp: public OTWriteRequest {
public:
    OTWRSetBoilerTemp(const uint8_t ch);
};

class OTWRMasterConfigMember: public OTWriteRequest {
public:
    OTWRMasterConfigMember();
};

class OTWRSetVentSetpoint: public OTWriteRequest {
public:
    OTWRSetVentSetpoint();
};

class OTWRSetRoomTemp: public OTWriteRequest {
public:
    OTWRSetRoomTemp(const uint8_t ch);
};

class OTWRSetRoomSetPoint: public OTWriteRequest {
public:
    OTWRSetRoomSetPoint(const uint8_t ch);
};

class OTWRSetOutsideTemp: public OTWriteRequest {
public:
    OTWRSetOutsideTemp();
};

class OTControl {
friend OTWriteRequest;
public:
    enum CtrlMode: int8_t {
        CTRLMODE_UNKNOWN = -1,
        CTRLMODE_OFF = 0,
        CTRLMODE_ON = 1,
        CTRLMODE_AUTO = 2
    };
    friend void otCbSlave(unsigned long response, OpenThermResponseStatus status);
    friend void otCbMaster(unsigned long response, OpenThermResponseStatus status);
    friend void IRAM_ATTR handleIrqMaster();
    friend void IRAM_ATTR handleIrqSlave();
    friend void IRAM_ATTR handleTimerIrqMaster();
    friend void IRAM_ATTR handleTimerIrqSlave();
    friend class OTValue;
private:
    void OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status);
    void OnRxSlave(const unsigned long msg, const OpenThermResponseStatus status);
    bool setThermostatVal(const unsigned long msg);
    void sendRequest(const char source, const unsigned long msg);
    void masterPinIrq();
    void slavePinIrq();
    double getFlow(const uint8_t channel);
    uint16_t tmpToData(const double tmpf);
    unsigned long lastBoilerStatus;
    unsigned long lastVentStatus;
    enum OTMode: int8_t {
        OTMODE_BYPASS = 0,
        OTMODE_MASTER = 1,
        OTMODE_REPEATER = 2,
        OTMODE_LOOPBACKTEST = 4
    } otMode;
    void setOTMode(const OTMode mode, const bool enableSlave = false);
    enum SlaveApplication: uint8_t {
        SLAVEAPP_HEATCOOL = 0,
        SLAVEAPP_VENT = 1,
        SLAVEAPP_SOLAR = 2
    } slaveApp;
    struct HeatingConfig {
        bool chOn;
        double roomSet; // default room set point
        double flowMax;
        double exponent;
        double gradient;
        double offset;
        double flow; // default flow temperature
        double roomTempComp; // P K/K
        double roomTempCompI; // I 1/h
        bool enableHyst;
        double hysteresis;
    } heatingConfig[2];
    struct HeatingControl {
        bool chOn;
        double flowTemp;
        CtrlMode mode {CTRLMODE_AUTO};
        bool overrideFlow;
        struct PiCtrl {
            bool init { false };
            double roomTempFilt;
            double integState {0}; // state of integrator / K
            double deltaT {0};
        } piCtrl;
        bool suspended {false};
    } heatingCtrl[2];
    void loopPiCtrl();
    unsigned long nextPiCtrl { 0 };
    struct {
        bool ventEnable;
        bool openBypass;
        bool autoBypass;
        bool freeVentEnable;
        uint8_t setpoint;
    } ventCtrl;
    struct {
        bool coolOn;
        bool otc;
        bool summerMode;
        bool dhwBlocking;
    } boilerConfig;
    struct {
        bool dhwOn;
        double dhwTemp;
        bool overrideDhw;
    } boilerCtrl;
    bool discFlag {true};
    OTWRSetDhw setDhwRequest;
    OTWRSetBoilerTemp setBoilerRequest[2];
    OTWRMasterConfigMember setMasterConfigMember;
    OTWRSetVentSetpoint setVentSetpointRequest;
    OTWRSetRoomTemp setRoomTemp[2];
    OTWRSetRoomSetPoint setRoomSetPoint[2];
    OTWRSetOutsideTemp setOutsideTemp;
    uint8_t masterMemberId;
    struct OTInterface {
        OTInterface(const uint8_t inPin, const uint8_t outPin, const bool isSlave);
        OpenTherm hal;
        uint32_t txCount;
        uint32_t rxCount;
        uint32_t timeoutCount;
        uint32_t invalidCount;
        unsigned long lastRx; // millis
        unsigned long lastTx; // millis
        unsigned long lastTxMsg;
        SemaphoreHandle_t mutex;
        void sendRequest(const char source, const unsigned long msg);
        void resetCounters();
        void onReceive(const char source, const unsigned long msg);
        void sendResponse(const unsigned long msg, const char source = 0);
    } master, slave;
    bool slaveEnabled {false};
    uint16_t statusReqOvl {0}; // will be or'ed to status request as this is needed by some boilers
public:
    OTControl();
    void begin();
    void loop();
    unsigned long slaveRequest(OpenThermMessageID id, OpenThermMessageType ty, uint16_t data);
    void getJson(JsonObject &obj);
    void setConfig(JsonObject &config);
    void setDhwTemp(const double temp);
    void setChTemp(const double temp, const uint8_t channel);
    void setChCtrlMode(const CtrlMode mode, const uint8_t channel);
    void setDhwCtrlMode(const CtrlMode mode);
    bool sendDiscovery();
    bool sendCapDiscoveries();
    void forceFlowCalc(const uint8_t channel);
    void setVentSetpoint(const uint8_t v);
    void setVentEnable(const bool en);
    void setOverrideCh(const bool ovrd, const uint8_t channel);
    void setOverrideDhw(const bool ovrd);
};


extern OTControl otcontrol;