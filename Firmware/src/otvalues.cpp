#include <Arduino.h>
#include "otvalues.h"
#include "otcontrol.h"
#include "mqtt.h"
#include "sensors.h"

struct OTItem {
    OpenThermMessageID id;
    const char* name;
    const char* haName {nullptr};
    static const char* getName(OpenThermMessageID id);
};

static const OTItem OTITEMS[] PROGMEM = {
//  ID of message                                   string id for MQTT                  
    {OpenThermMessageID::Status,                    PSTR("status")},
    {OpenThermMessageID::TSet,                      PSTR("ch_set_t")},
    {OpenThermMessageID::MConfigMMemberIDcode,      PSTR("master_config_member")},
    {OpenThermMessageID::SConfigSMemberIDcode,      PSTR("slave_config_member")},
    {OpenThermMessageID::RemoteRequest,             PSTR("remote_req")},
    {OpenThermMessageID::ASFflags,                  PSTR("fault_flags")},
    {OpenThermMessageID::RBPflags,                  PSTR("rp_flags")},
    {OpenThermMessageID::TsetCH2,                   PSTR("ch_set_t2")},
    {OpenThermMessageID::TrOverride,                PSTR("tr_override")},
    {OpenThermMessageID::MaxRelModLevelSetting,     PSTR("max_rel_mod")},
    {OpenThermMessageID::MaxCapacityMinModLevel,    PSTR("max_cap_min_mod")},
    {OpenThermMessageID::TrSet,                     PSTR("room_set_t")},
    {OpenThermMessageID::RelModLevel,               PSTR("rel_mod")},
    {OpenThermMessageID::CHPressure,                PSTR("ch_pressure")},
    {OpenThermMessageID::DHWFlowRate,               PSTR("dhw_flow_rate")},
    {OpenThermMessageID::DayTime,                   PSTR("day_time")},
    {OpenThermMessageID::Date,                      PSTR("date")},
    {OpenThermMessageID::Year,                      PSTR("year")},
    {OpenThermMessageID::TrSetCH2,                  PSTR("room_set_t2")},
    {OpenThermMessageID::Tr,                        PSTR("room_t")},
    {OpenThermMessageID::Tboiler,                   PSTR("flow_t")},
    {OpenThermMessageID::Tdhw,                      PSTR("dhw_t")},
    {OpenThermMessageID::Toutside,                  PSTR("outside_t")},
    {OpenThermMessageID::Tret,                      PSTR("return_t")},
    {OpenThermMessageID::TflowCH2,                  PSTR("flow_t2")},
    {OpenThermMessageID::Tdhw2,                     PSTR("dhw_t2")},
    {OpenThermMessageID::Texhaust,                  PSTR("exhaust_t")},
    {OpenThermMessageID::TboilerHeatExchanger,      PSTR("boiler_heat_ex_t")},
    {OpenThermMessageID::BoilerFanSpeedSetpointAndActual, PSTR("boiler_fan")},
    {OpenThermMessageID::FlameCurrent,              PSTR("flame_current")},
    {OpenThermMessageID::TrCH2,                     PSTR("room_t2")},
    {OpenThermMessageID::TrOverride2,               PSTR("tr_override2")},           
    {OpenThermMessageID::TdhwSetUBTdhwSetLB,        PSTR("dhw_bounds")},
    {OpenThermMessageID::MaxTSetUBMaxTSetLB,        PSTR("ch_bounds")},
    {OpenThermMessageID::TdhwSet,                   PSTR("dhw_set_t")},
    {OpenThermMessageID::StatusVentilationHeatRecovery, PSTR("vent_status")},
    {OpenThermMessageID::Vset,                      PSTR("rel_vent_set")},
    {OpenThermMessageID::ASFflagsOEMfaultCodeVentilationHeatRecovery, PSTR("vent_fault_flags")},
    {OpenThermMessageID::OpenThermVersionVentilationHeatRecovery,   PSTR("vent_ot_version")},
    {OpenThermMessageID::VentilationHeatRecoveryVersion,    PSTR("vent_prod_version")},
    {OpenThermMessageID::RelVentLevel,              PSTR("rel_vent")},
    {OpenThermMessageID::RHexhaust,                 PSTR("rel_hum_exhaust")},
    {OpenThermMessageID::CO2exhaust,                PSTR("co2_exhaust")},
    {OpenThermMessageID::Tsi,                       PSTR("supply_inlet_t")},
    {OpenThermMessageID::Tso,                       PSTR("supply_outlet_t")},
    {OpenThermMessageID::Tei,                       PSTR("exhaust_inlet_t")},
    {OpenThermMessageID::Teo,                       PSTR("exhaust_outlet_t")},
    {OpenThermMessageID::RPMexhaust,                PSTR("exhaust_fan_speed")},
    {OpenThermMessageID::RPMsupply,                 PSTR("supply_fan_speed")},
    {OpenThermMessageID::RemoteOverrideFunction,    PSTR("remote_override_function")},
    {OpenThermMessageID::UnsuccessfulBurnerStarts,  PSTR("unsuccessful_burner_starts")},
    {OpenThermMessageID::FlameSignalTooLowNumber,   PSTR("num_flame_signal_low")},
    {OpenThermMessageID::OEMDiagnosticCode,         PSTR("oem_diag_code")},
    {OpenThermMessageID::SuccessfulBurnerStarts,    PSTR("burner_starts")},
    {OpenThermMessageID::CHPumpStarts,              PSTR("ch_pump_starts")},
    {OpenThermMessageID::BurnerOperationHours,      PSTR("burner_op_hours")},
    {OpenThermMessageID::CHPumpOperationHours,      PSTR("chpump_op_hours")},
    {OpenThermMessageID::DHWPumpValveOperationHours,PSTR("dhwpump_op_hours")},
    {OpenThermMessageID::DHWBurnerOperationHours,   PSTR("dhw_burner_op_hours")},
    {OpenThermMessageID::OpenThermVersionMaster,    PSTR("master_ot_version")},
    {OpenThermMessageID::OpenThermVersionSlave,     PSTR("slave_ot_version")},
    {OpenThermMessageID::MasterVersion,             PSTR("master_prod_version")},
    {OpenThermMessageID::SlaveVersion,              PSTR("slave_prod_version")}
};

OTValue *slaveValues[47] = { // reply data collected (read) from slave (boiler / ventilation / solar)
    new OTValueSlaveConfigMember(),
    new OTValueProductVersion(  OpenThermMessageID::OpenThermVersionSlave,      0,                 PSTR("OT-version slave")),
    new OTValueProductVersion(  OpenThermMessageID::SlaveVersion,               0,                 PSTR("productversion slave")),
    new OTValueStatus(),
    new OTValueVentStatus(),
    new OTValueCapacityModulation(),
    new OTValueDHWBounds(),
    new OTValueCHBounds(),
    new OTValueFloatTemp(       OpenThermMessageID::TrOverride,                 PSTR("room setpoint override")),
    new OTValueFloat(           OpenThermMessageID::RelModLevel,                10),
    new OTValueFloat(           OpenThermMessageID::CHPressure,                 30),
    new OTValueFloat(           OpenThermMessageID::DHWFlowRate,                10),
    new OTValueFloatTemp(       OpenThermMessageID::Tboiler,                    PSTR("flow temp.")),
    new OTValueFloatTemp(       OpenThermMessageID::TflowCH2,                   PSTR("flow temp. 2")),
    new OTValueFloatTemp(       OpenThermMessageID::Tdhw,                       PSTR("DHW temperature")),
    new OTValueFloatTemp(       OpenThermMessageID::Tdhw2,                      PSTR("DHW temperature 2")),
    new OTValueFloatTemp(       OpenThermMessageID::Toutside,                   PSTR("outside temp.")),
    new OTValueFloatTemp(       OpenThermMessageID::Tret,                       PSTR("return temp.")),
    new OTValuei16(             OpenThermMessageID::Texhaust,                   10),
    new OTValueFloatTemp(       OpenThermMessageID::TrOverride2,                PSTR("room setpoint 2 override")),
    new OTValueProductVersion(  OpenThermMessageID::OpenThermVersionVentilationHeatRecovery,    0, PSTR("OT-version slave")),
    new OTValueProductVersion(  OpenThermMessageID::VentilationHeatRecoveryVersion,             0, PSTR("productversion slave")),
    new OTValueu16(             OpenThermMessageID::RelVentLevel,               10),
    new OTValueu16(             OpenThermMessageID::RHexhaust,                  10),
    new OTValueu16(             OpenThermMessageID::CO2exhaust,                 10),
    new OTValueFloatTemp(       OpenThermMessageID::Tsi,                        PSTR("supply inlet temp.")),
    new OTValueFloatTemp(       OpenThermMessageID::Tso,                        PSTR("supply outlet temp.")),
    new OTValueFloatTemp(       OpenThermMessageID::Tei,                        PSTR("exhaust inlet temp.")),
    new OTValueFloatTemp(       OpenThermMessageID::Teo,                        PSTR("exhaust outlet temp.")),
    new OTValueu16(             OpenThermMessageID::RPMexhaust,                 10),
    new OTValueu16(             OpenThermMessageID::RPMsupply,                  10),
    new OTValueu16(             OpenThermMessageID::UnsuccessfulBurnerStarts,   30,     PSTR("failed burnerstarts")),
    new OTValueu16(             OpenThermMessageID::FlameSignalTooLowNumber,    30,     PSTR("Flame sig low")),
    new OTValueu16(             OpenThermMessageID::OEMDiagnosticCode,          60,     PSTR("OEM diagnostic code")),
    new OTValueu16(             OpenThermMessageID::SuccessfulBurnerStarts,     30,     PSTR("burnerstarts")),
    new OTValueu16(             OpenThermMessageID::CHPumpStarts,               30,     PSTR("CH pump starts")),
    new OTValueOperatingHours(  OpenThermMessageID::BurnerOperationHours,               PSTR("burner op. hours")),
    new OTValueOperatingHours(  OpenThermMessageID::CHPumpOperationHours,               PSTR("DHW pump op. hours")),
    new OTValueOperatingHours(  OpenThermMessageID::DHWPumpValveOperationHours,         PSTR("DHW pump/value op. hours")),
    new OTValueOperatingHours(  OpenThermMessageID::DHWBurnerOperationHours,            PSTR("DHW op. hours")),
    new OTValueFaultFlags(                                                      30),
    new OTValueRemoteParameter(),
    new OTValueRemoteOverrideFunction(),
    new OTValueVentFaultFlags(                                                  30),
    new OTValueHeatExchangerTemp(),
    new OTValueBoilerFanSpeed(),
    new OTValueFlameCurrent(),
};


OTValue *thermostatValues[17] = { // request data sent (written) from roomunit
    new OTValueFloat(           OpenThermMessageID::TSet,                   -1),
    new OTValueFloat(           OpenThermMessageID::TsetCH2,                -1),
    new OTValueFloat(           OpenThermMessageID::Tr,                     -1),
    new OTValueFloat(           OpenThermMessageID::TrCH2,                  -1),
    new OTValueFloat(           OpenThermMessageID::TrSet,                  -1),
    new OTValueFloat(           OpenThermMessageID::TrSetCH2,               -1),
    new OTValueProductVersion(  OpenThermMessageID::MasterVersion,          -1, PSTR("productversion master")),
    new OTValueFloat(           OpenThermMessageID::MaxRelModLevelSetting,  -1),
    new OTValueProductVersion(  OpenThermMessageID::OpenThermVersionMaster, -1, PSTR("OT-version master")),
    new OTValueMasterConfig(),
    new OTValueFloat(           OpenThermMessageID::TdhwSet,                -1),
    new OTValueMasterStatus(),
    new OTValueVentMasterStatus(),
    new OTValueDayTime(),
    new OTValueDate(),
    new OTValueu16(             OpenThermMessageID::Year,                   -1),
    new OTValueu16(             OpenThermMessageID::Vset,                   -1),
};

const char* getOTname(OpenThermMessageID id) {
    return OTItem::getName(id);
}

const char* OTItem::getName(OpenThermMessageID id) {
    for (int i=0; i<sizeof(OTITEMS) / sizeof(OTITEMS[0]); i++)
        if (OTITEMS[i].id == id)
            return OTITEMS[i].name;
    return nullptr;
}

/**
 * @param interval -1: never query. 0: only query once. >0: query every interval seconds
 */
OTValue::OTValue(const OpenThermMessageID id, const int interval, const char *haName):
        id(id),
        interval(interval),
        value(0),
        enabled(interval != -1),
        isSet(false),
        discFlag(false),
        haName(haName) {
}

OTValue* OTValue::getSlaveValue(const OpenThermMessageID id) {
    for (auto *val: slaveValues) {
        if (val->id == id) {
            return val;
        }
    }
    return nullptr;
}

OTValue* OTValue::getThermostatValue(const OpenThermMessageID id) {
    for (auto *val: thermostatValues) {
        if (val->id == id) {
            return val;
        }
    }
    return nullptr;
}

bool OTValue::process() {
    if (!enabled || (interval == -1))
        return false;

    if (isSet && (interval == 0))
        return false;

    if ((lastTransfer > 0) && ((millis() - lastTransfer) / 1000 < interval))
        return false;

    unsigned long request = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, id, value);
    otcontrol.sendRequest('T', request);
    lastTransfer = millis();
    return true;
}

OpenThermMessageID OTValue::getId() const {
    return id;
}

bool OTValue::sendDiscovery() {
    const char *name = getName();
    if (name == nullptr)
        return false;

    String sName = FPSTR(name);

    if (haName != nullptr) {
        haDisc.createSensor(FPSTR(haName), sName);
        return OTValue::sendDiscovery("");
    }
    
/* missing discoveries: 
    {OpenThermMessageID::TrOverride,                PSTR("tr_override")},
    {OpenThermMessageID::DayTime,                   PSTR("day_time")},
    {OpenThermMessageID::Date,                      PSTR("date")},
    {OpenThermMessageID::Year,                      PSTR("year")},
    {OpenThermMessageID::TrCH2,                     PSTR("room_t2")},
    {OpenThermMessageID::TrOverride2,               PSTR("tr_override2")},           
    {OpenThermMessageID::TdhwSet,                   PSTR("dhw_set_t")},
    {OpenThermMessageID::Vset,                      PSTR("rel_vent_set")},
    {OpenThermMessageID::RemoteOverrideFunction,    PSTR("remote_override_function")},
*/

    switch (id) {
        case OpenThermMessageID::CO2exhaust:
            haDisc.createSensor(F("CO2 exhaust"), sName);
            haDisc.setUnit(F("ppm"));
            haDisc.setDeviceClass(F("carbon_dioxide"));
            break;

        case OpenThermMessageID::RelModLevel:
            haDisc.createPowerFactorSensor(F("rel. modulation"), sName);
            break;

        case OpenThermMessageID::CHPressure:
            haDisc.createPressureSensor(F("CH pressure"), sName);
            break;

        case OpenThermMessageID::RelVentLevel:
            haDisc.createSensor(F("rel. ventilation"), sName);
            break;

        case OpenThermMessageID::RHexhaust:
            haDisc.createSensor(F("humidity exhaust"), sName);
            haDisc.setDeviceClass(F("humidity"));
            haDisc.setUnit(F("%"));
            break;

        case OpenThermMessageID::RPMexhaust:
            haDisc.createSensor(F("exhaust fan speed"), sName);
            haDisc.setUnit(F("RPM"));
            break;

        case OpenThermMessageID::RPMsupply:
            haDisc.createSensor(F("supply fan speed"), sName);
            haDisc.setUnit(F("RPM"));
            break;

        case OpenThermMessageID::TSet:
            haDisc.createTempSensor(F("flow set temp."), sName);
            break;

        case OpenThermMessageID::Texhaust:
            haDisc.createTempSensor(F("exhaust temp."), sName);
            break;

        case OpenThermMessageID::DHWFlowRate:
            haDisc.createSensor(F("flow rate"), sName);
            haDisc.setUnit(F("L/min"));
            haDisc.setDeviceClass(F("volume_flow_rate"));
            break;

        default:
            return false;
    }

    String valTempl = F("{{ value_json.thermostat.# }}");
    for (auto *valobj: slaveValues) {
        if (valobj == this) {
            valTempl = F("{{ value_json.slave.# }}");
            break;
        }
    }
    valTempl.replace("#", sName);
    haDisc.setValueTemplate(valTempl);
        
    return haDisc.publish();
}

bool OTValue::sendDiscovery(String field, const bool addBaseName) {
    bool inSlave = false;
    for (auto *valobj: slaveValues) {
        if (valobj == this) {
            inSlave = true;
            break;
        }
    }

    String valTempl = F("{{ value_json");
    valTempl += inSlave ? F(".slave") : F(".thermostat");
    if (field.isEmpty() || addBaseName) {
        valTempl += '.';
        valTempl += FPSTR(getName());
    }
    if (!field.isEmpty()) {
        valTempl += '.';
        valTempl += field;
    }   
    valTempl += F(" }}");

    haDisc.setValueTemplate(valTempl);
    return haDisc.publish();
}

void OTValue::refreshDisc() {
    discFlag = false;
    if (isSet && enabled)
        discFlag = sendDiscovery();
}

const char* OTValue::getName() const {
    return OTItem::getName(id);
}

void OTValue::setValue(uint16_t val) {
    value = val;
    isSet = true;
    enabled = true;

    if (!discFlag)
        discFlag = sendDiscovery();
}

uint16_t OTValue::getValue() {
    return value;
}

void OTValue::disable() {
    enabled = false;
    isSet = false;
}

void OTValue::init(const bool enabled) {
    this->enabled = enabled;
    isSet = false;
}

void OTValue::getJson(JsonObject &obj) const {
    if (enabled) {
        if (isSet)
            getValue(obj);        
        else {
            const char *name = getName();
            if (name)
                obj[FPSTR(name)] = (char*) NULL;
        }
    }
}

OTValueu16::OTValueu16(const OpenThermMessageID id, const int interval, const char *haName):
        OTValue(id, interval, haName) {
}

uint16_t OTValueu16::getValue() const {
    return value;
}

void OTValueu16::getValue(JsonObject &obj) const {
    obj[FPSTR(getName())] = getValue();
}


OTValueOperatingHours::OTValueOperatingHours(const OpenThermMessageID id, const char *haName):
        OTValueu16(id, 300, haName) {
}

bool OTValueOperatingHours::sendDiscovery() {
    haDisc.createHourDuration(FPSTR(haName), FPSTR(getName()));
    return OTValue::sendDiscovery("");
}


OTValuei16::OTValuei16(const OpenThermMessageID id, const int interval):
        OTValue(id, interval) {
}

int16_t OTValuei16::getValue() const {
    return (int16_t) value;
}

void OTValuei16::getValue(JsonObject &obj) const {
    obj[FPSTR(getName())] = getValue();
}


OTValueFloat::OTValueFloat(const OpenThermMessageID id, const int interval):
        OTValue(id, interval) {
}

double OTValueFloat::getValue() const {
    int8_t i = value >> 8;
    if (i >= 0)
        return round((i + (value & 0xFF) / 256.0) * 10) / 10.0;
    else
        return round((i - (value & 0xFF) / 256.0) * 10) / 10.0;
}

void OTValueFloat::getValue(JsonObject &obj) const {
    obj[FPSTR(getName())] = getValue();
}


OTValueFloatTemp::OTValueFloatTemp(const OpenThermMessageID id, const char *haName):
        OTValueFloat(id, 10) {
    this->haName = haName;
}

bool OTValueFloatTemp::sendDiscovery() {
    haDisc.createTempSensor(FPSTR(haName), FPSTR(getName()));
    return OTValue::sendDiscovery("");
}


OTValueFlags::OTValueFlags(const OpenThermMessageID id, const int interval, const Flag *flagtable, const uint8_t numFlags, const bool slave):
        OTValue(id, interval),
        flagTable(flagtable),
        numFlags(numFlags),
        slave(slave) {
}

void OTValueFlags::getValue(JsonObject &obj) const {
    JsonObject flags = obj[FPSTR(getName())].to<JsonObject>();
    flags[F("value")] = String(value, HEX);
    for (uint8_t i=0; i<numFlags; i++) {
        const char *str = flagTable[i].name;
        flags[FPSTR(str)] = (bool) (value & (1<<flagTable[i].bit));
    }
}

bool OTValueFlags::sendDiscFlag(String name, const char *field, const char *devClass)  {
    String dc;
    if (devClass != nullptr)
        dc = FPSTR(devClass);
    haDisc.createBinarySensor(name, FPSTR(field), dc);
    String valTmpl = F("{{ 'ON' if value_json.#0.#1.#2 else 'OFF' }}");
    valTmpl.replace("#0", slave ? F("slave") : F("thermostat"));
    valTmpl.replace("#1", getName());
    valTmpl.replace("#2", FPSTR(field));
    haDisc.setValueTemplate(valTmpl);
    return haDisc.publish();
};

bool OTValueFlags::sendDiscovery() {
    for (uint8_t i=0; i<numFlags; i++) {
        if (flagTable[i].discName == nullptr)
            continue;
        const char *str = flagTable[i].name;
        const char *discName = flagTable[i].discName;
        const char *haDevClass = flagTable[i].haDevClass;
        if (!sendDiscFlag(FPSTR(discName), str, haDevClass))
            return false;
    }
    return true;
}


OTValueStatus::OTValueStatus():
        OTValueFlags(OpenThermMessageID::Status, -1, flags, sizeof(flags) / sizeof(flags[0]), true) {
}

bool OTValueStatus::getChActive(const uint8_t channel) const{
    if (!isSet)
        return false;

    return (value & (1<<((channel == 0) ? 1 : 5))) != 0;
}

bool OTValueStatus::getFlame() const {
    if (!isSet)
        return false;

    return (value & (1<<3)) != 0;
}

bool OTValueStatus::getDhwActive() const {
    if (!isSet)
        return false;

    return (value & (1<<2)) != 0;
}

OTValueMasterStatus::OTValueMasterStatus():
        OTValueFlags(OpenThermMessageID::Status, -1, flags, sizeof(flags) / sizeof(flags[0]), false) {
}


OTValueVentStatus::OTValueVentStatus():
        OTValueFlags(OpenThermMessageID::StatusVentilationHeatRecovery, -1, flags, sizeof(flags) / sizeof(flags[0]), true) {
}


OTValueVentMasterStatus::OTValueVentMasterStatus():
        OTValueFlags(OpenThermMessageID::StatusVentilationHeatRecovery, -1, flags, sizeof(flags) / sizeof(flags[0]), false) {
}

bool OTValueVentMasterStatus::sendDiscovery() {
    return true;
}

OTValueSlaveConfigMember::OTValueSlaveConfigMember():
        OTValueFlags(OpenThermMessageID::SConfigSMemberIDcode, 0, flags, sizeof(flags) / sizeof(flags[0]), true) {
}

void OTValueSlaveConfigMember::getValue(JsonObject &obj) const {
    OTValueFlags::getValue(obj);
    obj[F("memberId")] = value & 0xFF;
}

bool OTValueSlaveConfigMember::hasDHW() const {
    return (value & (1<<8)) != 0;
}

bool OTValueSlaveConfigMember::hasCh2() const {
    return (value & (1<<13)) != 0;
}

bool OTValueSlaveConfigMember::sendDiscovery() {
    if (!OTValueFlags::sendDiscovery())
        return false;
    if (!otcontrol.sendCapDiscoveries())
        return false;
    return true;
}


OTValueFaultFlags::OTValueFaultFlags(const int interval):
        OTValueFlags(OpenThermMessageID::ASFflags, interval, flags, sizeof(flags) / sizeof(flags[0]), true) {
}

void OTValueFaultFlags::getValue(JsonObject &obj) const {
    OTValueFlags::getValue(obj);
    obj[F("oem_fault_code")] = value & 0xFF;
}


OTValueVentFaultFlags::OTValueVentFaultFlags(const int interval):
        OTValueFlags(OpenThermMessageID::ASFflagsOEMfaultCodeVentilationHeatRecovery, interval, flags, sizeof(flags) / sizeof(flags[0]), true) {
}

void OTValueVentFaultFlags::getValue(JsonObject &obj) const {
    OTValueFlags::getValue(obj);
    obj[F("oem_vent_fault_code")] = value & 0xFF;
}


OTValueProductVersion::OTValueProductVersion(const OpenThermMessageID id, const int interval, const char *haName):
        OTValue(id, interval, haName) {
}

bool OTValueProductVersion::sendDiscovery() {
    haDisc.createSensor(FPSTR(haName), FPSTR(getName()));
    haDisc.setStateClass("");
    return OTValue::sendDiscovery("");
}

void OTValueProductVersion::getValue(JsonObject &obj) const {
    String v = String(value >> 8);
    v += '.';
    v += String(value & 0xFF);
    obj[FPSTR(getName())] = v;
}


OTValueCapacityModulation::OTValueCapacityModulation():
        OTValue(OpenThermMessageID::MaxCapacityMinModLevel, 0) {
}

bool OTValueCapacityModulation::sendDiscovery() {
    haDisc.createSensor(F("Max. capacity"), FPSTR(MAX_CAPACITY));
    haDisc.setDeviceClass(F("power"));
    haDisc.setUnit(F("kW"));
    if (!OTValue::sendDiscovery(FPSTR(MAX_CAPACITY)))
        return false;
    haDisc.createSensor(F("Min. modulation"), FPSTR(MIN_MODULATION));
    haDisc.setUnit(F("%"));
    return OTValue::sendDiscovery(FPSTR(MIN_MODULATION));
}

void OTValueCapacityModulation::getValue(JsonObject &obj) const {
    obj[PSTR(MAX_CAPACITY)] = value >> 8;
    obj[PSTR(MIN_MODULATION)] = value & 0xFF;
}

OTValueDHWBounds::OTValueDHWBounds():
        OTValue(OpenThermMessageID::TdhwSetUBTdhwSetLB, 0) {
}

void OTValueDHWBounds::getValue(JsonObject &obj) const {
    obj[PSTR(DHW_MAX)] = value >> 8;
    obj[PSTR(DHW_MIN)] = value & 0xFF;
}

bool OTValueDHWBounds::sendDiscovery() {
    haDisc.createTempSensor(F("DHW max. temp."), FPSTR(DHW_MAX));
    if (!OTValue::sendDiscovery(FPSTR(DHW_MAX)))
        return false;
    
    haDisc.createTempSensor(F("DHW min. temp."), FPSTR(DHW_MIN));
    return OTValue::sendDiscovery(FPSTR(DHW_MIN));
}

OTValueCHBounds::OTValueCHBounds():
        OTValue(OpenThermMessageID::MaxTSetUBMaxTSetLB, 0) {
}

void OTValueCHBounds::getValue(JsonObject &obj) const {
    obj[PSTR(CH_MAX)] = value >> 8;
    obj[PSTR(CH_MIN)] = value & 0xFF;
}

bool OTValueCHBounds::sendDiscovery() {
    haDisc.createTempSensor(F("CH max. temp."), FPSTR(CH_MAX));
    if (!OTValue::sendDiscovery(FPSTR(CH_MAX)))
        return false;
    
    haDisc.createTempSensor(F("CH min. temp."), FPSTR(CH_MIN));
    return OTValue::sendDiscovery(FPSTR(CH_MIN));
}


OTValueMasterConfig::OTValueMasterConfig():
        OTValueFlags(OpenThermMessageID::MConfigMMemberIDcode, -1, flags, sizeof(flags) / sizeof(flags[0]), false) {
}

void OTValueMasterConfig::getValue(JsonObject &obj) const {
    OTValueFlags::getValue(obj);
    obj[F("memberId")] = value & 0xFF;
}

bool OTValueMasterConfig::sendDiscovery() {
    return true;
}

OTValueRemoteParameter::OTValueRemoteParameter():
        OTValueFlags(OpenThermMessageID::RBPflags, 0, flags, sizeof(flags) / sizeof(flags[0]), true) {
}


OTValueRemoteOverrideFunction::OTValueRemoteOverrideFunction():
        OTValueFlags(OpenThermMessageID::RemoteOverrideFunction, 0, flags, sizeof(flags) / sizeof(flags[0]), true) {
}

bool OTValueRemoteOverrideFunction::sendDiscovery() {
    return true;
}

OTValueDayTime::OTValueDayTime():
        OTValue(OpenThermMessageID::DayTime, 0) {
}

void OTValueDayTime::getValue(JsonObject &obj) const {
    obj[F("dayOfWeek")] = (value >> 13) & 0x07;
    obj[F("hour")] = (value >> 8) & 0x1F;
    obj[F("minute")] = value & 0xFF;
}

bool OTValueDayTime::sendDiscovery() {
    return true;
}


OTValueDate::OTValueDate():
        OTValue(OpenThermMessageID::Date, 0) {
}

void OTValueDate::getValue(JsonObject &obj) const {
    obj[F("month")] = (value >> 8);
    obj[F("day")] = value & 0xFF;
}

bool OTValueDate::sendDiscovery() {
    return true;
}


OTValueHeatExchangerTemp::OTValueHeatExchangerTemp():
        OTValueFloat(OpenThermMessageID::TboilerHeatExchanger, 30) {
}

bool OTValueHeatExchangerTemp::sendDiscovery() {
    haDisc.createTempSensor(F("Heat exchange temp."), FPSTR(getName()));
    return OTValue::sendDiscovery("");
}


OTValueBoilerFanSpeed::OTValueBoilerFanSpeed():
        OTValue(OpenThermMessageID::BoilerFanSpeedSetpointAndActual, 30) {
}

void OTValueBoilerFanSpeed::getValue(JsonObject &obj) const {
    JsonObject fanspeeds = obj[FPSTR(getName())].to<JsonObject>();
    fanspeeds[PSTR(SETPOINT)] = value >> 8;
    fanspeeds[PSTR(ACTUAL)] = value & 0xFF;
}

bool OTValueBoilerFanSpeed::sendDiscovery() {
    haDisc.createTempSensor(F("Boiler fan speed setpoint"), FPSTR(SETPOINT));
    String field = FPSTR(getName());
    if (!OTValue::sendDiscovery(FPSTR(SETPOINT), true))
        return false;
    
    haDisc.createTempSensor(F("Boiler fan speed actual"), FPSTR(ACTUAL));
    return OTValue::sendDiscovery(FPSTR(ACTUAL), true);
}


OTValueFlameCurrent::OTValueFlameCurrent():
        OTValueFloat(OpenThermMessageID::FlameCurrent, 30) {
}

bool OTValueFlameCurrent::sendDiscovery() {
    haDisc.createSensor(F("Flame current"), FPSTR(getName()));
    haDisc.setUnit(F("µA"));
    haDisc.setDeviceClass(F("current"));
    return OTValue::sendDiscovery("");
}
