#include <Arduino.h>
#include "otvalues.h"
#include "otcontrol.h"
#include "mqtt.h"

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
    {OpenThermMessageID::RPMsupply,                 PSTR("inlet_fan_speed")},
    {OpenThermMessageID::RemoteOverrideFunction,    PSTR("remote_override_function")},
    {OpenThermMessageID::UnsuccessfulBurnerStarts,  PSTR("unsuccessful_burner_starts")},
    {OpenThermMessageID::FlameSignalTooLowNumber,   PSTR("num_flame_signal_low")},
    {OpenThermMessageID::OEMDiagnosticCode,         PSTR("oem_diag_code")},
    {OpenThermMessageID::SuccessfulBurnerStarts,    PSTR("burner_starts")},
    {OpenThermMessageID::CHPumpStarts,              PSTR("ch_pump_starts")},
    {OpenThermMessageID::BurnerOperationHours,      PSTR("burner_op_hours")},
    {OpenThermMessageID::DHWBurnerOperationHours,   PSTR("dhw_burner_op_hours")},
    {OpenThermMessageID::OpenThermVersionMaster,    PSTR("master_ot_version")},
    {OpenThermMessageID::OpenThermVersionSlave,     PSTR("slave_ot_version")},
    {OpenThermMessageID::MasterVersion,             PSTR("master_prod_version")},
    {OpenThermMessageID::SlaveVersion,              PSTR("slave_prod_version")}
};

OTValue *slaveValues[42] = { // reply data collected (read) from slave (boiler / ventilation / solar)
    new OTValueSlaveConfigMember(),
    new OTValueProductVersion(  OpenThermMessageID::OpenThermVersionSlave,      0),
    new OTValueProductVersion(  OpenThermMessageID::SlaveVersion,               0),
    new OTValueStatus(),
    new OTValueVentStatus(),
    new OTValueCapacityModulation(),
    new OTValueDHWBounds(),
    new OTValueCHBounds(),
    new OTValueFloat(           OpenThermMessageID::TrOverride,                 10),
    new OTValueFloat(           OpenThermMessageID::RelModLevel,                10),
    new OTValueFloat(           OpenThermMessageID::CHPressure,                 30),
    new OTValueFloat(           OpenThermMessageID::DHWFlowRate,                10),
    new OTValueFloat(           OpenThermMessageID::Tboiler,                    10),
    new OTValueFloat(           OpenThermMessageID::TflowCH2,                   10),
    new OTValueFloat(           OpenThermMessageID::Tdhw,                       10),
    new OTValueFloat(           OpenThermMessageID::Tdhw2,                      10),
    new OTValueFloat(           OpenThermMessageID::Toutside,                   30),
    new OTValueFloat(           OpenThermMessageID::Tret,                       10),
    new OTValuei16(             OpenThermMessageID::Texhaust,                   10),
    new OTValueFloat(           OpenThermMessageID::TrOverride2,                10),
    new OTValueProductVersion(  OpenThermMessageID::OpenThermVersionVentilationHeatRecovery,    0),
    new OTValueProductVersion(  OpenThermMessageID::VentilationHeatRecoveryVersion,             0),
    new OTValueu16(             OpenThermMessageID::RelVentLevel,               10),
    new OTValueu16(             OpenThermMessageID::RHexhaust,                  10),
    new OTValueu16(             OpenThermMessageID::CO2exhaust,                 10),
    new OTValueFloat(           OpenThermMessageID::Tsi,                        10),
    new OTValueFloat(           OpenThermMessageID::Tso,                        10),
    new OTValueFloat(           OpenThermMessageID::Tei,                        10),
    new OTValueFloat(           OpenThermMessageID::Teo,                        10),
    new OTValueu16(             OpenThermMessageID::RPMexhaust,                 10),
    new OTValueu16(             OpenThermMessageID::RPMsupply,                  10),
    new OTValueu16(             OpenThermMessageID::UnsuccessfulBurnerStarts,   30),
    new OTValueu16(             OpenThermMessageID::FlameSignalTooLowNumber,    30),
    new OTValueu16{             OpenThermMessageID::OEMDiagnosticCode,          60},
    new OTValueu16(             OpenThermMessageID::SuccessfulBurnerStarts,     30),
    new OTValueu16(             OpenThermMessageID::CHPumpStarts,               30),
    new OTValueu16(             OpenThermMessageID::BurnerOperationHours,       120),
    new OTValueu16(             OpenThermMessageID::DHWBurnerOperationHours,    120),
    new OTValueFaultFlags(                                                      30),
    new OTValueRemoteParameter(),
    new OTValueRemoteOverrideFunction(),
    new OTValueVentFaultFlags(                                                  30),
};


OTValue *thermostatValues[17] = { // request data sent (written) from roomunit
    new OTValueFloat(           OpenThermMessageID::TSet,                   -1),
    new OTValueFloat(           OpenThermMessageID::TsetCH2,                -1),
    new OTValueFloat(           OpenThermMessageID::Tr,                     -1),
    new OTValueFloat(           OpenThermMessageID::TrCH2,                  -1),
    new OTValueFloat(           OpenThermMessageID::TrSet,                  -1),
    new OTValueFloat(           OpenThermMessageID::TrSetCH2,               -1),
    new OTValueProductVersion(  OpenThermMessageID::MasterVersion,          -1),
    new OTValueFloat(           OpenThermMessageID::MaxRelModLevelSetting,  -1),
    new OTValueProductVersion(  OpenThermMessageID::OpenThermVersionMaster, -1),
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
OTValue::OTValue(const OpenThermMessageID id, const int interval):
        id(id),
        interval(interval),
        value(0),
        enabled(interval != -1),
        isSet(false),
        discFlag(false) {
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

/* missing discoveries: 
    new OTValueCapacityModulation(),
    new OTValueDHWBounds(),
    new OTValueCHBounds(),
    new OTValueFloat(           OpenThermMessageID::TrOverride,                 10),
    new OTValueFloat(           OpenThermMessageID::DHWFlowRate,                10),
    new OTValueFloat(           OpenThermMessageID::TrOverride2,                10),
    new OTValueu16(             OpenThermMessageID::CO2exhaust,                 10),


    new OTValueu16(             OpenThermMessageID::CHPumpStarts,               30),
    new OTValueRemoteParameter(),
    new OTValueRemoteOverrideFunction(),
*/

    switch (id) {
        case OpenThermMessageID::Tboiler:
            haDisc.createTempSensor(F("flow temp."), FPSTR(name));
            break;
            
        case OpenThermMessageID::TflowCH2:
            haDisc.createTempSensor(F("flow temp. 2"), FPSTR(name));
            break;

        case OpenThermMessageID::Tret:
            haDisc.createTempSensor(F("return temp."), FPSTR(name));
            break;

        case OpenThermMessageID::RelModLevel:
            haDisc.createPowerFactorSensor(F("rel. modulation"), FPSTR(name));
            break;

        case OpenThermMessageID::Tdhw:
            haDisc.createTempSensor(F("DHW temperature"), FPSTR(name));
            break;

        case OpenThermMessageID::Tdhw2:
            haDisc.createTempSensor(F("DHW temperature 2"), FPSTR(name));
            break;

        case OpenThermMessageID::CHPressure:
            haDisc.createPressureSensor(F("CH pressure"), FPSTR(name));
            break;

        case OpenThermMessageID::RelVentLevel:
            haDisc.createSensor(F("rel. ventilation"), FPSTR(name));
            break;

        case OpenThermMessageID::RHexhaust:
            haDisc.createSensor(F("humidity exhaust"), FPSTR(name));
            break;

        case OpenThermMessageID::Tsi:
            haDisc.createTempSensor(F("supply inlet temp."), FPSTR(name));
            break;

        case OpenThermMessageID::Tso:
            haDisc.createTempSensor(F("supply outlet temp."), FPSTR(name));
            break;

        case OpenThermMessageID::Tei:
            haDisc.createTempSensor(F("exhaust inlet temp."), FPSTR(name));
            break;

        case OpenThermMessageID::Teo:
            haDisc.createTempSensor(F("exhaust outlet temp."), FPSTR(name));
            break;

        case OpenThermMessageID::RPMexhaust:
            haDisc.createSensor(F("exhaust fan speed"), FPSTR(name));
            break;

        case OpenThermMessageID::RPMsupply:
            haDisc.createSensor(F("supply fan speed"), FPSTR(name));
            break;

        case OpenThermMessageID::BurnerOperationHours:
            haDisc.createHourDuration(F("operating hours"), FPSTR(name));
            break;

        case OpenThermMessageID::DHWBurnerOperationHours:
            haDisc.createHourDuration(F("operating hours DHW"), FPSTR(name));
            break;

        case OpenThermMessageID::SuccessfulBurnerStarts:
            haDisc.createSensor(F("burnerstarts"), FPSTR(name));
            break;

        case OpenThermMessageID::UnsuccessfulBurnerStarts:
            haDisc.createSensor(F("failed burnerstarts"), FPSTR(name));
            break;

        case OpenThermMessageID::TSet:
            haDisc.createTempSensor(F("flow set temp."), FPSTR(name));
            break;

        case OpenThermMessageID::Texhaust:
            haDisc.createTempSensor(F("exhaust temp."), FPSTR(name));
            break;

        case OpenThermMessageID::SlaveVersion:
            haDisc.createSensor(F("productversion slave"), FPSTR(name));
            break;

        case OpenThermMessageID::MasterVersion:
            haDisc.createSensor(F("productversion master"), FPSTR(name));
            break;

        case OpenThermMessageID::OpenThermVersionMaster:
            haDisc.createSensor(F("OT-version master"), FPSTR(name));
            break;

        case OpenThermMessageID::Toutside:
            haDisc.createSensor(F("outside temp."), FPSTR(name));
            break;

        case OpenThermMessageID::OpenThermVersionSlave:
            haDisc.createSensor(F("OT-version slave"), FPSTR(name));
            break;

        case OpenThermMessageID::DHWFlowRate:
            haDisc.createSensor(F("flow rate"), FPSTR(name));
            break;

        case OpenThermMessageID::FlameSignalTooLowNumber:
            haDisc.createSensor(F("Flame sig low"), FPSTR(name));
            break;

        case OpenThermMessageID::OEMDiagnosticCode:
            haDisc.createSensor(F("OEM diagnostic code"), FPSTR(name));
            break;

        // TODO
        case OpenThermMessageID::CHPumpStarts:
        case OpenThermMessageID::TrOverride:
            return false;

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
    valTempl.replace("#", FPSTR(name));
    haDisc.setValueTemplate(valTempl);
        
    return haDisc.publish();
}

bool OTValue::sendDiscovery(String field) {
    bool inSlave = false;
    for (auto *valobj: slaveValues) {
        if (valobj == this) {
            inSlave = true;
            break;
        }
    }

    String valTempl = F("{{ value_json");
    valTempl += inSlave ? F(".slave.") : F(".thermostat.");
    if (field.isEmpty()) 
        valTempl += FPSTR(getName());
    else
        valTempl += field;
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

OTValueu16::OTValueu16(const OpenThermMessageID id, const int interval):
        OTValue(id, interval) {
}

uint16_t OTValueu16::getValue() const {
    return value;
}

void OTValueu16::getValue(JsonObject &obj) const {
    obj[FPSTR(getName())] = getValue();
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


OTValueProductVersion::OTValueProductVersion(const OpenThermMessageID id, const int interval):
        OTValue(id, interval) {
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