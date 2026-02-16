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

using enum OpenThermMessageID;

static const OTItem OTITEMS[] PROGMEM = {
//  ID of message                                   string id for MQTT                  
    {Status,                    PSTR("status")},
    {TSet,                      PSTR("ch_set_t")},
    {MConfigMMemberIDcode,      PSTR("master_config_member")},
    {SConfigSMemberIDcode,      PSTR("slave_config_member")},
    {RemoteRequest,             PSTR("remote_req")},
    {ASFflags,                  PSTR("fault_flags")},
    {RBPflags,                  PSTR("rp_flags")},
    {TsetCH2,                   PSTR("ch_set_t2")},
    {TrOverride,                PSTR("tr_override")},
    {TSP,                       PSTR("num_tsps")},
    {FHBsize,                   PSTR("size_fhb")},
    {MaxRelModLevelSetting,     PSTR("max_rel_mod")},
    {MaxCapacityMinModLevel,    PSTR("max_cap_min_mod")},
    {TrSet,                     PSTR("room_set_t")},
    {RelModLevel,               PSTR("rel_mod")},
    {CHPressure,                PSTR("ch_pressure")},
    {DHWFlowRate,               PSTR("dhw_flow_rate")},
    {DayTime,                   PSTR("day_time")},
    {Date,                      PSTR("date")},
    {Year,                      PSTR("year")},
    {TrSetCH2,                  PSTR("room_set_t2")},
    {Tr,                        PSTR("room_t")},
    {Tboiler,                   PSTR("flow_t")},
    {Tdhw,                      PSTR("dhw_t")},
    {Toutside,                  PSTR("outside_t")},
    {Tret,                      PSTR("return_t")},
    {TflowCH2,                  PSTR("flow_t2")},
    {Tdhw2,                     PSTR("dhw_t2")},
    {Texhaust,                  PSTR("exhaust_t")},
    {TboilerHeatExchanger,      PSTR("boiler_heat_ex_t")},
    {BoilerFanSpeedSetpointAndActual, PSTR("boiler_fan")},
    {FlameCurrent,              PSTR("flame_current")},
    {TrCH2,                     PSTR("room_t2")},
    {TrOverride2,               PSTR("tr_override2")},           
    {TdhwSetUBTdhwSetLB,        PSTR("dhw_bounds")},
    {MaxTSetUBMaxTSetLB,        PSTR("ch_bounds")},
    {TdhwSet,                   PSTR("dhw_set_t")},
    {StatusVentilationHeatRecovery, PSTR("vent_status")},
    {Vset,                      PSTR("rel_vent_set")},
    {ASFflagsOEMfaultCodeVentilationHeatRecovery, PSTR("vent_fault_flags")},
    {OpenThermVersionVentilationHeatRecovery,   PSTR("vent_ot_version")},
    {VentilationHeatRecoveryVersion,    PSTR("vent_prod_version")},
    {RelVentLevel,              PSTR("rel_vent")},
    {RHexhaust,                 PSTR("rel_hum_exhaust")},
    {CO2exhaust,                PSTR("co2_exhaust")},
    {Tsi,                       PSTR("supply_inlet_t")},
    {Tso,                       PSTR("supply_outlet_t")},
    {Tei,                       PSTR("exhaust_inlet_t")},
    {Teo,                       PSTR("exhaust_outlet_t")},
    {RPMexhaust,                PSTR("exhaust_fan_speed")},
    {RPMsupply,                 PSTR("supply_fan_speed")},
    {Brand,                     PSTR("brand")},
    {BrandVersion,              PSTR("brand_version")},
    {BrandSerialNumber,         PSTR("brand_serial")},
    {PowerCycles,               PSTR("power_cycles")},
    {RemoteOverrideFunction,    PSTR("remote_override_function")},
    {UnsuccessfulBurnerStarts,  PSTR("unsuccessful_burner_starts")},
    {FlameSignalTooLowNumber,   PSTR("num_flame_signal_low")},
    {OEMDiagnosticCode,         PSTR("oem_diag_code")},
    {SuccessfulBurnerStarts,    PSTR("burner_starts")},
    {CHPumpStarts,              PSTR("ch_pump_starts")},
    {DHWPumpValveStarts,        PSTR("dhw_pump_starts")},
    {DHWBurnerStarts,           PSTR("dhw_burner_starts")},
    {BurnerOperationHours,      PSTR("burner_op_hours")},
    {CHPumpOperationHours,      PSTR("chpump_op_hours")},
    {DHWPumpValveOperationHours,PSTR("dhwpump_op_hours")},
    {DHWBurnerOperationHours,   PSTR("dhw_burner_op_hours")},
    {OpenThermVersionMaster,    PSTR("master_ot_version")},
    {OpenThermVersionSlave,     PSTR("slave_ot_version")},
    {MasterVersion,             PSTR("master_prod_version")},
    {SlaveVersion,              PSTR("slave_prod_version")}
};

OTValue *slaveValues[55] = { // reply data collected (read) from slave (boiler / ventilation / solar)
    new OTValueSlaveConfigMember(),
    new OTValueProductVersion(  OpenThermVersionSlave,      0,                 PSTR("OT-version slave")),
    new OTValueProductVersion(  SlaveVersion,               0,                 PSTR("productversion slave")),
    new OTValueStatus(),
    new OTValueVentStatus(),
    new OTValueCapacityModulation(),
    new OTValueTempBounds(TdhwSetUBTdhwSetLB,               PSTR("DHW")),
    new OTValueTempBounds(MaxTSetUBMaxTSetLB,               PSTR("CH")),
    new OTValueFloatTemp(       TrOverride,                 PSTR("room setpoint override")),
    new OTValueFloat(           RelModLevel,                10),
    new OTValueFloat(           CHPressure,                 30),
    new OTValueFloat(           DHWFlowRate,                10),
    new OTValueFloatTemp(       Tboiler,                    PSTR("flow temp.")),
    new OTValueFloatTemp(       TflowCH2,                   PSTR("flow temp. 2")),
    new OTValueFloatTemp(       Tdhw,                       PSTR("DHW temperature")),
    new OTValueFloatTemp(       Tdhw2,                      PSTR("DHW temperature 2")),
    new OTValueFloatTemp(       Toutside,                   PSTR("outside temp.")),
    new OTValueFloatTemp(       Tret,                       PSTR("return temp.")),
    new OTValuei16(             Texhaust,                   10),
    new OTValueFloatTemp(       TrOverride2,                PSTR("room setpoint 2 override")),
    new OTValueProductVersion(  OpenThermVersionVentilationHeatRecovery,    0, PSTR("OT-version slave")),
    new OTValueProductVersion(  VentilationHeatRecoveryVersion,             0, PSTR("productversion slave")),
    new OTValueu16(             RelVentLevel,               10),
    new OTValueu16(             RHexhaust,                  10),
    new OTValueu16(             CO2exhaust,                 10),
    new OTValueFloatTemp(       Tsi,                        PSTR("supply inlet temp.")),
    new OTValueFloatTemp(       Tso,                        PSTR("supply outlet temp.")),
    new OTValueFloatTemp(       Tei,                        PSTR("exhaust inlet temp.")),
    new OTValueFloatTemp(       Teo,                        PSTR("exhaust outlet temp.")),
    new OTValueu16(             RPMexhaust,                 10),
    new OTValueu16(             RPMsupply,                  10),
    new OTValueu16(             PowerCycles,                180,    PSTR("power cycles")),
    new OTValueu16(             UnsuccessfulBurnerStarts,   60,     PSTR("failed burnerstarts")),
    new OTValueu16(             FlameSignalTooLowNumber,    60,     PSTR("Flame sig low")),
    new OTValueu16(             OEMDiagnosticCode,          60,     PSTR("OEM diagnostic code")),
    new OTValueu16(             SuccessfulBurnerStarts,     60,     PSTR("burnerstarts")),
    new OTValueu16(             CHPumpStarts,               60,     PSTR("CH pump starts")),
    new OTValueu16(             DHWPumpValveStarts,         60,     PSTR("DHW pump starts")),
    new OTValueu16(             DHWBurnerStarts,            60,     PSTR("DHW burnerstarts")),
    new OTValueOperatingHours(  BurnerOperationHours,               PSTR("burner op. hours")),
    new OTValueOperatingHours(  CHPumpOperationHours,               PSTR("DHW pump op. hours")),
    new OTValueOperatingHours(  DHWPumpValveOperationHours,         PSTR("DHW pump/value op. hours")),
    new OTValueOperatingHours(  DHWBurnerOperationHours,            PSTR("DHW op. hours")),
    new OTValueFaultFlags(                                                      30),
    new OTValueRemoteParameter(),
    new OTValueRemoteOverrideFunction(),
    new OTValueVentFaultFlags(                                                  30),
    new OTValueHeatExchangerTemp(),
    new OTValueBoilerFanSpeed(),
    new OTValueFlameCurrent(),
    new BrandInfo(              Brand,                              PSTR("brand")),
    new BrandInfo(              BrandVersion,                       PSTR("brand version")),
    new BrandInfo(              BrandSerialNumber,                  PSTR("brand serial")),

    new OTValueBufSize(         TSP),
    new OTValueBufSize(         FHBsize)
};


OTValue *thermostatValues[18] = { // request data sent (written) from roomunit
    new OTValueFloat(           TSet,                   -1),
    new OTValueFloat(           TsetCH2,                -1),
    new OTValueFloat(           Tr,                     -1),
    new OTValueFloat(           TrCH2,                  -1),
    new OTValueFloat(           TrSet,                  -1),
    new OTValueFloat(           TrSetCH2,               -1),
    new OTValueProductVersion(  MasterVersion,          -1, PSTR("productversion master")),
    new OTValueFloat(           MaxRelModLevelSetting,  -1),
    new OTValueProductVersion(  OpenThermVersionMaster, -1, PSTR("OT-version master")),
    new OTValueMasterConfig(),
    new OTValueFloat(           TdhwSet,                -1),
    new OTValueMasterStatus(),
    new OTValueVentMasterStatus(),
    new OTValueDayTime(),
    new OTValueDate(),
    new OTValueu16(             Year,                   -1),
    new OTValueu16(             Vset,                   -1),
    new OTValueFloat(           Toutside,               -1)
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
        lastMsgType(OpenThermMessageType::RESERVED),
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

OTValueSlaveConfigMember* OTValue::getSlaveConfig() {
    return static_cast<OTValueSlaveConfigMember*>(getSlaveValue(SConfigSMemberIDcode));
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

bool OTValue::hasReply() const {
    return numSet > 0;
}

OpenThermMessageType OTValue::getLastMsgType() const {
    return lastMsgType;
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
        case CO2exhaust:
            haDisc.createSensor(F("CO2 exhaust"), sName);
            haDisc.setUnit(FPSTR(HA_UNIT_PPM));
            haDisc.setDeviceClass(F("carbon_dioxide"));
            break;

        case RelModLevel:
            haDisc.createPowerFactorSensor(F("rel. modulation"), sName);
            break;

        case CHPressure:
            haDisc.createPressureSensor(F("CH pressure"), sName);
            break;

        case RelVentLevel:
            haDisc.createSensor(F("rel. ventilation"), sName);
            break;

        case RHexhaust:
            haDisc.createSensor(F("humidity exhaust"), sName);
            haDisc.setDeviceClass(F("humidity"));
            haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
            break;

        case RPMexhaust:
            haDisc.createSensor(F("exhaust fan speed"), sName);
            haDisc.setUnit(FPSTR(HA_UNIT_RPM));
            break;

        case RPMsupply:
            haDisc.createSensor(F("supply fan speed"), sName);
            haDisc.setUnit(FPSTR(HA_UNIT_RPM));
            break;

        case TSet:
            haDisc.createTempSensor(F("flow set temp."), sName);
            break;

        case Texhaust:
            haDisc.createTempSensor(F("exhaust temp."), sName);
            break;

        case DHWFlowRate:
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
        OTValueFlags(Status, -1, flags, sizeof(flags) / sizeof(flags[0]), true) {
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

void OTValueStatus::getValue(JsonVariant var) const {
    OTValueFlags::getValue(var);

    OTValueSlaveConfigMember *cfg = getSlaveConfig();
    if (cfg) {
        if (!cfg->hasDHW())
            var.remove(PSTR(DHW_MODE));

        if (!cfg->hasCh2())
            var.remove(PSTR(CH2_MODE));
    }
}


OTValueMasterStatus::OTValueMasterStatus():
        OTValueFlags(Status, -1, flags, sizeof(flags) / sizeof(flags[0]), false) {
}

void OTValueMasterStatus::getValue(JsonVariant var) const {
    OTValueFlags::getValue(var);

    OTValueSlaveConfigMember *cfg = getSlaveConfig();
    if (cfg) {
        if (!cfg->hasDHW())
            var.remove(PSTR(DHW_ENABLE));

        if (!cfg->hasCh2())
            var.remove(PSTR(CH2_ENABLE));
    }
}


OTValueVentStatus::OTValueVentStatus():
        OTValueFlags(StatusVentilationHeatRecovery, -1, flags, sizeof(flags) / sizeof(flags[0]), true) {
}


OTValueVentMasterStatus::OTValueVentMasterStatus():
        OTValueFlags(StatusVentilationHeatRecovery, -1, flags, sizeof(flags) / sizeof(flags[0]), false) {
}

bool OTValueVentMasterStatus::sendDiscovery() {
    return true;
}

OTValueSlaveConfigMember::OTValueSlaveConfigMember():
        OTValueFlags(SConfigSMemberIDcode, 0, flags, sizeof(flags) / sizeof(flags[0]), true) {
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
        OTValueFlags(ASFflags, interval, flags, sizeof(flags) / sizeof(flags[0]), true) {
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
        OTValueFlags(ASFflagsOEMfaultCodeVentilationHeatRecovery, interval, flags, sizeof(flags) / sizeof(flags[0]), true) {
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
        OTValue(MaxCapacityMinModLevel, 0) {
}

bool OTValueCapacityModulation::sendDiscovery() {
    haDisc.createSensor(F("Max. capacity"), FPSTR(MAX_CAPACITY));
    haDisc.setDeviceClass(F("power"));
    haDisc.setUnit(F("kW"));
    if (!OTValue::sendDiscovery(FPSTR(MAX_CAPACITY)))
        return false;
    haDisc.createSensor(F("Min. modulation"), FPSTR(MIN_MODULATION));
    haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
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
        OTValueFlags(MConfigMMemberIDcode, -1, flags, sizeof(flags) / sizeof(flags[0]), false) {
}

void OTValueMasterConfig::getValue(JsonVariant var) const {
    OTValueFlags::getValue(var);
    var[F("memberId")] = value & 0xFF;
}

bool OTValueMasterConfig::sendDiscovery() {
    return true;
}

OTValueRemoteParameter::OTValueRemoteParameter():
        OTValueFlags(RBPflags, 0, flags, sizeof(flags) / sizeof(flags[0]), true) {
}


OTValueRemoteOverrideFunction::OTValueRemoteOverrideFunction():
        OTValueFlags(RemoteOverrideFunction, 0, flags, sizeof(flags) / sizeof(flags[0]), true) {
}

bool OTValueRemoteOverrideFunction::sendDiscovery() {
    return true;
}

OTValueDayTime::OTValueDayTime():
        OTValue(DayTime, 0) {
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
        OTValue(Date, 0) {
}

void OTValueDate::getValue(JsonVariant var) const {
    var[F("month")] = (value >> 8);
    var[F("day")] = value & 0xFF;
}

bool OTValueDate::sendDiscovery() {
    return true;
}


OTValueHeatExchangerTemp::OTValueHeatExchangerTemp():
        OTValueFloat(TboilerHeatExchanger, 30) {
}

bool OTValueHeatExchangerTemp::sendDiscovery() {
    haDisc.createTempSensor(F("Heat exchange temp."), FPSTR(getName()));
    return OTValue::sendDiscovery("");
}


OTValueBoilerFanSpeed::OTValueBoilerFanSpeed():
        OTValue(BoilerFanSpeedSetpointAndActual, 30) {
}

void OTValueBoilerFanSpeed::getValue(JsonVariant var) const {
    var[PSTR(SETPOINT)] = value >> 8;
    var[PSTR(ACTUAL)] = value & 0xFF;
}

bool OTValueBoilerFanSpeed::sendDiscovery() {
    haDisc.createSensor(F("Boiler fan speed setpoint"), FPSTR(SETPOINT));
    haDisc.setUnit(FPSTR(HA_UNIT_HZ));
    String field = FPSTR(getName());
    if (!OTValue::sendDiscovery(FPSTR(SETPOINT)))
        return false;
    
    haDisc.createSensor(F("Boiler fan speed actual"), FPSTR(ACTUAL));
    haDisc.setUnit(FPSTR(HA_UNIT_HZ));
    return OTValue::sendDiscovery(FPSTR(ACTUAL));
}


OTValueFlameCurrent::OTValueFlameCurrent():
        OTValueFloat(FlameCurrent, 30) {
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