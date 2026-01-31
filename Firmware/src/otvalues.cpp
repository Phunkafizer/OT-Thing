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
    {OpenThermMessageID::TSP,                       PSTR("num_tsps")},
    {OpenThermMessageID::FHBsize,                   PSTR("size_fhb")},
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
    {OpenThermMessageID::Brand,                     PSTR("brand")},
    {OpenThermMessageID::BrandVersion,              PSTR("brand_version")},
    {OpenThermMessageID::BrandSerialNumber,         PSTR("brand_serial")},
    {OpenThermMessageID::PowerCycles,               PSTR("power_cycles")},
    {OpenThermMessageID::RemoteOverrideFunction,    PSTR("remote_override_function")},
    {OpenThermMessageID::UnsuccessfulBurnerStarts,  PSTR("unsuccessful_burner_starts")},
    {OpenThermMessageID::FlameSignalTooLowNumber,   PSTR("num_flame_signal_low")},
    {OpenThermMessageID::OEMDiagnosticCode,         PSTR("oem_diag_code")},
    {OpenThermMessageID::SuccessfulBurnerStarts,    PSTR("burner_starts")},
    {OpenThermMessageID::CHPumpStarts,              PSTR("ch_pump_starts")},
    {OpenThermMessageID::DHWPumpValveStarts,        PSTR("dhw_pump_starts")},
    {OpenThermMessageID::DHWBurnerStarts,           PSTR("dhw_burner_starts")},
    {OpenThermMessageID::BurnerOperationHours,      PSTR("burner_op_hours")},
    {OpenThermMessageID::CHPumpOperationHours,      PSTR("chpump_op_hours")},
    {OpenThermMessageID::DHWPumpValveOperationHours,PSTR("dhwpump_op_hours")},
    {OpenThermMessageID::DHWBurnerOperationHours,   PSTR("dhw_burner_op_hours")},
    {OpenThermMessageID::OpenThermVersionMaster,    PSTR("master_ot_version")},
    {OpenThermMessageID::OpenThermVersionSlave,     PSTR("slave_ot_version")},
    {OpenThermMessageID::MasterVersion,             PSTR("master_prod_version")},
    {OpenThermMessageID::SlaveVersion,              PSTR("slave_prod_version")}
};

OTValue *slaveValues[55] = { // reply data collected (read) from slave (boiler / ventilation / solar)
    new OTValueSlaveConfigMember(),
    new OTValueProductVersion(  OpenThermMessageID::OpenThermVersionSlave,      0,                 PSTR("OT-version slave")),
    new OTValueProductVersion(  OpenThermMessageID::SlaveVersion,               0,                 PSTR("productversion slave")),
    new OTValueStatus(),
    new OTValueVentStatus(),
    new OTValueCapacityModulation(),
    new OTValueTempBounds(OpenThermMessageID::TdhwSetUBTdhwSetLB,               PSTR("DHW")),
    new OTValueTempBounds(OpenThermMessageID::MaxTSetUBMaxTSetLB,               PSTR("CH")),
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
    new OTValueu16(             OpenThermMessageID::PowerCycles,                180,    PSTR("power cycles")),
    new OTValueu16(             OpenThermMessageID::UnsuccessfulBurnerStarts,   60,     PSTR("failed burnerstarts")),
    new OTValueu16(             OpenThermMessageID::FlameSignalTooLowNumber,    60,     PSTR("Flame sig low")),
    new OTValueu16(             OpenThermMessageID::OEMDiagnosticCode,          60,     PSTR("OEM diagnostic code")),
    new OTValueu16(             OpenThermMessageID::SuccessfulBurnerStarts,     60,     PSTR("burnerstarts")),
    new OTValueu16(             OpenThermMessageID::CHPumpStarts,               60,     PSTR("CH pump starts")),
    new OTValueu16(             OpenThermMessageID::DHWPumpValveStarts,         60,     PSTR("DHW pump starts")),
    new OTValueu16(             OpenThermMessageID::DHWBurnerStarts,            60,     PSTR("DHW burnerstarts")),
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
    new BrandInfo(              OpenThermMessageID::Brand,                              PSTR("brand")),
    new BrandInfo(              OpenThermMessageID::BrandVersion,                       PSTR("brand version")),
    new BrandInfo(              OpenThermMessageID::BrandSerialNumber,                  PSTR("brand serial")),

    new OTValueBufSize(         OpenThermMessageID::TSP),
    new OTValueBufSize(         OpenThermMessageID::FHBsize)
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
        interval(interval),
        id(id),
        value(0),
        enabled(interval != -1),
        discFlag(false),
        setFlag(false),
        numSet(0),
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

    if (isSet() && (interval == 0))
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

bool OTValue::isSet() const {
    return setFlag;
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
    {OpenThermMessageID::DayTime,                   PSTR("day_time")},
    {OpenThermMessageID::Date,                      PSTR("date")},
    {OpenThermMessageID::Year,                      PSTR("year")},
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

    return sendDiscovery("");
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
    valTempl += FPSTR(getName());
    if (!field.isEmpty()) {
        valTempl += '.';
        valTempl += field;
    }   
    valTempl += F(" | default(None) }}");

    if (interval == 0)
        haDisc.setStateClass("");

    haDisc.setValueTemplate(valTempl);
    return haDisc.publish(enabled);
}

void OTValue::refreshDisc() {
    discFlag = false;
    if (isSet() && enabled)
        discFlag = sendDiscovery();
}

const char* OTValue::getName() const {
    return OTItem::getName(id);
}

void OTValue::setValue(const OpenThermMessageType ty, const uint16_t val) {
    numSet++;
    if ((ty == OpenThermMessageType::READ_ACK) || (ty == OpenThermMessageType::WRITE_DATA)) {
        value = val;
        setFlag = true;
        enabled = true;
    }
    else
        enabled = false;

    if (!discFlag)
        discFlag = sendDiscovery();

    lastMsgType = ty;
}

uint16_t OTValue::getValue() {
    return value;
}

void OTValue::init(const bool enabled) {
    this->enabled = enabled;
    numSet = 0;
    setFlag = false;
}

void OTValue::getJson(JsonObject &obj) const {
    if (enabled) {
        JsonVariant var = obj[FPSTR(getName())].to<JsonVariant>();
        if (isSet())
            getValue(var);
        else
            var.set(nullptr);
    }
}

void OTValue::getStatus(JsonObject &obj) const {
    JsonObject stat = obj[FPSTR(getName())].to<JsonObject>();

    stat[F("id")] = (int) id;
    stat[F("enabled")] = enabled;
    stat[F("lastMsgType")] = (int) lastMsgType;
    stat[F("numSet")] = numSet;
    if (isSet()) {
        stat[F("value")] = String(value, HEX);
        stat[F("disc")] = discFlag;
    }
}

OTValueu16::OTValueu16(const OpenThermMessageID id, const int interval, const char *haName):
        OTValue(id, interval, haName) {
}

void OTValueu16::getValue(JsonVariant var) const {
    var.set<unsigned int>(value);
}


OTValueBufSize::OTValueBufSize(const OpenThermMessageID id):
        OTValue(id, 0) {
}

void OTValueBufSize::getValue(JsonVariant var) const {
    var.set<unsigned int>(value >> 8);
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

void OTValuei16::getValue(JsonVariant var) const {
    var.set<int>(value);
}


OTValueFloat::OTValueFloat(const OpenThermMessageID id, const int interval):
        OTValue(id, interval) {
}

void OTValueFloat::getValue(JsonVariant var) const {
    int8_t i = value >> 8;
    double d;
    if (i >= 0)
        d = round((i + (value & 0xFF) / 256.0) * 10) / 10.0;
    else
        d = round((i - (value & 0xFF) / 256.0) * 10) / 10.0;

    var.set<double>(d);
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
        numFlags(numFlags),
        flagTable(flagtable),
        slave(slave) {
}

void OTValueFlags::getValue(JsonVariant var) const {
    var[F("value")] = String(value, HEX);
    for (uint8_t i=0; i<numFlags; i++) {
        const char *str = flagTable[i].name;
        var[FPSTR(str)] = (bool) (value & (1<<flagTable[i].bit));
    }
}

bool OTValueFlags::sendDiscFlag(String name, const char *field, const char *devClass)  {
    String dc;
    if (devClass != nullptr)
        dc = FPSTR(devClass);
    haDisc.createBinarySensor(name, FPSTR(field), dc);
    
    String valTmpl = F("{{ None if (value_json.#0.get('#1')) is none else 'ON' if (value_json.#0.#1.#2) else 'OFF' }}");

    valTmpl.replace("#0", slave ? F("slave") : F("thermostat"));
    valTmpl.replace("#1", getName());
    valTmpl.replace("#2", FPSTR(field));
    haDisc.setValueTemplate(valTmpl);
    return haDisc.publish(enabled);
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
    return isSet() ? ((value & (1<<((channel == 0) ? 1 : 5))) != 0) : false;
}

bool OTValueStatus::getFlame() const {
    return isSet() ? ((value & (1<<3)) != 0) : false;
}

bool OTValueStatus::getDhwActive() const {
    return isSet() ? ((value & (1<<2)) != 0) : false;
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

void OTValueSlaveConfigMember::getValue(JsonVariant var) const {
    OTValueFlags::getValue(var);
    var[F("memberId")] = value & 0xFF;
}

bool OTValueSlaveConfigMember::hasDHW() const {
    return (value & (1<<8)) != 0;
}

bool OTValueSlaveConfigMember::hasCh2() const {
    return (value & (1<<13)) != 0;
}

bool OTValueSlaveConfigMember::sendDiscovery() {
    haDisc.createSensor(F("slave member ID"), F("slave_member_id"));
    if (!OTValue::sendDiscovery(F("memberId")))
        return false;

    if (!OTValueFlags::sendDiscovery())
        return false;
    if (!otcontrol.sendCapDiscoveries())
        return false;
    return true;
}


OTValueFaultFlags::OTValueFaultFlags(const int interval):
        OTValueFlags(OpenThermMessageID::ASFflags, interval, flags, sizeof(flags) / sizeof(flags[0]), true) {
}

void OTValueFaultFlags::getValue(JsonVariant var) const {
    OTValueFlags::getValue(var);
    var[PSTR(OEM_FAULT_CODE)] = value & 0xFF;
}

bool OTValueFaultFlags::sendDiscovery() {
    if (!OTValueFlags::sendDiscovery())
        return false;

    haDisc.createSensor(F("OEM fault code"), FPSTR(OEM_FAULT_CODE));
    return OTValue::sendDiscovery(FPSTR(OEM_FAULT_CODE));
}


OTValueVentFaultFlags::OTValueVentFaultFlags(const int interval):
        OTValueFlags(OpenThermMessageID::ASFflagsOEMfaultCodeVentilationHeatRecovery, interval, flags, sizeof(flags) / sizeof(flags[0]), true) {
}

void OTValueVentFaultFlags::getValue(JsonVariant var) const {
    OTValueFlags::getValue(var);
    var[PSTR(OEM_VENT_FAULT_CODE)] = value & 0xFF;
}

bool OTValueVentFaultFlags::sendDiscovery() {
    if (!OTValueFlags::sendDiscovery())
        return false;

    haDisc.createSensor(F("OEM fault code"), FPSTR(OEM_VENT_FAULT_CODE));
    return OTValue::sendDiscovery(FPSTR(OEM_VENT_FAULT_CODE));
}


OTValueProductVersion::OTValueProductVersion(const OpenThermMessageID id, const int interval, const char *haName):
        OTValue(id, interval, haName) {
}

bool OTValueProductVersion::sendDiscovery() {
    haDisc.createSensor(FPSTR(haName), FPSTR(getName()));
    haDisc.setStateClass("");
    return OTValue::sendDiscovery("");
}

void OTValueProductVersion::getValue(JsonVariant var) const {
    String v = String(value >> 8);
    v += '.';
    v += String(value & 0xFF);
    var.set<String>(v);
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

void OTValueCapacityModulation::getValue(JsonVariant var) const {
    var[PSTR(MAX_CAPACITY)] = value >> 8;
    var[PSTR(MIN_MODULATION)] = value & 0xFF;
}

OTValueTempBounds::OTValueTempBounds(const OpenThermMessageID id, const char *namePrefix):
        OTValue(id, 0),
        namePrefix(namePrefix) {
}

void OTValueTempBounds::getValue(JsonVariant var) const {
    var[PSTR(MAX)] = value >> 8;
    var[PSTR(MIN)] = value & 0xFF;
}

bool OTValueTempBounds::sendDiscovery() {
    String name = FPSTR(namePrefix);
    name += F(" max. temp.");
    String id = FPSTR(namePrefix);
    id += F("_max");
    haDisc.createTempSensor(name, id);
    if (!OTValue::sendDiscovery(FPSTR(MAX)))
        return false;

    name = FPSTR(namePrefix);
    name += F(" min. temp.");
    id = FPSTR(namePrefix);
    id += F("_min");
    haDisc.createTempSensor(name, id);
    return OTValue::sendDiscovery(FPSTR(MIN));
}


OTValueMasterConfig::OTValueMasterConfig():
        OTValueFlags(OpenThermMessageID::MConfigMMemberIDcode, -1, flags, sizeof(flags) / sizeof(flags[0]), false) {
}

void OTValueMasterConfig::getValue(JsonVariant var) const {
    OTValueFlags::getValue(var);
    var[F("memberId")] = value & 0xFF;
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

void OTValueDayTime::getValue(JsonVariant var) const {
    var[F("dayOfWeek")] = (value >> 13) & 0x07;
    var[F("hour")] = (value >> 8) & 0x1F;
    var[F("minute")] = value & 0xFF;
}

bool OTValueDayTime::sendDiscovery() {
    return true;
}


OTValueDate::OTValueDate():
        OTValue(OpenThermMessageID::Date, 0) {
}

void OTValueDate::getValue(JsonVariant var) const {
    var[F("month")] = (value >> 8);
    var[F("day")] = value & 0xFF;
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

void OTValueBoilerFanSpeed::getValue(JsonVariant var) const {
    var[PSTR(SETPOINT)] = value >> 8;
    var[PSTR(ACTUAL)] = value & 0xFF;
}

bool OTValueBoilerFanSpeed::sendDiscovery() {
    haDisc.createTempSensor(F("Boiler fan speed setpoint"), FPSTR(SETPOINT));
    String field = FPSTR(getName());
    if (!OTValue::sendDiscovery(FPSTR(SETPOINT)))
        return false;
    
    haDisc.createTempSensor(F("Boiler fan speed actual"), FPSTR(ACTUAL));
    return OTValue::sendDiscovery(FPSTR(ACTUAL));
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


BrandInfo::BrandInfo(const OpenThermMessageID id, const char *name):
        OTValue(id, 0, name) {
    buf[0] = 0;
}

void BrandInfo::init(const bool enabled) {
    OTValue::init(enabled);
    buf[0] = 0;
}

bool BrandInfo::process() {
    if (isSet() || !enabled) 
        return false;

    unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, id, strlen(buf) << 8);
    otcontrol.sendRequest('T', req);
    return true;
}

void BrandInfo::setValue(const OpenThermMessageType ty, const uint16_t val) {
    lastMsgType = ty;
    numSet++;

    if (ty == OpenThermMessageType::READ_ACK) {
        value = val;
        if (strlen(buf) >= sizeof(buf) - 1) {
            setFlag = true;
            return;
        }
        buf[strlen(buf) + 1] = 0;
        buf[strlen(buf)] = val & 0xFF;
        if ( (strlen(buf) == (val >> 8)) || ((val & 0xFF) == 0) )
            setFlag = true;
    }
    else {
        setFlag = (strlen(buf) > 0);
        enabled = setFlag;
    }

    if ((isSet() || !enabled) && !discFlag)
        discFlag = sendDiscovery();
}

void BrandInfo::getValue(JsonVariant var) const {
    var.set<String>(buf);
}