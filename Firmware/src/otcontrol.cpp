#include "otcontrol.h"
#include "command.h"

static OpenTherm otmaster(6, 7);
static OpenTherm otslave(10, 4, true);

OTControl otcontrol;

const char STR_STATUS[] PROGMEM = "status";
const char STR_SLAVE_CONFIG_MEMBER[] PROGMEM = "slave_config_member";
const char STR_MAX_REL_MOD[] PROGMEM = "max_rel_mode";
const char STR_REL_MOD[] PROGMEM = "rel_mod";
const char STR_CH_PRESSURE[] PROGMEM = "ch_pressure";
const char STR_DHW_FLOW_RATE[] PROGMEM = "dhw_flow_rate";
const char STR_BOILER_T[] PROGMEM = "boiler_t";
const char STR_CH_PUMP_STARTS[] PROGMEM = "ch_pump_starts";
const char STR_DHW_BURNER_STARTS[] PROGMEM = "dhw_burner_starts";
const char STR_DHW_T[] PROGMEM = "dhw_t";
const char STR_OUTSIDE_T[] PROGMEM = "outside_t";
const char STR_RETURN_T[] PROGMEM = "return_t";
const char STR_BOILER2_T[] PROGMEM = "boiler2_t";
const char STR_EXHAUST_T[] PROGMEM = "exhaust_t";
const char str_ot_slave_version[] PROGMEM = "ot_slave_version";
const char STR_SLAVE_PROD_VERSION[] PROGMEM = "slave_prod_version";
const char STR_BURNER_STARTS[] PROGMEM = "burner_starts";
const char STR_BURNER_OP_HOURS[] PROGMEM = "burner_op_hours";
const char STR_CH_PUMP_OP_HOURS[] PROGMEM = "ch_pump_op_hours";
const char STR_DHW_PUMP_VALUE_STARTS[] PROGMEM = "dhw_pump_valve_starts";

OTValue *boilerValues[] = { // data collected from boiler
    new OTValueStatus(),
    new OTValueSlaveConfigMember(),
    //new PTValueFloat(   STR_MAX_REL_MOD         MaxRelModLevelSetting,  0),
    new OTValueFloat(   STR_REL_MOD,                OpenThermMessageID::RelModLevel,            10),
    new OTValueFloat(   STR_CH_PRESSURE,            OpenThermMessageID::CHPressure,             10),
    new OTValueFloat(   STR_DHW_FLOW_RATE,          OpenThermMessageID::DHWFlowRate,            10),
    new OTValueFloat(   STR_BOILER_T,               OpenThermMessageID::Tboiler,                10),
    new OTValueFloat(   STR_DHW_T,                  OpenThermMessageID::Tdhw,                   10),
    new OTValueFloat(   STR_OUTSIDE_T,              OpenThermMessageID::Toutside,               10),
    new OTValueFloat(   STR_RETURN_T,               OpenThermMessageID::Tret,                   10),
    new OTValueFloat(   STR_BOILER2_T,              OpenThermMessageID::TflowCH2,               10),
    new OTValuei16(     STR_EXHAUST_T,              OpenThermMessageID::Texhaust,               10),
    new OTValueu16(     STR_BURNER_STARTS,          OpenThermMessageID::BurnerStarts,           10),
    new OTValueu16(     STR_CH_PUMP_STARTS,         OpenThermMessageID::CHPumpStarts,           10),
    new OTValueu16(     STR_DHW_PUMP_VALUE_STARTS,  OpenThermMessageID::DHWPumpValveStarts,     10),
    new OTValueu16(     STR_DHW_BURNER_STARTS,      OpenThermMessageID::DHWBurnerStarts,        60),
    new OTValueu16(     STR_BURNER_OP_HOURS,        OpenThermMessageID::BurnerOperationHours,   60),
    new OTValueu16(     STR_CH_PUMP_OP_HOURS,       OpenThermMessageID::CHPumpOperationHours,   60),
    //new OTValueu16("dhw_pumpt_valve_op_hours", DHWPumpValveOperationHours, 60),
    //new OTValueu16("dhw_burner_op_hours", DHWBurnerOperationHours, 60),
    //new OTValueFloat(str_max_rel_mod, MaxRelModLevelSetting, 0),
    ////new OTValue("max_boiler_cap_min_rel_mod", MaxCapacityMinModLevel, 0),
    //new OTValueFloat(str_ot_slave_version, OpenThermVersionSlave, 0),
    new OTValueFloat(   STR_SLAVE_PROD_VERSION,     OpenThermMessageID::SlaveVersion,           0),
};


OTValue::OTValue(const char *name, const OpenThermMessageID id, const unsigned int interval):
        id(id),
        interval(interval),
        name(name),
        value(0),
        lastStatus(OpenThermResponseStatus::NONE) {
}


bool OTValue::process() {
    switch (lastStatus) {
    case OpenThermResponseStatus::INVALID:
        return false;
    
    case OpenThermResponseStatus::SUCCESS:
        if (interval == 0)
            return false;

        if ((millis() - lastTransfer) / 1000 < interval)
            return false;

        break;
    
    default: 
        break;
    }

    unsigned long request = otmaster.buildRequest(OpenThermMessageType::READ_DATA, id, value);
    command.sendOtEvent('T', request);
    otmaster.sendRequestAync(request);
    lastTransfer = millis();
    return true;
}

OpenThermMessageID OTValue::getId() const {
    return id;
}

void OTValue::setValue(uint16_t val) {
    lastStatus = OpenThermResponseStatus::SUCCESS;
    value = val;
}

const char *OTValue::getName() const {
    return name;
}

void OTValue::setStatus(OpenThermResponseStatus status) {
    lastStatus = status;
}

void OTValue::getJson(JsonObject &obj) const {
    switch (lastStatus) {
    case OpenThermResponseStatus::SUCCESS:
        getValue(obj);
        break;
    case OpenThermResponseStatus::TIMEOUT:
        obj[FPSTR(name)] = (char*) NULL;
        break;
    default:
        break;
    }        
}


OTValueu16::OTValueu16(const char *name, const OpenThermMessageID id, const unsigned int interval):
        OTValue(name, id, interval) {
}

uint16_t OTValueu16::getValue() const {
    return value;
}

void OTValueu16::getValue(JsonObject &obj) const {
    obj[FPSTR(name)] = getValue();
}


OTValuei16::OTValuei16(const char *name, const OpenThermMessageID id, const unsigned int interval):
        OTValue(name, id, interval) {
}

int16_t OTValuei16::getValue() const {
    return (int16_t) value;
}

void OTValuei16::getValue(JsonObject &obj) const {
    obj[FPSTR(name)] = getValue();
}


OTValueFloat::OTValueFloat(const char *name, const OpenThermMessageID id, const unsigned int interval):
        OTValue(name, id, interval) {
}

double OTValueFloat::getValue() const {
    int8_t i = value >> 8;
    if (i >= 0)
        return round((i + (value & 0xFF) / 256.0) * 10) / 10.0;
    else
        return round((i - (value & 0xFF) / 256.0) * 10) / 10.0;
}

void OTValueFloat::getValue(JsonObject &obj) const {
    obj[FPSTR(name)] = getValue();
}


OTValueStatus::OTValueStatus():
        OTValue(STR_STATUS, OpenThermMessageID::Status, 0) {
}

void OTValueStatus::getValue(JsonObject &obj) const {
    JsonObject slaveStatus = obj[F("slaveStatus")].to<JsonObject>();
    slaveStatus[F("fault")] = (bool) (value & (1<<0));
    slaveStatus[F("ch_mode")] = (bool) (value & (1<<1));
    slaveStatus[F("dhw_mode")] = (bool) (value & (1<<2));
    slaveStatus[F("flame")] = (bool) (value & (1<<3));
    slaveStatus[F("cooling")] = (bool) (value & (1<<4));
    slaveStatus[F("ch2_mode")] = (bool) (value & (1<<5));
    slaveStatus[F("diagnostic")] = (bool) (value & (1<<6));

    JsonObject masterStatus = obj[F("masterStatus")].to<JsonObject>();
    masterStatus[F("ch_enable")] = (bool) (value & (1<<8));
    masterStatus[F("dhw_enable")] = (bool) (value & (1<<9));
    masterStatus[F("cooling_enable")] = (bool) (value & (1<<10));
    masterStatus[F("otc_active")] = (bool) (value & (1<<11));
    masterStatus[F("ch2_enable")] = (bool) (value & (1<<12));
}

OTValueSlaveConfigMember::OTValueSlaveConfigMember():
        OTValue(STR_SLAVE_CONFIG_MEMBER, OpenThermMessageID::SConfigSMemberIDcode, 0) {
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


void IRAM_ATTR handleIrqMaster() {
    otmaster.handleInterrupt();
}

void IRAM_ATTR handleIrqSlave() {
    otslave.handleInterrupt();
}

//OpenTherm master callback
void otCbMaster(unsigned long response, OpenThermResponseStatus status) {
    otcontrol.OnRxMaster(response, status);
}

//OpenTherm slave callback
void otCbSlave(unsigned long response, OpenThermResponseStatus status) {
    otcontrol.OnRxSlave(response, status);
}

OTControl::OTControl():
        lastMillis(millis()),
        otMode(OTMODE_LOOPBACK),
        loopbackData(0) {
}

void OTControl::begin() {
    // +24V enable for room unit (OT slave)
    pinMode(5, OUTPUT);
    digitalWrite(5, HIGH);

    // relay on
    pinMode(20, OUTPUT);
    digitalWrite(20, HIGH);

    otmaster.begin(handleIrqMaster, otCbMaster);
    otslave.begin(handleIrqSlave, otCbSlave);
}

void OTControl::loop() {
    otmaster.process();
    otslave.process();

    if (!otmaster.isReady())
        return;

    switch (otMode) {
    case OTMODE_MASTER:
        for (auto *valobj: boilerValues) {
            if (valobj->process())
                return;
        }

        if (millis() > lastMillis + 1000) {
            lastMillis = millis();
            uint32_t req = otmaster.buildSetBoilerStatusRequest(true, true, false, false, false);
            otmaster.sendRequestAync(req);
            command.sendOtEvent('T', req);
            return;
        }

        static uint32_t lastBoilerTemp = -10000;
        if (millis() > lastBoilerTemp + 10000) {
            lastBoilerTemp = millis();
            uint32_t req = otmaster.buildSetBoilerTemperatureRequest(50);
            otmaster.sendRequestAync(req);
            command.sendOtEvent('T', req);
            return;
        }

        static uint32_t lastDHWSet = -10000;
        if (millis() > lastDHWSet + 10000) {
            lastDHWSet = millis();
            unsigned int data = OpenTherm::temperatureToData(50);
            uint32_t req = otmaster.buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet, data); //otmaster.setDHWSetpoint(50);
            otmaster.sendRequestAync(req);
            command.sendOtEvent('T', req);
            return;
        }
        break;

    case OTMODE_LOOPBACK:
        if (millis() > lastMillis + 1000) {
            lastMillis = millis();
            uint32_t req = otmaster.buildRequest(OpenThermMessageType::READ, OpenThermMessageID::Status, loopbackData++);
            otmaster.sendRequestAync(req);
        }
        break;
    }
}

void OTControl::OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status) {
    // received response from boiler
    auto id = OpenTherm::getDataID(msg);
    OTValue *otval = nullptr;
    for (auto *valobj: boilerValues) {
        if (valobj->getId() == id) {
            otval = valobj;
            break;
        }
    }

    command.sendOtEvent('B', msg);
    Serial.println((int) status);

    switch (status) {
    case OpenThermResponseStatus::SUCCESS: {
        if (otval)
            otval->setValue(msg & 0xFFFF);
        break;
    }

    case OpenThermResponseStatus::INVALID: {
        if (otval)
            otval->setStatus(OpenThermResponseStatus::INVALID);
        break;
    }
    default:
        break;
    }
}

void OTControl::OnRxSlave(const unsigned long msg, const OpenThermResponseStatus status) {
    command.sendOtEvent('T', msg);

    switch (otMode) {
    case OTMODE_GATEWAY:
        // we received a request from the room unit, forward it to boiler
        otmaster.sendRequestAync(msg);
        break;

    case OTMODE_LOOPBACK: {
        auto id = OpenTherm::getDataID(msg);
        uint32_t reply = otslave.buildResponse(OpenThermMessageType::READ_ACK, id, msg & 0xFFFF);
        otslave.sendResponse(reply);
        break;
    }

    default:
        break;
    }
}

void OTControl::getJson(JsonObject &obj) const {
    JsonObject slave = obj[F("slave")].to<JsonObject>();
    for (auto *valobj: boilerValues)
        valobj->getJson(slave);

    static bool slaveConnected = false;
    switch (otmaster.getLastResponseStatus()) {
    case OpenThermResponseStatus::SUCCESS:
    case OpenThermResponseStatus::INVALID:
        slaveConnected = true;
        break;
    case OpenThermResponseStatus::TIMEOUT:
        slaveConnected = false;
    default:
        break;
    }
    slave[F("connected")] = slaveConnected;
}

void OTControl::setChCtrlConfig(ChControlConfig &config) {

}