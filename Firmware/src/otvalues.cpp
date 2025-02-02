#include <Arduino.h>
#include "otvalues.h"
#include "otcontrol.h"
#include "HADiscLocal.h"
#include "mqtt.h"

const char ID_STR_STATUS[] PROGMEM = "status";
const char ID_STR_SLAVE_CONFIG_MEMBER[] PROGMEM = "slave_config_member";
const char ID_STR_REL_MOD[] PROGMEM = "rel_mod";
const char ID_STR_CH_PRESSURE[] PROGMEM = "ch_pressure";
const char ID_STR_FLOW_T[] PROGMEM = "flow_t";
const char ID_STR_FLOW_T2[] PROGMEM = "flow_t2";
const char ID_STR_DHW_T[] PROGMEM = "dhw_t";
const char ID_STR_DHW_T2[] PROGMEM = "dhw_t2";
const char ID_STR_ROOM_T[] PROGMEM = "room_t";
const char ID_STR_ROOM_SET_T[] PROGMEM = "room_set_t";
const char ID_STR_DHW_FLOW_RATE[] PROGMEM = "dhw_flow_rate";
const char ID_STR_EXHAUST_T[] PROGMEM = "exhaust_t";
const char ID_STR_BURNER_STARTS[] PROGMEM = "burner_starts";
const char ID_STR_BURNER_OP_HOURS[] PROGMEM = "burner_op_hours";
const char ID_STR_RETURN_T[] PROGMEM = "return_t";
const char ID_STR_CH_PUMP_STARTS[] PROGMEM = "ch_pump_starts";
const char ID_STR_SLAVE_PROD_VERSION[] PROGMEM = "slave_prod_version";
const char ID_STR_DHW_SET_T[] PROGMEM = "dhw_set_t";
const char ID_STR_CH_SET_T[] PROGMEM = "ch_set_t";
const char ID_STR_CH_SET_T2[] PROGMEM = "ch_set_t";
const char ID_STR_CAPACITY_MODULATION[] PROGMEM = "cap_mod";
const char ID_STR_OUTSIDE_T[] PROGMEM = "outside_t";
const char ID_STR_MASTER_PROD_VERSION[] PROGMEM = "master_prod_version";
const char ID_STR_MAX_REL_MOD[] PROGMEM = "max_rel_mod";
const char ID_STR_MASTER_OT_VERSION[] PROGMEM = "master_ot_version";
const char ID_STR_RPFLAGS[] PROGMEM = "rp_flags";

OTItem OTITEMS[] PROGMEM = {
    {OpenThermMessageID::Status,                    ID_STR_STATUS},
    {OpenThermMessageID::RelModLevel,               ID_STR_REL_MOD},
    {OpenThermMessageID::CHPressure,                ID_STR_CH_PRESSURE},
    {OpenThermMessageID::DHWFlowRate,               ID_STR_DHW_FLOW_RATE},
    {OpenThermMessageID::Tboiler,                   ID_STR_FLOW_T},
    {OpenThermMessageID::TflowCH2,                  ID_STR_FLOW_T2},
    {OpenThermMessageID::Tdhw,                      ID_STR_DHW_T},
    {OpenThermMessageID::Tdhw2,                     ID_STR_DHW_T2},
    {OpenThermMessageID::Toutside,                  ID_STR_OUTSIDE_T},
    {OpenThermMessageID::Tret,                      ID_STR_RETURN_T},
    {OpenThermMessageID::TdhwSet,                   ID_STR_DHW_SET_T},
    {OpenThermMessageID::TSet,                      ID_STR_CH_SET_T},
    {OpenThermMessageID::TsetCH2,                   ID_STR_CH_SET_T2},
    {OpenThermMessageID::Tr,                        ID_STR_ROOM_T},
    {OpenThermMessageID::TrSet,                     ID_STR_ROOM_SET_T},
    {OpenThermMessageID::MaxRelModLevelSetting,     ID_STR_MAX_REL_MOD},
    {OpenThermMessageID::SConfigSMemberIDcode,      ID_STR_SLAVE_CONFIG_MEMBER},
    {OpenThermMessageID::SlaveVersion,              ID_STR_SLAVE_PROD_VERSION},
    {OpenThermMessageID::MasterVersion,             ID_STR_MASTER_PROD_VERSION},
    {OpenThermMessageID::Texhaust,                  ID_STR_EXHAUST_T},
    {OpenThermMessageID::SuccessfulBurnerStarts,    ID_STR_BURNER_STARTS},
    {OpenThermMessageID::CHPumpStarts,              ID_STR_CH_PUMP_STARTS},
    {OpenThermMessageID::BurnerOperationHours,      ID_STR_BURNER_OP_HOURS},
    {OpenThermMessageID::OpenThermVersionMaster,    ID_STR_MASTER_OT_VERSION},
    {OpenThermMessageID::RBPflags,                  ID_STR_RPFLAGS} 
};

OTValue *boilerValues[18] = { // data collected from boiler
    new OTValueStatus(),
    new OTValueSlaveConfigMember(),
    new OTValueCapacityModulation(),
    new OTValueDHWBounds(),
    new OTValueFloat(           OpenThermMessageID::RelModLevel,            10),
    new OTValueFloat(           OpenThermMessageID::CHPressure,             30),
    new OTValueFloat(           OpenThermMessageID::DHWFlowRate,            10),
    new OTValueFloat(           OpenThermMessageID::Tboiler,                10),
    new OTValueFloat(           OpenThermMessageID::TflowCH2,               10),
    new OTValueFloat(           OpenThermMessageID::Tdhw,                   10),
    new OTValueFloat(           OpenThermMessageID::Tdhw2,                  10),
    new OTValueFloat(           OpenThermMessageID::Toutside,               30),
    new OTValueFloat(           OpenThermMessageID::Tret,                   10),
    new OTValuei16(             OpenThermMessageID::Texhaust,               10),
    new OTValueu16(             OpenThermMessageID::SuccessfulBurnerStarts, 30),
    new OTValueu16(             OpenThermMessageID::CHPumpStarts,           30),
    new OTValueu16(             OpenThermMessageID::BurnerOperationHours,   120),
    new OTValueProductVersion(  OpenThermMessageID::SlaveVersion,           0),
    //new OTValueFloat(           OpenThermMessageID::TdhwSet,                -1),
    //new OTValueFloat(           OpenThermMessageID::TSet,                   -1),

    //TODO 5, 6, 
};

OTValue *thermostatValues[10] = {
    new OTValueFloat(           OpenThermMessageID::TSet,                   -1),
    new OTValueFloat(           OpenThermMessageID::TsetCH2,                -1),
    new OTValueFloat(           OpenThermMessageID::Tr,                     -1),
    new OTValueFloat(           OpenThermMessageID::TrSet,                  -1),
    new OTValueProductVersion(  OpenThermMessageID::MasterVersion,          -1),
    new OTValueFloat(           OpenThermMessageID::MaxRelModLevelSetting,  -1),
    new OTValueProductVersion(  OpenThermMessageID::OpenThermVersionMaster, -1),
    new OTValueMasterConfig(),
    new OTValueFloat(           OpenThermMessageID::TdhwSet,                -1),
    new OTValueMasterStatus(),
};

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

OTValue* OTValue::getBoilerValue(const OpenThermMessageID id) {
    for (auto *val: boilerValues) {
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

    switch (id) {
        case OpenThermMessageID::Tboiler:
            haDisc.createTempSensor(F("Vorlauftemperatur"), FPSTR(name));
            break;

        case OpenThermMessageID::TflowCH2:
            haDisc.createTempSensor(F("Vorlauftemperatur 2"), FPSTR(name));
            break;

        case OpenThermMessageID::Tret:
            haDisc.createTempSensor(F("Rücklauftemperatur"), FPSTR(name));
            break;

        case OpenThermMessageID::RelModLevel:
            haDisc.createPowerFactorSensor(F("Rel. Modulation"), FPSTR(name));
            break;

        case OpenThermMessageID::Tdhw:
            haDisc.createTempSensor(F("Brauchwasser"), FPSTR(name));
            break;

        case OpenThermMessageID::Tdhw2:
            haDisc.createTempSensor(F("Brauchwasser 2"), FPSTR(name));
            break;

        case OpenThermMessageID::CHPressure:
            haDisc.createPressureSensor(F("Druck Heizkreis"), FPSTR(name));
            break;

        case OpenThermMessageID::BurnerOperationHours:
            haDisc.createHourDuration(F("Betriebsstunden"), FPSTR(name));
            break;

        case OpenThermMessageID::SuccessfulBurnerStarts:
            haDisc.createSensor(F("Brennerstarts"), FPSTR(name));
            break;

        case OpenThermMessageID::TSet:
            haDisc.createTempSensor(F("Vorlaufsolltemperatur"), FPSTR(name));
            break;

        case OpenThermMessageID::Texhaust:
            haDisc.createTempSensor(F("Abgastemperatur"), FPSTR(name));
            break;

        case OpenThermMessageID::SlaveVersion:
            haDisc.createSensor(F("Produktversion Slave"), FPSTR(name));
            break;

        case OpenThermMessageID::MasterVersion:
            haDisc.createSensor(F("Produktversion Master"), FPSTR(name));
            break;

        case OpenThermMessageID::OpenThermVersionMaster:
            haDisc.createSensor(F("OT-Version Master"), FPSTR(name));
            break;

        // TODO
        case OpenThermMessageID::DHWFlowRate:
        case OpenThermMessageID::CHPumpStarts:
            return false;

        default:
            return false;
    }

    String valTempl;
    for (auto *valobj: boilerValues) {
        if (valobj == this) {
            valTempl = F("{{ value_json.boiler.# }}");
            break;
        }
    }
    if (valTempl.isEmpty())
        valTempl = F("{{ value_json.thermostat.# }}");
        
    valTempl.replace("#", FPSTR(name));
    haDisc.setValueTemplate(valTempl);
        
    return mqtt.publish(haDisc.topic, haDisc.doc);
}

const char* OTValue::getName() const {
    return OTItem::getName(id);
}

void OTValue::setValue(uint16_t val) {
    value = val;
    isSet = true;
    enabled = true;

    if (!discFlag) {
        discFlag = sendDiscovery();
    }
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


OTValueStatus::OTValueStatus():
        OTValue(OpenThermMessageID::Status, -1) {
}

void OTValueStatus::getValue(JsonObject &obj) const {
    JsonObject slaveStatus = obj[F("status")].to<JsonObject>();
    slaveStatus[FPSTR(STATUS_FAULT)] = (bool) (value & (1<<0));
    slaveStatus[F("ch_mode")] = (bool) (value & (1<<1));
    slaveStatus[FPSTR(STATUS_DHW_MODE)] = (bool) (value & (1<<2));
    slaveStatus[FPSTR(STATUS_FLAME)] = (bool) (value & (1<<3));
    slaveStatus[F("cooling")] = (bool) (value & (1<<4));
    slaveStatus[F("ch2_mode")] = (bool) (value & (1<<5));
    slaveStatus[F("diagnostic")] = (bool) (value & (1<<6));
}

bool OTValueStatus::sendDiscovery() {
    auto send = [](String name, const char *field, const char *devClass)  {
        haDisc.createBinarySensor(name, FPSTR(field), FPSTR(devClass));
        String valTmpl = F("{{ 'ON' if value_json.boiler.status.# else 'OFF' }}");
        valTmpl.replace("#", FPSTR(field));
        haDisc.setValueTemplate(valTmpl);
        return mqtt.publish(haDisc.topic, haDisc.doc);;
    };

    if (!send(F("Brenner"), STATUS_FLAME, HA_DEVICE_CLASS_RUNNING))
        return false;

    if (!send(F("Brauchwasserbereitung"), STATUS_DHW_MODE, HA_DEVICE_CLASS_RUNNING))
        return false;

    if (!send(F("Heizung"), STATUS_CH_MODE, HA_DEVICE_CLASS_RUNNING))
        return false;

    if (!send(F("Fehler"), STATUS_FAULT, HA_DEVICE_CLASS_PROBLEM))
        return false;
  
    return true;
}


OTValueMasterStatus::OTValueMasterStatus():
        OTValue(OpenThermMessageID::Status, -1) {
}

void OTValueMasterStatus::getValue(JsonObject &obj) const {
    JsonObject status = obj[F("status")].to<JsonObject>();

    status[FPSTR(STATUS_CH_ENABLE)] = (bool) (value & (0x100<<0));
    status[FPSTR(STATUS_DHW_ENABLE)] = (bool) (value & (0x100<<1));
    status[FPSTR(STATUS_COOLING_ENABLE)] = (bool) (value & (0x100<<2));
    status[FPSTR(STATUS_OTC_ACTIVE)] = (bool) (value & (0x100<<3));
    status[FPSTR(STATUS_CH2_ENABLE)] = (bool) (value & (0x100<<4));
}

bool OTValueMasterStatus::sendDiscovery() {
    /*
    auto send = [](String name, const char *field, const char *devClass)  {
        haDisc.createBinarySensor(name, FPSTR(field), FPSTR(devClass));
        String valTmpl = F("{{ 'ON' if value_json.boiler.status.# else 'OFF' }}");
        valTmpl.replace("#", FPSTR(field));
        haDisc.setValueTemplate(valTmpl);
        return mqtt.publish(haDisc.topic, haDisc.doc);;
    };

    if (!send(F("Brenner"), STATUS_FLAME, HA_DEVICE_CLASS_RUNNING))
        return false;

    if (!send(F("Brauchwasserbereitung"), STATUS_DHW_MODE, HA_DEVICE_CLASS_RUNNING))
        return false;

    if (!send(F("Heizung"), STATUS_CH_MODE, HA_DEVICE_CLASS_RUNNING))
        return false;

    if (!send(F("Fehler"), STATUS_FAULT, HA_DEVICE_CLASS_PROBLEM))
        return false;
  */
    return true;
}



OTValueSlaveConfigMember::OTValueSlaveConfigMember():
        OTValue(OpenThermMessageID::SConfigSMemberIDcode, 0) {
}

void OTValueSlaveConfigMember::getValue(JsonObject &obj) const {
    JsonObject slaveConfig = obj[F("slaveConfig")].to<JsonObject>();
    slaveConfig[F("dhw_present")] = (bool) (value & (1<<8)); 
    slaveConfig[F("ctrl_type")] = (bool) (value & (1<<9)); 
    slaveConfig[F("cooling_config")] = (bool) (value & (1<<10)); 
    slaveConfig[F("dhw_config")] = (bool) (value & (1<<11)); 
    slaveConfig[F("master_lowoff_pumpctrl")] = (bool) (value & (1<<12)); 
    slaveConfig[F("ch2_present")] = (bool) (value & (1<<13));

    obj[F("slaveMemberId")] = value & 0xFF;
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
    return true;
}

void OTValueCapacityModulation::getValue(JsonObject &obj) const {
    obj[F("max_capacity")] = value >> 8;
    obj[F("min_modulation")] = value & 0xFF;
}

OTValueDHWBounds::OTValueDHWBounds():
        OTValue(OpenThermMessageID::TdhwSetUBTdhwSetLB, 0) {
}

void OTValueDHWBounds::getValue(JsonObject &obj) const {
    obj[F("dhwMax")] = value >> 8;
    obj[F("dhwMin")] = value & 0xFF;
}

bool OTValueDHWBounds::sendDiscovery() {
    return true;
}


OTValueMasterConfig::OTValueMasterConfig():
        OTValue(OpenThermMessageID::MConfigMMemberIDcode, -1) {
}

void OTValueMasterConfig::getValue(JsonObject &obj) const {
    obj[F("memberId")] = value & 0xFF;
}

bool OTValueMasterConfig::sendDiscovery() {
    return true;
}

OTValueRemoteParameter::OTValueRemoteParameter():
        OTValue(OpenThermMessageID::RBPflags, 0) {
}

void OTValueRemoteParameter::getValue(JsonObject &obj) const {
    JsonObject rpFlags = obj[F("rpFlags")].to<JsonObject>();
    rpFlags[F("dhw_setpoint_trans")] = (bool) (value & (0x10<<0));
    rpFlags[F("max_ch_setpoint_trans")] = (bool) (value & (0x10<<1));

    rpFlags[F("dhw_setpoint_rw")] = (bool) (value & (1<<0));
    rpFlags[F("max_ch_setpoint_rw")] = (bool) (value & (1<<1));
}

bool OTValueRemoteParameter::sendDiscovery() {
    return true;
}