#include "otcontrol.h"
#include "command.h"

static OpenTherm otmaster(4, 5);
static OpenTherm otslave(12, 13, true);

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
const char str_slave_prod_version[] PROGMEM = "slave_prod_version";
const char STR_BURNER_STARTS[] PROGMEM = "burner_starts";
const char STR_BURNER_OP_HOURS[] PROGMEM = "burner_op_hours";
const char STR_CH_PUMP_OP_HOURS[] PROGMEM = "ch_pump_op_hours";

OTValue *otSlaveValues[] = { // data collected from boiler
    new OTValueStatus(),
    new OTValueSlaveConfigMember(),
    //new PTValueFloat(   STR_MAX_REL_MOD         MaxRelModLevelSetting,  0),
    new OTValueFloat(   STR_REL_MOD,            RelModLevel,            10),
    new OTValueFloat(   STR_CH_PRESSURE,        CHPressure,             10),
    new OTValueFloat(   STR_DHW_FLOW_RATE,      DHWFlowRate,            10),
    new OTValueFloat(   STR_BOILER_T,           Tboiler,                10),
    new OTValueFloat(   STR_DHW_T,              Tdhw,                   10),
    new OTValueFloat(   STR_OUTSIDE_T,          Toutside,               10),
    new OTValueFloat(   STR_RETURN_T,           Tret,                   10),
    new OTValueFloat(   STR_BOILER2_T,          TflowCH2,               10),
    new OTValuei16(     STR_EXHAUST_T,          Texhaust,               10),
    new OTValueu16(     STR_BURNER_STARTS,      BurnerStarts,           10),
    new OTValueu16(     STR_CH_PUMP_STARTS,     CHPumpStarts,           10),
    //new OTValueu16("dhw_pump_valve_starts", DHWPumpValveStarts, 10),
    new OTValueu16(     STR_DHW_BURNER_STARTS,  DHWBurnerStarts,        60),
    new OTValueu16(     STR_BURNER_OP_HOURS,    BurnerOperationHours,   60),
    new OTValueu16(     STR_CH_PUMP_OP_HOURS,   CHPumpOperationHours,   60),
    //new OTValueu16("dhw_pumpt_valve_op_hours", DHWPumpValveOperationHours, 60),
    //new OTValueu16("dhw_burner_op_hours", DHWBurnerOperationHours, 60),
    //new OTValueFloat(str_max_rel_mod, MaxRelModLevelSetting, 0),
    ////new OTValue("max_boiler_cap_min_rel_mod", MaxCapacityMinModLevel, 0),
    //new OTValueFloat(str_ot_slave_version, OpenThermVersionSlave, 0),
    //new OTValueFloat(str_slave_prod_version, SlaveVersion, 0),
};


OTValue::OTValue(const char *name, const OpenThermMessageID id, const unsigned int interval):
        id(id),
        interval(interval),
        name(name),
        lastStatus(NONE) {
}


bool OTValue::process() {
    switch (lastStatus) {
    case INVALID:
        return false;
    
    case SUCCESS:
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
    lastStatus = SUCCESS;
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
    case SUCCESS:
        getValue(obj);
        break;
    case TIMEOUT:
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
        OTValue(STR_STATUS, Status, 0) {
}

void OTValueStatus::getValue(JsonObject &obj) const {
    JsonObject slaveStatus = obj.createNestedObject(F("slaveStatus"));
    slaveStatus[F("fault")] = (bool) (value & (1<<0));
    slaveStatus[F("ch_mode")] = (bool) (value & (1<<1));
    slaveStatus[F("dhw_mode")] = (bool) (value & (1<<2));
    slaveStatus[F("flame")] = (bool) (value & (1<<3));
    slaveStatus[F("cooling")] = (bool) (value & (1<<4));
    slaveStatus[F("ch2_mode")] = (bool) (value & (1<<5));
    slaveStatus[F("diagnostic")] = (bool) (value & (1<<6));

    JsonObject masterStatus = obj.createNestedObject(F("masterStatus"));
    masterStatus[F("ch_enable")] = (bool) (value & (1<<8));
    masterStatus[F("dhw_enable")] = (bool) (value & (1<<9));
    masterStatus[F("cooling_enable")] = (bool) (value & (1<<10));
    masterStatus[F("otc_active")] = (bool) (value & (1<<11));
    masterStatus[F("ch2_enable")] = (bool) (value & (1<<12));
}

OTValueSlaveConfigMember::OTValueSlaveConfigMember():
        OTValue(STR_SLAVE_CONFIG_MEMBER, SConfigSMemberIDcode, 0) {
}

void OTValueSlaveConfigMember::getValue(JsonObject &obj) const {
    JsonObject slaveConfig = obj.createNestedObject(F("slaveConfig"));
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
    // received response from boiler
    switch (status) {
    case SUCCESS: {
        command.sendOtEvent('B', response);
        OpenThermMessageID id = (OpenThermMessageID) ((response >> 16) & 0xFF);
        for (auto &valobj: otSlaveValues) {
            if (valobj->getId() == id) {
                valobj->setValue(response & 0xFFFF);
                break;
            }
        }
        break;
    }

    case INVALID: {
        OpenThermMessageID id = (OpenThermMessageID) ((response >> 16) & 0xFF);
        for (auto &valobj: otSlaveValues) {
            if (valobj->getId() == id) {
                valobj->setStatus(status);
                break;
            }
        }
        break;
    }
    default:
        break;
    }
}

//OpenTherm slave callback
void otCbSlave(unsigned long response, OpenThermResponseStatus status) {

}

OTControl::OTControl():
        lastMillis(millis()) {
}

void OTControl::begin() {
    otmaster.begin(handleIrqMaster, otCbMaster);

    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);

    //otslave.begin(handleIrqSlave, otCbSlave);
}

void OTControl::loop() {
    otmaster.process();
    //otslave.process();

    if (millis() > lastMillis + 1000) {
        if (otmaster.isReady()) {
            lastMillis = millis();
            bool idle = true;
            for (auto &valobj: otSlaveValues) {
                if (valobj->process()) {
                    idle = false;
                    break;
                }
            }
            if (idle) {
                uint32_t req = otmaster.setBoilerStatus(true, true, false, false, false);
                command.sendOtEvent('T', req);
            }
        }
    }

    static uint32_t lastBoilerTemp = -10000;
    if (millis() > lastBoilerTemp + 10000) {
        if (otmaster.isReady()) {
            lastBoilerTemp = millis();
            otmaster.setBoilerTemperature(54);
        }
    }

    static uint32_t lastDHWSet = -10000;
    if (millis() > lastDHWSet + 10000) {
        if (otmaster.isReady()) {
            lastDHWSet = millis();
            otmaster.setDHWSetpoint(50);
        }
    }
}

void OTControl::getJson(JsonObject &obj) const {
    bool slaveConnected = false;
    switch (otmaster.getLastResponseStatus()) {
    case SUCCESS:
    case INVALID:
        slaveConnected = true;
        break;
    default:
        break;
    }
    obj[F("slaveConnected")] = slaveConnected;

    JsonObject sv = obj.createNestedObject(F("slaveValues"));
    for (auto &valobj: otSlaveValues)
        valobj->getJson(sv);
}

void OTControl::setChCtrlConfig(ChControlConfig &config) {

}