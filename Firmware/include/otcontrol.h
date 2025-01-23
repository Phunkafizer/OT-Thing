#pragma once

#include <OpenTherm.h>
#include "ArduinoJson.h"

class OTControl {
public:
    enum CtrlMode: uint8_t {
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
    unsigned long lastMillis;
    enum OTMode: int8_t {
        OTMODE_BYPASS = 0,
        OTMODE_MASTER = 1,
        OTMODE_GATEWAY = 2,
        OTMODE_REPEATER = 3,
        OTMODE_LOOPBACKTEST = 4
    } otMode;
    struct HeatingParams {
        bool chOn;
        double roomSet;
        double flowMax;
        double exponent;
        double gradient;
        double offset;
        double flow;
        double flowDefault;
        bool override;
        CtrlMode ctrlMode;
    } heatingParams[2];
    double dhwTemp;
    bool dhwOn;
    bool discFlag;
    uint32_t nextDHWSet;
    uint32_t nextBoilerTemp;
    struct OTInterface {
        OTInterface(const uint8_t inPin, const uint8_t outPin, const bool isSlave);
        OpenTherm hal;
        uint32_t txCount;
        uint32_t rxCount;
        unsigned long lastRx;
        unsigned long lastTx;
        void sendRequest(const char source, const unsigned long msg);
        void resetCounters();
        void onReceive(const char source, const unsigned long msg);
        void sendResponse(const unsigned long msg);
    } master, slave;
public:
    OTControl();
    void setOTMode(const OTMode mode);
    void begin();
    void loop();
    void getJson(JsonObject &obj);
    void setChCtrlConfig(JsonObject &config);
    void resetDiscovery();
    void setDhwTemp(const double temp);
    void setChTemp(const double temp);
    void setChCtrlMode(const CtrlMode mode);
};

extern OTControl otcontrol;