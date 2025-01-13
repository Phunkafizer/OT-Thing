#ifndef _otcontrol_h_
#define _otcontrol_h_

#include <OpenTherm.h>
#include "ArduinoJson.h"

class OTValue {
private:
    const OpenThermMessageID id;
    unsigned long lastTransfer;
    const int interval;
    virtual void getValue(JsonObject &stat) const = 0;
protected:
    const char *name;
    uint16_t value;
    bool enabled;
    bool isSet;
    virtual bool sendDiscovery();
public:
    OTValue(const char *name, const OpenThermMessageID id, const int interval);
    bool process();
    OpenThermMessageID getId() const;
    void setValue(uint16_t val);
    const char *getName() const;
    void setStatus(const OpenThermMessageType mt);
    void getJson(JsonObject &obj) const;
    void disable();
    void init(const bool enabled);
    void setTimeout();
    bool discFlag;
};

class OTValueu16: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:
    OTValueu16(const char *name, const OpenThermMessageID id, const int interval);
    uint16_t getValue() const;
};

class OTValuei16: public OTValue {
private:
    void getValue(JsonObject &stat) const;
public:
    OTValuei16(const char *name, const OpenThermMessageID id, const int interval);
    int16_t getValue() const;
};

class OTValueFloat: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:
    OTValueFloat(const char *name, const OpenThermMessageID id, const int interval);
    double getValue() const;
};

class OTValueStatus: public OTValue {
private:
    void getValue(JsonObject &obj) const;
    const char *STATUS_FLAME PROGMEM = "flame";
    const char *STATUS_DHW_MODE PROGMEM = "dhw_mode";
    const char *STATUS_CH_MODE PROGMEM = "ch_mode";
    const char *STATUS_FAULT PROGMEM = "fault";
protected:
    bool sendDiscovery();
public:    
    OTValueStatus();
};

class OTValueSlaveConfigMember: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:    
    OTValueSlaveConfigMember();
};

class OTValueProductVersion: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:    
    OTValueProductVersion(const char *name, const OpenThermMessageID id);
};

class OTValueCapacityModulation: public OTValue {
private:
    void getValue(JsonObject &obj) const;
protected:
    bool sendDiscovery();
public:    
    OTValueCapacityModulation();
};

class OTControl {
    friend void otCbSlave(unsigned long response, OpenThermResponseStatus status);
    friend void otCbMaster(unsigned long response, OpenThermResponseStatus status);
private:
    void OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status);
    void OnRxSlave(const unsigned long msg, const OpenThermResponseStatus status);
    unsigned long lastMillis;
    enum OTMode: int8_t {
        OTMODE_MASTER = 0,
        OTMODE_GATEWAY = 1,
        OTMODE_BYPASS = 2,
        OTMODE_LOOPBACKTEST = 3
    } otMode;
    unsigned long lastRxMaster;
    unsigned long lastRxSlave;
    struct HeatingParams {
        bool chOn;
        double roomSet;
        double flowMax;
        double exponent;
        double gradient;
        double offset;
    } heatingParams[2];
    double dhwTemp;
    bool dhwOn;
    bool discFlag;
public:
    OTControl();
    void setOTMode(const OTMode mode);
    void begin();
    void loop();
    void getJson(JsonObject &obj) const;
    void setChCtrlConfig(JsonObject &config);
    void resetDiscovery();
    void setDhwTemp(double temp);
};

extern OTControl otcontrol;
#endif