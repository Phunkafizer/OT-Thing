#pragma once

#include <ArduinoJson.h>
#include <OpenTherm.h>

struct OTItem {
    OpenThermMessageID id;
    const char* name;
    static const char* getName(OpenThermMessageID id);
};

class OTValue {
private:
    const OpenThermMessageID id;
    unsigned long lastTransfer;
    const int interval;
    virtual void getValue(JsonObject &stat) const = 0;
protected:
    uint16_t value;
    bool enabled;
    bool isSet;
    virtual bool sendDiscovery();
    const char* getName() const;
public:
    OTValue(const OpenThermMessageID id, const int interval);
    bool process();
    OpenThermMessageID getId() const;
    void setValue(uint16_t val);
    void setStatus(const OpenThermMessageType mt);
    void getJson(JsonObject &obj) const;
    void disable();
    void init(const bool enabled);
    void setTimeout();
    static OTValue* getBoilerValue(const OpenThermMessageID id);
    static OTValue* getThermostatValue(const OpenThermMessageID id);
    bool discFlag;
};

class OTValueu16: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:
    OTValueu16(const OpenThermMessageID id, const int interval);
    uint16_t getValue() const;
};

class OTValuei16: public OTValue {
private:
    void getValue(JsonObject &stat) const;
public:
    OTValuei16(const OpenThermMessageID id, const int interval);
    int16_t getValue() const;
};

class OTValueFloat: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:
    OTValueFloat(const OpenThermMessageID id, const int interval);
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

class OTValueMasterStatus: public OTValue {
private:
    void getValue(JsonObject &obj) const;
    const char *STATUS_CH_ENABLE PROGMEM = "ch_enable";
    const char *STATUS_DHW_ENABLE PROGMEM = "dhw_enable";
    const char *STATUS_COOLING_ENABLE PROGMEM = "cooling_enable";
    const char *STATUS_OTC_ACTIVE PROGMEM = "otc_active";
    const char *STATUS_CH2_ENABLE PROGMEM = "ch2_enable";
protected:
    bool sendDiscovery();
public:    
    OTValueMasterStatus();
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
    OTValueProductVersion(const OpenThermMessageID id, const int interval);
};

class OTValueCapacityModulation: public OTValue {
private:
    void getValue(JsonObject &obj) const;
protected:
    bool sendDiscovery();
public:    
    OTValueCapacityModulation();
};

class OTValueDHWBounds: public OTValue {
private:
    void getValue(JsonObject &obj) const;
protected:
    bool sendDiscovery();
public:    
    OTValueDHWBounds();
};

class OTValueMasterConfig: public OTValue {
private:
    void getValue(JsonObject &obj) const;
protected:
    bool sendDiscovery();
public:    
    OTValueMasterConfig();
};

extern OTItem OTITEMS[] PROGMEM;
extern OTValue *boilerValues[18];
extern OTValue *thermostatValues[10];