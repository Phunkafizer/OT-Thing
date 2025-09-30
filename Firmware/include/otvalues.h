#pragma once

#include <ArduinoJson.h>
#include <OpenTherm.h>
#include "HADiscLocal.h"

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
    static OTValue* getSlaveValue(const OpenThermMessageID id);
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
        const char *discName;
        const char *haDevClass;
    };
    uint8_t numFlags;
    const Flag *flagTable;
    OTValueFlags(const OpenThermMessageID id, const int interval, const Flag *flagtable, const uint8_t numFlags);
    void getValue(JsonObject &obj) const;
    bool sendDiscFlag(String name, const char *field, const char *devClass);
    bool sendDiscovery();
};

class OTValueStatus: public OTValueFlags {
private:
    const Flag flags[7] PROGMEM = {
        {0, "fault",        "fault",        HA_DEVICE_CLASS_PROBLEM},
        {1, "ch_mode",      "heating",      HA_DEVICE_CLASS_RUNNING},
        {2, "dhw_mode",     "DHW",          HA_DEVICE_CLASS_RUNNING},
        {3, "flame",        "flame",        HA_DEVICE_CLASS_RUNNING},
        {4, "cooling",      "cooling",      HA_DEVICE_CLASS_RUNNING},
        {5, "ch2_mode",     "heating 2",    HA_DEVICE_CLASS_RUNNING},
        {6, "diagnostic",   "diagnostic",   HA_DEVICE_CLASS_PROBLEM}
    };
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

class OTValueVentStatus: public OTValueFlags {
private:
    const Flag flags[6] PROGMEM = {
        {0, "fault",        "fault",                HA_DEVICE_CLASS_PROBLEM},
        {1, "vent_active",  "Ventilation active",   HA_DEVICE_CLASS_RUNNING },
        {2, "bypass_open",  "Bypass open",          HA_DEVICE_CLASS_RUNNING},
        {3, "bypass_auto",  "Bypass auto",          HA_DEVICE_CLASS_RUNNING},
        {4, "free_vent",    "free ventilation",     HA_DEVICE_CLASS_RUNNING},
        {6, "diagnostic",   "diagnostic",           HA_DEVICE_CLASS_PROBLEM}
    };
public:    
    OTValueVentStatus();
};

class OTValueVentMasterStatus: public OTValueFlags {
private:
    const Flag flags[4] PROGMEM = {
        {8, "vent_enable"},
        {9, "open_bypass"},
        {10, "auto_bypass"},
        {11, "free_vent_enable"}
    };
protected:
    bool sendDiscovery();
public:    
    OTValueVentMasterStatus();
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
        {8, "service_request",      "service request",      HA_DEVICE_CLASS_PROBLEM},
        {9, "lockout_reset",        "lockout reset",        HA_DEVICE_CLASS_PROBLEM},
        {10, "low_water_pressure",  "low pressure",         HA_DEVICE_CLASS_PROBLEM},
        {11, "gas_flame_fault",     "flame fault",          HA_DEVICE_CLASS_PROBLEM},
        {12, "air_pressure_fault",  "air pressure fault",   HA_DEVICE_CLASS_PROBLEM},
        {13, "water_over_temp",     "water over temp",      HA_DEVICE_CLASS_PROBLEM}
    };
public:
    OTValueFaultFlags(const int interval);
};


class OTValueVentFaultFlags: public OTValueFlags {
private:
    void getValue(JsonObject &obj) const;
    const Flag flags[4] PROGMEM = {
        {8, "service_request",      "vent. service request",    HA_DEVICE_CLASS_PROBLEM},
        {9, "exhaust_fan_fault",    "exhaust fan fault",        HA_DEVICE_CLASS_PROBLEM},
        {10, "inlet_fan_fault",     "inlet fan fault",          HA_DEVICE_CLASS_PROBLEM},
        {11, "frost_protection",    "frost protection",         HA_DEVICE_CLASS_PROBLEM}
    };
public:
    OTValueVentFaultFlags(const int interval);
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


extern OTValue *slaveValues[39];
extern OTValue *thermostatValues[18];
extern const char* getOTname(OpenThermMessageID id);