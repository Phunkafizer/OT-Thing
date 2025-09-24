#pragma once

#include <OpenTherm.h>
#include "ArduinoJson.h"

class OTWriteRequest {
private:
    uint32_t nextMillis {0};
    uint16_t interval;
protected:
    OpenThermMessageID id;
public:
    OTWriteRequest(OpenThermMessageID id, uint16_t intervalS);
    void send(const uint16_t data);
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
    unsigned long lastMillis;
    enum OTMode: int8_t {
        OTMODE_BYPASS = 0,
        OTMODE_MASTER = 1,
        OTMODE_REPEATER = 2,
        OTMODE_LOOPBACKTEST = 4
    } otMode;
    struct HeatingParams {
        bool chOn;
        double roomSet; // default room set point
        double flowMax;
        double exponent;
        double gradient;
        double offset;
        double flow;
        double flowDefault;
        bool overrideFlow;
        CtrlMode ctrlMode;
    } heatingParams[2];
    double dhwTemp;
    bool dhwOn;
    bool overrideDhw;
    bool discFlag {true};
    OTWRSetDhw setDhwRequest;
    OTWRSetBoilerTemp setBoilerRequest[2];
    OTWRMasterConfigMember setMasterConfigMember;
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
        void sendRequest(const char source, const unsigned long msg);
        void resetCounters();
        void onReceive(const char source, const unsigned long msg);
        void sendResponse(const unsigned long msg, const char source = 0);
    } master, slave;
public:
    OTControl();
    void setOTMode(const OTMode mode);
    void begin();
    void loop();
    void getJson(JsonObject &obj);
    void setConfig(JsonObject &config);
    void setDhwTemp(const double temp);
    void setChTemp(const double temp, const uint8_t channel);
    void setChCtrlMode(const CtrlMode mode, const uint8_t channel);
    void setDhwCtrlMode(const CtrlMode mode);
    bool sendDiscovery();
    void forceFlowCalc(const uint8_t channel);
};


extern OTControl otcontrol;