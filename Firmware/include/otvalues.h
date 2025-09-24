#pragma once

#include <ArduinoJson.h>
#include <OpenTherm.h>

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

class OTValueFlags: public OTValue {
protected:
    struct Flag {
        uint8_t bit;
        const char *name;
    };
    OTValueFlags(const OpenThermMessageID id, const int interval, const Flag *flagtable, const uint8_t numFlags);
    void getValue(JsonObject &obj) const;
private:
    uint8_t numFlags;
    const Flag *flagTable;
};

class OTValueStatus: public OTValueFlags {
private:
    const char *STATUS_FLAME PROGMEM = "flame";
    const char *STATUS_DHW_MODE PROGMEM = "dhw_mode";
    const char *STATUS_CH_MODE PROGMEM = "ch_mode";
    const char *STATUS_FAULT PROGMEM = "fault";
    const char *STATUS_COOLING PROGMEM = "cooling";
    const char *STATUS_CH_MODE2 PROGMEM = "ch2_mode";
    const char *STATUS_DIAGNOSTIC PROGMEM = "diagnostic";
    const Flag flags[7] PROGMEM = {
        {0, STATUS_FAULT},
        {1, STATUS_CH_MODE},
        {2, STATUS_DHW_MODE},
        {3, STATUS_FLAME},
        {4, STATUS_COOLING},
        {5, STATUS_CH_MODE2},
        {6, STATUS_DIAGNOSTIC}
    };
protected:
    bool sendDiscovery();
public:    
    OTValueStatus();
};

class OTValueMasterStatus: public OTValueFlags {
private:
    const Flag flags[5] PROGMEM = {
        {8, "ch_enable"},
        {9, "dhw_enable"},
        {10, "cooling_enable"},
        {11, "otc_active"},
        {12, "ch2_enable"}
    };
protected:
    bool sendDiscovery();
public:    
    OTValueMasterStatus();
};

class OTValueSlaveConfigMember: public OTValueFlags {
private:
    void getValue(JsonObject &obj) const;
    const Flag flags[6] PROGMEM = {
        {8, "dhw_present"},
        {9, "ctrl_type"},
        {10, "cooling_config"},
        {11, "dhw_config"},
        {12, "master_lowoff_pumpctrl"},
        {13, "ch2_present"}
    };
public:    
    OTValueSlaveConfigMember();
};

class OTValueFaultFlags: public OTValueFlags {
private:
    void getValue(JsonObject &obj) const;
    const Flag flags[6] PROGMEM = {
        {8, "service_request"},
        {9, "lockout_reset"},
        {10, "low_water_pressure"},
        {11, "gas_flame_fault"},
        {12, "air_pressure_fault"},
        {13, "water_over_temp"}
    };
public:
    OTValueFaultFlags(const int interval);
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

class OTValueCHBounds: public OTValue {
private:
    void getValue(JsonObject &obj) const;
protected:
    bool sendDiscovery();
public:    
    OTValueCHBounds();
};

class OTValueMasterConfig: public OTValueFlags {
private:
    const Flag flags[1] PROGMEM = {
        {0, "smartPowerImplemented"},
    };
    void getValue(JsonObject &obj) const;
protected:
    bool sendDiscovery();
public:    
    OTValueMasterConfig();
};

class OTValueRemoteParameter: public OTValueFlags {
private:
    const Flag flags[4] PROGMEM = {
        {0, "dhw_setpoint_rw"},
        {1, "max_ch_setpoint_rw"},
        {8, "dhw_setpoint_trans"},
        {9, "max_ch_setpoint_trans"}
    };
protected:
    bool sendDiscovery();
public:    
    OTValueRemoteParameter();
};


class OTValueRemoteOverrideFunction: public OTValueFlags {
private:
    const Flag flags[2] PROGMEM = {
        {0, "manual_change_priority"},
        {1, "program_change_priority"}
    };
protected:
    bool sendDiscovery();
public:
    OTValueRemoteOverrideFunction();
};


class OTValueDayTime: public OTValue {
private:
    void getValue(JsonObject &obj) const;
protected:
    bool sendDiscovery();
public:    
    OTValueDayTime();
};

class OTValueDate: public OTValue {
private:
    void getValue(JsonObject &obj) const;
protected:
    bool sendDiscovery();
public:    
    OTValueDate();
};


extern OTValue *boilerValues[28];
extern OTValue *thermostatValues[16];
extern const char* getOTname(OpenThermMessageID id);