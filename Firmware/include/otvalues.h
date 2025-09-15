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
    const char *STATUS_CH_ENABLE PROGMEM = "ch_enable";
    const char *STATUS_DHW_ENABLE PROGMEM = "dhw_enable";
    const char *STATUS_COOLING_ENABLE PROGMEM = "cooling_enable";
    const char *STATUS_OTC_ACTIVE PROGMEM = "otc_active";
    const char *STATUS_CH2_ENABLE PROGMEM = "ch2_enable";
    const Flag flags[5] PROGMEM = {
        {8, STATUS_CH_ENABLE},
        {9, STATUS_DHW_ENABLE},
        {10, STATUS_COOLING_ENABLE},
        {11, STATUS_OTC_ACTIVE},
        {12, STATUS_CH2_ENABLE}
    };
protected:
    bool sendDiscovery();
public:    
    OTValueMasterStatus();
};

class OTValueSlaveConfigMember: public OTValueFlags {
private:
    void getValue(JsonObject &obj) const;
    const char *DHW_PRESENT PROGMEM = "dhw_present";
    const char *CTRL_TYPE PROGMEM = "ctrl_type";
    const char *COOLONG_CFG PROGMEM = "cooling_config";
    const char *DHW_CONFIG PROGMEM = "dhw_config";
    const char *MASTER_LOW_PUMP PROGMEM = "master_lowoff_pumpctrl";
    const char *CH2_PRESENT PROGMEM = "ch2_present";
    const Flag flags[6] PROGMEM = {
        {8, DHW_PRESENT},
        {9, CTRL_TYPE},
        {10, COOLONG_CFG},
        {11, DHW_CONFIG},
        {12, MASTER_LOW_PUMP},
        {13, CH2_PRESENT}
    };
public:    
    OTValueSlaveConfigMember();
};

class OTValueFaultFlags: public OTValueFlags {
private:
    void getValue(JsonObject &obj) const;
    const char *SRV_REQUEST PROGMEM = "service_request";
    const char *LOCKOUT_RESET PROGMEM = "lockout_reset";
    const char *LOW_WATER_PRESSURE PROGMEM = "low_water_pressure";
    const char *GAS_FLAME_FAULT PROGMEM = "gas_flame_fault";
    const char *AIR_PRESS_FAULT PROGMEM = "air_pressure_fault";
    const char *WATER_OVER_TEMP PROGMEM = "water_over_temp";
    const Flag flags[6] PROGMEM = {
        {8, SRV_REQUEST},
        {9, LOCKOUT_RESET},
        {10, LOW_WATER_PRESSURE},
        {11, GAS_FLAME_FAULT},
        {12, AIR_PRESS_FAULT},
        {13, WATER_OVER_TEMP}
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

class OTValueMasterConfig: public OTValueFlags {
private:
    void getValue(JsonObject &obj) const;
protected:
    bool sendDiscovery();
public:    
    OTValueMasterConfig();
};

class OTValueRemoteParameter: public OTValueFlags {
private:
    const char *DHW_SETPOINT_RW PROGMEM = "dhw_setpoint_rw";
    const char *MAX_CH_SETPOINT_RW PROGMEM = "max_ch_setpoint_rw";
    const char *DHW_SETPOINT_TRANS PROGMEM = "dhw_setpoint_trans";
    const char *MAX_CH_SETPOINT_TRANS PROGMEM = "max_ch_setpoint_trans";
    const Flag flags[4] PROGMEM = {
        {0, DHW_SETPOINT_RW},
        {1, MAX_CH_SETPOINT_RW},
        {8, DHW_SETPOINT_TRANS},
        {9, MAX_CH_SETPOINT_TRANS}
    };
protected:
    bool sendDiscovery();
public:    
    OTValueRemoteParameter();
};


class OTValueRemoteOverrideFunction: public OTValueFlags {
private:
    const char *MANUAL_CHANGE_PRIORITY PROGMEM = "manual_change_priority";
    const char *PROGRAM_CHANGE_PRIORITY PROGMEM = "program_change_priority";
    const Flag flags[2] PROGMEM = {
        {0, MANUAL_CHANGE_PRIORITY},
        {1, PROGRAM_CHANGE_PRIORITY}
    };
protected:
    bool sendDiscovery();
public:
    OTValueRemoteOverrideFunction();
};


extern OTValue *boilerValues[24];
extern OTValue *thermostatValues[13];
extern const char* getOTname(OpenThermMessageID id);