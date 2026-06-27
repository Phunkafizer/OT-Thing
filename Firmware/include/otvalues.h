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
115	R-	*   -   *   OEM diagnostic code


   Class 2 : Configuration Information
ID	Msg	Name
2	-W	-   *   -   Master configuration
3	R-	*   -   *   Slave configuration
74	R-	Configuration ventilation/heat-recovery
75	R-	*   -   *   OpenTherm version ventilation/heat-recovery
76  R-  *   -   *   Ventilation / heat-recovery product version number and type
93	R-	*   *   *   Brand index
94	R-	*   *   *   Brand version index
95	R-	*   *   *   Brand serial number index
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
97	RW	*   -   *   Power Cycles
98	-W	Type of sensor
109	RW	Electricity producer starts
110	RW	Electricity producer hours
111	R-	Electricity production
112	RW	Cumulative Electricity production
113	RW	*   -   *   Number of un-successful burner starts
114	RW	*   -   *   Number of times flame signal was too low
116	RW	*   -   *   Successful Burner starts
117	RW	*   -   *   CH pump starts
118	RW	*   -   *   DHW pump/valve starts
119	RW	*   -   *   DHW burner starts
120	RW	*   -   *   Burner operation hours
121	RW	*   -   *   CH pump operation hours
122	RW	*   -   *   DHW pump/valve operation hours
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
10	R-	*   -   -   Number of Transparent Slave Parameters
11	RW	TSP index/value
88	R-	Number of TSPs ventilation/heat-recovery
89	RW	TSP index/value ventilation/heat-recovery
105	R-	Number of TSPs Solar Storage
106	RW	TSP index/value Solar Storage

   Class 7 : Fault History Data
ID	Msg	Name
12	R-	*   -   -   Size of Fault Buffer
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
    unsigned long lastTransfer;
    const int interval;
    virtual void getValue(JsonVariant var) const = 0;
protected:
    virtual bool sendDiscovery();
    bool sendDiscovery(String field);
    PGM_P getName() const;
    const OpenThermMessageID id;
    uint16_t value;
    bool enabled;
    bool discFlag;
    bool setFlag;
    uint32_t numSet;
    OpenThermMessageType lastMsgType;
    PGM_P haName;
    PGM_P entityCategory;
public:
    OTValue(const OpenThermMessageID id, const int interval, PGM_P haName = nullptr);
    virtual ~OTValue() = default;
    virtual bool process();
    OpenThermMessageID getId() const;
    virtual void setValue(const OpenThermMessageType ty, const uint16_t val);
    void setMsgType(const OpenThermMessageType ty);
    uint16_t getValue();
    void setStatus(const OpenThermMessageType mt);
    void getJson(JsonObject &obj, const bool addResult = false) const;
    void getStatus(JsonObject &obj) const;
    virtual void init(const bool enabled);
    void setTimeout();
    static OTValue* getSlaveValue(const OpenThermMessageID id);
    static OTValue* getMasterValue(const OpenThermMessageID id);
    static class OTValueSlaveConfigMember* getSlaveConfig();
    static void setTexhaustAsFloat(bool asFloat);
    void refreshDisc();
    bool isSet() const;
    bool hasReply() const;
    OpenThermMessageType getLastMsgType() const;
};

class OTValueu16: public OTValue {
private:
    void getValue(JsonVariant var) const override;
public:
    OTValueu16(const OpenThermMessageID id, const int interval, PGM_P haName = nullptr);
};

class OTValueOperatingHours: public OTValueu16 {
private:
    bool sendDiscovery() override;
public:
    OTValueOperatingHours(const OpenThermMessageID id, PGM_P haName);
};

class OTValuei16: public OTValue {
private:
    void getValue(JsonVariant var) const override;
public:
    OTValuei16(const OpenThermMessageID id, const int interval, PGM_P haName);
};


class OTValueBufSize: public OTValue {
private:
    void getValue(JsonVariant var) const override;
public:
    OTValueBufSize(const OpenThermMessageID id);
};

class OTValueFloat: public OTValue {
private:
    void getValue(JsonVariant var) const override;
public:
    OTValueFloat(const OpenThermMessageID id, const int interval, PGM_P haName=nullptr);
};


class OTValueFloatTemp: public OTValueFloat {
private:
    bool sendDiscovery() override;
public:
    OTValueFloatTemp(const OpenThermMessageID id, PGM_P haName);
};

class OTValueFlags: public OTValue {
protected:
    struct Flag {
        uint8_t bit {0};
        const char *field {nullptr};
        const char *discName {nullptr};
        const char *haDevClass {nullptr};
    };
    uint8_t numFlags;
    const Flag *flagTable;
    bool slave;
    OTValueFlags(const OpenThermMessageID id, const int interval, const Flag *flagtable, const uint8_t numFlags, const bool slave);
    void getValue(JsonVariant var) const override;
    bool sendDiscFlag(const Flag *flag, const bool enb);
    bool sendDiscovery() override;
};

class OTValueStatus: public OTValueFlags {
public:
    enum {
        BIT_FAULT = 0,
        BIT_CH_MODE = 1,
        BIT_DHW_MODE = 2,
        BIT_FLAME = 3,
        BIT_COOLING = 4,
        BIT_CH2_MODE = 5,
        BIT_DIAGNOSTIC = 6
    };
    OTValueStatus();
    bool getChActive(const uint8_t channel) const;
    bool getFlame() const;
    bool getDhwActive() const;
private:
    const char *CH2_MODE PROGMEM = "ch2_mode";
    const char *DHW_MODE PROGMEM = "dhw_mode";
    const char *COOLING PROGMEM = "cooling";
    const char *DIAGNOSTIC PROGMEM = "diagnostic";
    const char *FAULT PROGMEM = "fault";
    const Flag flags[7] PROGMEM = {
        {BIT_FAULT,         FAULT,          FAULT,          HA_DEVICE_CLASS_PROBLEM},
        {BIT_CH_MODE,       "ch_mode",      "heating",      HA_DEVICE_CLASS_RUNNING},
        {BIT_DHW_MODE,      DHW_MODE,       "DHW",          HA_DEVICE_CLASS_RUNNING},
        {BIT_FLAME,         "flame",        "flame",        HA_DEVICE_CLASS_RUNNING},
        {BIT_COOLING,       COOLING,        COOLING,        HA_DEVICE_CLASS_RUNNING},
        {BIT_CH2_MODE,      CH2_MODE,       "heating 2",    HA_DEVICE_CLASS_RUNNING},
        {BIT_DIAGNOSTIC,    DIAGNOSTIC,     DIAGNOSTIC,     HA_DEVICE_CLASS_PROBLEM}
    };
protected:
    void getValue(JsonVariant var) const override;
    bool sendDiscovery() override;
};

class OTValueMasterStatus: public OTValueFlags {
public:    
    enum {
        BIT_CH_ENABLE = 8,
        BIT_DHW_ENABLE = 9,
        BIT_COOLING_ENABLE = 10,
        BIT_OTC_ACTIVE = 11,
        BIT_CH2_ENABLE = 12,
        BIT_SUMMER_MODE = 13,
        BIT_DHW_BLOCKING = 14
    };
    OTValueMasterStatus();
private:
    const char *DHW_ENABLE PROGMEM = "dhw_enable";
    const char *CH2_ENABLE PROGMEM = "ch2_enable";
    const char *COOLING_ENABLE PROGMEM = "cooling_enable";
    const Flag flags[7] PROGMEM = {
        {BIT_CH_ENABLE,         "ch_enable",        "CH enable",        nullptr},
        {BIT_DHW_ENABLE,        DHW_ENABLE,         "DHW enable",       nullptr},
        {BIT_COOLING_ENABLE,    COOLING_ENABLE,     "cooling enable",   nullptr},
        {BIT_OTC_ACTIVE,        "otc_active",       "OTC active",       nullptr},
        {BIT_CH2_ENABLE,        CH2_ENABLE,         "CH2 enable",       nullptr},
        {BIT_SUMMER_MODE,       "summer_mode",      "summer mode",      nullptr},
        {BIT_DHW_BLOCKING,      "dhw_blocking",     "DHW blocking",     nullptr}
    };
protected:
    void getValue(JsonVariant var) const override;
    bool sendDiscovery() override;
};

class OTValueVentStatus: public OTValueFlags {
private:
    const Flag flags[6] PROGMEM = {
        {0, "fault",        "fault",                HA_DEVICE_CLASS_PROBLEM},
        {1, "vent_active",  "Ventilation active",   HA_DEVICE_CLASS_RUNNING },
        {2, "bypass_open",  "Bypass open",          HA_DEVICE_CLASS_OPENING},
        {3, "bypass_auto",  "Bypass auto",          HA_DEVICE_CLASS_RUNNING},
        {4, "free_vent",    "free ventilation",     HA_DEVICE_CLASS_RUNNING},
        {6, "diagnostic",   "diagnostic",           HA_DEVICE_CLASS_PROBLEM}
    };
public:    
    OTValueVentStatus();
};

class OTValueVentMasterStatus: public OTValueFlags {
public:    
    enum {
        BIT_VENT_ENABLE = 8,
        BIT_OPEN_BYPASS = 9,
        BIT_AUTO_BYPASS = 10,
        BIT_FREE_VENT_ENABLE = 11
    };
    OTValueVentMasterStatus();
private:
    const Flag flags[4] PROGMEM = {
        {BIT_VENT_ENABLE,       "vent_enable"},
        {BIT_OPEN_BYPASS,       "open_bypass"},
        {BIT_AUTO_BYPASS,       "auto_bypass"},
        {BIT_FREE_VENT_ENABLE,  "free_vent_enable"}
    };
protected:
    bool sendDiscovery() override;
};

class OTValueSlaveConfigMember: public OTValueFlags {
private:
    void getValue(JsonVariant var) const override;
    const Flag flags[7] PROGMEM = {
        {8, "dhw_present",              "DHW present",              nullptr},
        {9, "ctrl_type",                "Control type on/off",      nullptr},
        {10, "cooling_config",          "Cooling supported",        nullptr},
        {11, "dhw_config",              "DHW storage",              nullptr},
        {12, "master_lowoff_pumpctrl",  "Master pump ctrl allowed", nullptr},
        {13, "ch2_present",             "CH2 present",              nullptr},
        {15, "heat_cool_ctrl",          "Heat/Cool master ctrl",    nullptr},
    };
protected:
    bool sendDiscovery() override;
public:    
    OTValueSlaveConfigMember();
    bool hasDHW() const;
    bool hasCh2() const;
    bool hasCooling() const;
};


class OTValueFaultFlags: public OTValueFlags {
private:
    void getValue(JsonVariant var) const override;
    const Flag flags[6] PROGMEM = {
        {8, "service_request",      "service request",      HA_DEVICE_CLASS_PROBLEM},
        {9, "lockout_reset",        "lockout reset",        HA_DEVICE_CLASS_PROBLEM},
        {10, "low_water_pressure",  "low pressure",         HA_DEVICE_CLASS_PROBLEM},
        {11, "gas_flame_fault",     "flame fault",          HA_DEVICE_CLASS_PROBLEM},
        {12, "air_pressure_fault",  "air pressure fault",   HA_DEVICE_CLASS_PROBLEM},
        {13, "water_over_temp",     "water over temp",      HA_DEVICE_CLASS_PROBLEM}
    };
    const char *OEM_FAULT_CODE PROGMEM = "oem_fault_code";
    bool sendDiscovery() override;
public:
    OTValueFaultFlags(const int interval);
};


class OTValueVentFaultFlags: public OTValueFlags {
private:
    void getValue(JsonVariant var) const override;
    const Flag flags[4] PROGMEM = {
        {8, "service_request",      "vent. service request",    HA_DEVICE_CLASS_PROBLEM},
        {9, "exhaust_fan_fault",    "exhaust fan fault",        HA_DEVICE_CLASS_PROBLEM},
        {10, "inlet_fan_fault",     "inlet fan fault",          HA_DEVICE_CLASS_PROBLEM},
        {11, "frost_protection",    "frost protection",         HA_DEVICE_CLASS_PROBLEM}
    };
    const char *OEM_VENT_FAULT_CODE PROGMEM = "oem_vent_fault_code";
    bool sendDiscovery() override;
public:
    OTValueVentFaultFlags(const int interval);
};

class OTValueProductVersion: public OTValue {
private:
    void getValue(JsonVariant var) const override;
    bool sendDiscovery() override;
public:    
    OTValueProductVersion(const OpenThermMessageID id, const int interval, PGM_P haName);
};

class OTValueCapacityModulation: public OTValue {
private:
    void getValue(JsonVariant var) const override;
    const char *MAX_CAPACITY = "max_capacity" PROGMEM;
    const char *MIN_MODULATION = "min_modulation" PROGMEM;
protected:
    bool sendDiscovery() override;
public:    
    OTValueCapacityModulation();
};

class OTValueTempBounds: public OTValue {
private:
    void getValue(JsonVariant var) const override;
    const char *MAX = "max" PROGMEM;
    const char *MIN = "min" PROGMEM;
    const char *namePrefix;
protected:
    bool sendDiscovery() override;
public:    
    OTValueTempBounds(const OpenThermMessageID id, const char *namePrefix);
};

class OTValueMasterConfig: public OTValueFlags {
private:
    const Flag flags[1] PROGMEM = {
        {8, "smartPowerImplemented"},
    };
    void getValue(JsonVariant var) const override;
protected:
    bool sendDiscovery() override;
public:    
    OTValueMasterConfig();
};

class OTValueRemoteParameter: public OTValueFlags {
private:
    const Flag flags[4] PROGMEM = {
        {0, "dhw_setpoint_rw",      "DHW setpoint write",           nullptr},
        {1, "max_ch_setpoint_rw",   "Max. CH setpoint write",       nullptr},
        {8, "dhw_setpoint_trans",   "DHW setpoint transfer",        nullptr},
        {9, "max_ch_setpoint_trans", "Max. CH setpoint transfer",   nullptr}
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
    bool sendDiscovery() override;
public:
    OTValueRemoteOverrideFunction();
};


class OTValueDayTime: public OTValue {
private:
    void getValue(JsonVariant obj) const override;
protected:
    bool sendDiscovery() override;
public:    
    OTValueDayTime();
};

class OTValueDate: public OTValue {
private:
    void getValue(JsonVariant obj) const override;
protected:
    bool sendDiscovery() override;
public:    
    OTValueDate();
};


class OTValueHeatExchangerTemp: public OTValueFloat {
protected:
    bool sendDiscovery() override;
public:
    OTValueHeatExchangerTemp();
};


class OTValueBoilerFanSpeed: public OTValue {
private:
    void getValue(JsonVariant var) const override;
    const char *SETPOINT = "setpoint" PROGMEM;
    const char *ACTUAL = "actual" PROGMEM;
protected:
    bool sendDiscovery() override;
public:
    OTValueBoilerFanSpeed();
};


class OTValueFlameCurrent: public OTValueFloat {
protected:
    bool sendDiscovery() override;
public:
    OTValueFlameCurrent();
};


class BrandInfo: public OTValue {
private:
    void getValue(JsonVariant var) const override;
    char buf[50];
public:
    BrandInfo(const OpenThermMessageID id, const char *name);
    void init(const bool enabled) override;
    bool process() override;
    void setValue(const OpenThermMessageType ty, const uint16_t val) override;
};


extern OTValue *slaveValues[55];
extern OTValue *masterValues[20];
extern const char* getOTname(OpenThermMessageID id);