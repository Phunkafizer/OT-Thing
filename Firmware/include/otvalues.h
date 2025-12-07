#pragma once

#include <ArduinoJson.h>
#include <OpenTherm.h>
#include "HADiscLocal.h"


/*
   Class 1 : Control and Status Information
ID	Msg SV  TV  LV  Name
0	R-	*   *   *   Master status
1	W	-   *   -   Control Setpoint (Tset)
5	R	*   -   *   ASF-flags / OEM-fault-code
8	-W	-   *   -   Control Setpoint 2 (TsetCH2)
70	R-	*   *   *   Master status ventilation/heat-recovery
71	W	-   *   -   Vset / Relative ventilation position
72  R-  *   -   *   ASF-flags / OEM-fault-code (ventilation/heat-recovery)
73	R-	OEM diagnostic code ventilation/heat-recovery
101	R-	Solar Storage status
102	R-	Solar Storage specific fault flags
115	R-	OEM diagnostic code


   Class 2 : Configuration Information
ID	Msg	Name
2	-W	-   *   -   Master configuration
3	R-	*   -   *   Slave configuration
74	R-	Configuration ventilation/heat-recovery
75	R-	*   -   *   OpenTherm version ventilation/heat-recovery
76  R-  *   -   *   Ventilation / heat-recovery product version number and type
93	R-	Brand index
94	R-	Brand version index
95	R-	Brand serial number index
103	R-	Solar Storage configuration
104\tR-\tSolar Storage product version number and type
124	-W	-   *   -   OpenTherm version Master
125	R-	*   -   *   OpenTherm version Slave
126	-W	-   *   -   Master product version number and type
127 R-  *   -   *   Slave product version number and type

   Class 3 : Remote Request
ID	Msg	Name
4	-W	Remote Request

   Class 4 : Sensor and Informational Data
ID	Msg	Name
16	-W	-   *   -   Room Setpoint
17	R-	*   -   *   Relative Modulation Level
18	R-	*   -   *   CH water pressure
19	R-	*   -   *   DHW flow rate
20	RW	-   *   -   Day of Week & Time of Day
21	RW	-   *   -   Date
22	RW	-   *   -   Year
23	-W	-   *   -   Room Setpoint CH2
24	-W	-   *   -   Room temperature
25	R-	*   -   *   Boiler water temp.
26	R-	*   -   *   DHW temperature
27	RW	*   -   *   Outside temperature
28	R-	*   -   *   Return water temperature
29	R-	Solar storage temperature
30	R-	Solar collector temperature
31	R-	*   -   *   Flow temperature CH2
32  R-  *   -   *   DHW2 temperature
33  R-  *   -   *   Exhaust temperature
34  R-  *   -   *   Boiler heat exchanger temperature
35  R-  *   -   *   Boiler fan speed (setpoint/actual)
36  R-  *   -   *   Flame current
37  -W  -   *   -   TrCH2 (room temp CH2)
38\tRW\tRelative Humidity
77  R-  *   -   *   Relative ventilation
78  RW  *   -   *   Relative humidity (exhaust)
79	RW	*   -   *   CO2 level
80	R-	*   -   *   Supply inlet temperature
81	R-	*   -   *   Supply outlet temperature
82	R-	*   -   *   Exhaust inlet temperature
83	R-	*   -   *   Exhaust outlet temperature
84	R-	*   -   *   Actual exhaust fan speed
85	R-	*   -   *   Actual inlet fan speed
96	RW	Cooling Operation hours
97	RW	Power Cycles
98	-W	Type of sensor
109	RW	Electricity producer starts
110	RW	Electricity producer hours
111	R-	Electricity production
112	RW	Cumulative Electricity production
113	RW	*   -   *   Number of un-successful burner starts
114	RW	*   -   *   Number of times flame signal was too low
116	RW	*   -   *   Successful Burner starts
117	RW	*   -   *   CH pump starts
118	RW	DHW pump/valve starts
119	RW	DHW burner starts
120	RW	*   -   *   Burner operation hours
121	RW	CH pump operation hours
122	RW	DHW pump/valve operation hours
123	RW	*   -   *   DHW burner operation hours

   Class 5 : Remote Boiler Parameters
ID	Msg	Name
6	R-	*   -   *   Remote-parameter transfer-enable flags

48	R-	*   -   *   DHW setpoint upper/lower bound
49	R-	*   -   *   max CH setpoint upper/lower bound
56	RW	-   *   -   DHW Setpoint
57	RW	max CH water Setpoint
86	R-	Remote-parameter transfer-enable flags ventilation/heat-recovery
87	RW	Nominal ventilation value

   Class 6 : Transparent Slave Parameters
ID	Msg	Name
10	R-	Number of Transparent Slave Parameters
11	RW	TSP index/value
88	R-	Number of TSPs ventilation/heat-recovery
89	RW	TSP index/value ventilation/heat-recovery
105	R-	Number of TSPs Solar Storage
106	RW	TSP index/value Solar Storage

   Class 7 : Fault History Data
ID	Msg	Name
12	R-	Size of Fault Buffer
13	R-	FHB-entry index/value
90	R-	Size of Fault Buffer ventilation/heat-recovery
91	R-	FHB-entry index/value ventilation/heat-recovery
107	R-	Size of Fault Buffer Solar Storage
108	R-	FHB-entry index/value Solar Storage

   Class 8 : Control of Special Applications
ID	Msg	Name
7	-W	Cooling control signal
9	R-	*   -   *   Remote Override Room Setpoint
14	-W	-   *   -   Maximum relative modulation level setting
15	R-	*   -   *   Maximum boiler capacity & Minimum modulation level
39  R-  *   -   *   Remote Override Room Setpoint 2
99	RW	Remote Override Operating Mode Heating
100	R-	*   -   *   Remote Override Room Setpoint function
*/

class OTValue {
private:
    const OpenThermMessageID id;
    unsigned long lastTransfer;
    const int interval;
    virtual void getValue(JsonObject &stat) const = 0;
protected:
    uint16_t value;
    bool enabled;
    virtual bool sendDiscovery();
    bool sendDiscovery(String field, const bool addBaseName = false);
    const char* getName() const;
    bool discFlag;
public:
    OTValue(const OpenThermMessageID id, const int interval);
    bool process();
    OpenThermMessageID getId() const;
    void setValue(uint16_t val);
    uint16_t getValue();
    void setStatus(const OpenThermMessageType mt);
    void getJson(JsonObject &obj) const;
    void disable();
    void init(const bool enabled);
    void setTimeout();
    static OTValue* getSlaveValue(const OpenThermMessageID id);
    static OTValue* getThermostatValue(const OpenThermMessageID id);
    void refreshDisc();
    
    bool isSet;
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


class OTValueFloatTemp: public OTValueFloat {
private:
    bool sendDiscovery();
    const char *haName;
public:
    OTValueFloatTemp(const OpenThermMessageID id, const char *haName);
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
    bool slave;
    OTValueFlags(const OpenThermMessageID id, const int interval, const Flag *flagtable, const uint8_t numFlags, const bool slave);
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
        {8, "ch_enable",    "CH enable",    nullptr},
        {9, "dhw_enable",   "DHW enable",   nullptr},
        {10, "cooling_enable", "cooling enable", nullptr},
        {11, "otc_active",  "OTC active",   nullptr},
        {12, "ch2_enable",  "CH2 enable",   nullptr}
    };
public:    
    OTValueMasterStatus();
};

class OTValueVentStatus: public OTValueFlags {
private:
    const Flag flags[6] PROGMEM = {
        {0, "fault",        "fault",                HA_DEVICE_CLASS_PROBLEM},
        {1, "vent_active",  "Ventilation active",   HA_DEVICE_CLASS_RUNNING },
        {2, "bypass_open",  "Bypass open",          HA_DEVICE_CLASS_OPEN},
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
        {8, "dhw_present",              "DHW presemt",              nullptr},
        {9, "ctrl_type",                "Control type on/off",      nullptr},
        {10, "cooling_config",          "Cooling supported",        nullptr},
        {11, "dhw_config",              "DHW storage",              nullptr},
        {12, "master_lowoff_pumpctrl",  "Master pump ctrl allowed", nullptr},
        {13, "ch2_present",             "CH2 present",              nullptr}
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
    bool sendDiscovery();
    const char *haName;
public:    
    OTValueProductVersion(const OpenThermMessageID id, const int interval, const char *haName);
};

class OTValueCapacityModulation: public OTValue {
private:
    void getValue(JsonObject &obj) const;
    const char *MAX_CAPACITY = "max_capacity" PROGMEM;
    const char *MIN_MODULATION = "min_modulation" PROGMEM;
protected:
    bool sendDiscovery();
public:    
    OTValueCapacityModulation();
};

class OTValueDHWBounds: public OTValue {
private:
    void getValue(JsonObject &obj) const;
    const char *DHW_MAX = "dhwMax" PROGMEM;
    const char *DHW_MIN = "dhwMin" PROGMEM;
protected:
    bool sendDiscovery();
public:    
    OTValueDHWBounds();
};

class OTValueCHBounds: public OTValue {
private:
    void getValue(JsonObject &obj) const;
    const char *CH_MAX = "chMax" PROGMEM;
    const char *CH_MIN = "chMin" PROGMEM;
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
        {0, "dhw_setpoint_rw", "DHW setpoint write", nullptr},
        {1, "max_ch_setpoint_rw", "Max. CH setpoint write", nullptr},
        {8, "dhw_setpoint_trans", "DHW setpoint transfer", nullptr},
        {9, "max_ch_setpoint_trans", "Max. CH setpoint transfer", nullptr}
    };
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


class OTValueHeatExchangerTemp: public OTValueFloat {
protected:
    bool sendDiscovery();
public:
    OTValueHeatExchangerTemp();
};


class OTValueBoilerFanSpeed: public OTValue {
private:
    void getValue(JsonObject &obj) const;
    const char *SETPOINT = "setpoint" PROGMEM;
    const char *ACTUAL = "actual" PROGMEM;
protected:
    bool sendDiscovery();
public:
    OTValueBoilerFanSpeed();
};


class OTValueFlameCurrent: public OTValueFloat {
protected:
    bool sendDiscovery();
public:
    OTValueFlameCurrent();
};


extern OTValue *slaveValues[45];
extern OTValue *thermostatValues[17];
extern const char* getOTname(OpenThermMessageID id);