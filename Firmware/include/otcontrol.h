#pragma once

#include <OpenTherm.h>
#include <map>
#include "ArduinoJson.h"
#include "util.h"
#include "masterrequests.h"
#include "CHcontrol.h"
#include "dhwControl.h"
#include "vent.h"
#include "flamestats.h"

const uint8_t NUM_HEATCIRCUITS = 2;

struct SlaveRequestStruct {
    OpenThermMessageID idReq;
    OpenThermMessageType typeReq;
    uint16_t dataReq;
    OpenThermMessageType typeResp;
    uint16_t dataResp;
};

extern struct OTTestItem {
    OpenThermMessageID id;
    uint16_t value;
} loopbackTestData[51];

class OTControl {
friend class BrandInfo;
friend class SemMaster;
public:
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
    void masterPinIrq();
    void slavePinIrq();
    uint16_t tmpToData(const double tmpf);
    void hwYield();
    unsigned long buildBrandResponse(const OpenThermMessageID id, const String &str, const uint8_t idx);
    bool sendChDiscoveries(const uint8_t ch, const bool en);
    unsigned long lastBoilerStatus;
    enum OTMode: int8_t {
        OTMODE_BYPASS = 0,
        OTMODE_MASTER = 1,
        OTMODE_REPEATER = 2,
        OTMODE_LOOPBACKTEST = 4
    } otMode;
    void setOTMode(const OTMode mode);
    enum SlaveApplication: uint8_t {
        SLAVEAPP_HEATCOOL = 0,
        SLAVEAPP_VENT = 1,
        SLAVEAPP_SOLAR = 2
    } slaveApp;
    CHcontrol chcontrol[NUM_HEATCIRCUITS];
    void loopRoomComp(const uint8_t ch);
    void loopRetLimit(const uint8_t ch);
    unsigned long nextPiCtrl { 0 };
    struct {
        bool otc;
        double chOffTemp;
    } boilerConfig;
    struct {
        uint8_t maxModulation;
        bool summerMode;
        bool dhwBlocking;
        bool coolOn;
        uint8_t coolingCtrl;
    } boilerCtrl;
    FlameStats flameStats;
    bool discFlag {true};
    OTWRSetBoilerTemp setBoilerRequest[NUM_HEATCIRCUITS];
    OTWRMasterConfigMember setMasterConfigMember;
    OTWRSetRoomTemp setRoomTemp[NUM_HEATCIRCUITS];
    OTWRSetRoomSetPoint setRoomSetPoint[NUM_HEATCIRCUITS];
    OTWRSetOutsideTemp setOutsideTemp;
    OTWRSetMaxModulation setMaxModulation;
    OTWRProdVersion setProdVersion;
    OTWRSetOTVersion setOTVersion;
    OTWRSetMaxCh setMaxCh;
    OTWRSetCoolingCtrl setCoolingCtrlSetpoint;
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
    bool enableSlave {false};
    bool bypass {false};
    uint16_t statusReqOvl {0}; // will be or'ed to status request as this is needed by some boilers
    bool init {false};
    bool noDhwSet;
public:
    std::map<OpenThermMessageID, uint16_t> masterTestValues;
    DHWControl dhwControl;
    VentControl ventCtrl;
    OTControl();
    void begin();
    void loop();
    bool slaveRequest(SlaveRequestStruct &srs);
    void sendRequest(const char source, const unsigned long msg);
    void getJson(JsonObject &obj);
    void setConfig(JsonObject &config);
    void setChTemp(const double temp, const uint8_t channel);
    void setChCtrlMode(const HADiscovery::ClimateMode mode, const uint8_t channel);
    void setDhwCtrlMode(const HADiscovery::ClimateMode mode);
    void setCoolingMode(const bool on);
    void setCoolingCtrl(const int ctrl);
    bool sendDiscovery();
    bool sendCapDiscoveries();
    void forceFlowCalc(const uint8_t channel);
    void setOverrideChOn(const bool ovrd, const uint8_t channel);
    void setOverrideChFlow(const bool ovrd, const uint8_t channel);
    void setMaxMod(const int mm);
    void setRoomMode(const HADiscovery::ClimateMode mode, const uint8_t channel);
    void setFlowMin(const double flowMin, const uint8_t channel);
    void setBypass(const bool bypass);
    void setSummerMode(const bool summerMode);
    void setDhwBlocking(const bool dhwBlocking);
    bool getFlame() const;
};

extern OTControl otcontrol;