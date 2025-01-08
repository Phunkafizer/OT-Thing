#include "otcontrol.h"
#include "command.h"
#include "HADiscLocal.h"
#include "mqtt.h"

static OpenTherm otmaster(3, 1);
static OpenTherm otslave(6, 7, true);

OTControl otcontrol;

const char ID_STR_STATUS[] PROGMEM = "status";
const char ID_STR_SLAVE_CONFIG_MEMBER[] PROGMEM = "slave_config_member";
const char ID_STR_REL_MOD[] PROGMEM = "rel_mod";
const char ID_STR_CH_PRESSURE[] PROGMEM = "ch_pressure";
const char ID_STR_FLOW_T[] PROGMEM = "flow_t";
const char ID_STR_FLOW_T2[] PROGMEM = "flow_t2";
const char ID_STR_DHW_T[] PROGMEM = "dhw_t";
const char ID_STR_DHW_T2[] PROGMEM = "dhw_t2";
const char ID_STR_CONTROL_SETPOINT[] PROGMEM = "t_set";
const char ID_STR_ROOM_T[] PROGMEM = "room_t";
const char ID_STR_DHW_FLOW_RATE[] PROGMEM = "dhw_flow_rate";
const char ID_STR_EXHAUST_T[] PROGMEM = "exhaust_t";
const char ID_STR_BURNER_STARTS[] PROGMEM = "burner_starts";
const char ID_STR_BURNER_OP_HOURS[] PROGMEM = "burner_op_hours";
const char ID_STR_RETURN_T[] PROGMEM = "return_t";
const char ID_STR_CH_PUMP_STARTS[] PROGMEM = "ch_pump_starts";
const char ID_STR_SLAVE_PROD_VERSION[] PROGMEM = "slave_prod_version";

const char STR_MAX_REL_MOD[] PROGMEM = "max_rel_mode";
const char STR_DHW_BURNER_STARTS[] PROGMEM = "dhw_burner_starts";
const char STR_OUTSIDE_T[] PROGMEM = "outside_t";
const char str_ot_slave_version[] PROGMEM = "ot_slave_version";
const char STR_SLAVE_PROD_VERSION[] PROGMEM = "slave_prod_version";
const char STR_CH_PUMP_OP_HOURS[] PROGMEM = "ch_pump_op_hours";
const char STR_DHW_PUMP_VALUE_STARTS[] PROGMEM = "dhw_pump_valve_starts";

OTValue *boilerValues[] = { // data collected from boiler
    new OTValueStatus(),
    new OTValueSlaveConfigMember(),
    //new PTValueFloat(   STR_MAX_REL_MOD         MaxRelModLevelSetting,  0),
    new OTValueFloat(           ID_STR_REL_MOD,                 OpenThermMessageID::RelModLevel,            10),
    new OTValueFloat(           ID_STR_CH_PRESSURE,             OpenThermMessageID::CHPressure,             30),
    new OTValueFloat(           ID_STR_DHW_FLOW_RATE,           OpenThermMessageID::DHWFlowRate,            10),
    new OTValueFloat(           ID_STR_FLOW_T,                  OpenThermMessageID::Tboiler,                10),
    new OTValueFloat(           ID_STR_FLOW_T2,                 OpenThermMessageID::TflowCH2,               10),
    new OTValueFloat(           ID_STR_DHW_T,                   OpenThermMessageID::Tdhw,                   10),
    new OTValueFloat(           ID_STR_DHW_T2,                  OpenThermMessageID::Tdhw2,                  10),
    //new OTValueFloat(   STR_OUTSIDE_T,              OpenThermMessageID::Toutside,               10),
    new OTValueFloat(           ID_STR_RETURN_T,                OpenThermMessageID::Tret,                   10),
    new OTValuei16(             ID_STR_EXHAUST_T,               OpenThermMessageID::Texhaust,               10),
    new OTValueu16(             ID_STR_BURNER_STARTS,           OpenThermMessageID::SuccessfulBurnerStarts, 30),
    new OTValueu16(             ID_STR_CH_PUMP_STARTS,          OpenThermMessageID::CHPumpStarts,           30),
    //new OTValueu16(     STR_DHW_PUMP_VALUE_STARTS,  OpenThermMessageID::DHWPumpValveStarts,     10),
    //new OTValueu16(     STR_DHW_BURNER_STARTS,      OpenThermMessageID::DHWBurnerStarts,        60),
    new OTValueu16(             ID_STR_BURNER_OP_HOURS,         OpenThermMessageID::BurnerOperationHours,   120),
    new OTValueProductVersion(  ID_STR_SLAVE_PROD_VERSION,      OpenThermMessageID::SlaveVersion)
    //new OTValueu16(     STR_CH_PUMP_OP_HOURS,       OpenThermMessageID::CHPumpOperationHours,   60),
    //new OTValueu16("dhw_pumpt_valve_op_hours", DHWPumpValveOperationHours, 60),
    //new OTValueu16("dhw_burner_op_hours", DHWBurnerOperationHours, 60),
    //new OTValueFloat(str_max_rel_mod, MaxRelModLevelSetting, 0),
    ////new OTValue("max_boiler_cap_min_rel_mod", MaxCapacityMinModLevel, 0),
    //new OTValueFloat(str_ot_slave_version, OpenThermVersionSlave, 0),
    //new OTValueFloat(   STR_SLAVE_PROD_VERSION,     OpenThermMessageID::SlaveVersion,           0),
};

OTValue *thermostatValues[] = {
    new OTValueFloat(       ID_STR_CONTROL_SETPOINT,        OpenThermMessageID::TSet,                   0),
    new OTValueFloat(       ID_STR_ROOM_T,                  OpenThermMessageID::Tr,                     0)
};


OTValue::OTValue(const char *name, const OpenThermMessageID id, const unsigned int interval):
        id(id),
        interval(interval),
        name(name),
        value(0),
        enabled(true),
        isSet(false),
        discFlag(false) {
}

bool OTValue::process() {
    if (!enabled)
        return false;

    if (isSet && (interval == 0))
        return false;

    if ((lastTransfer > 0) && ((millis() - lastTransfer) / 1000 < interval))
        return false;

    unsigned long request = otmaster.buildRequest(OpenThermMessageType::READ_DATA, id, value);
    command.sendOtEvent('T', request);
    otmaster.sendRequestAsync(request);
    lastTransfer = millis();
    return true;
}

OpenThermMessageID OTValue::getId() const {
    return id;
}

bool OTValue::sendDiscovery() {
    switch (id) {
        case OpenThermMessageID::Tboiler:
            haDisc.createTempSensor(F("Boiler"), FPSTR(name));
            break;

        case OpenThermMessageID::RelModLevel:
            haDisc.createPowerFactorSensor(F("Rel. Modulation"), FPSTR(name));
            break;

        case OpenThermMessageID::Tdhw:
            haDisc.createTempSensor(F("Brauchwasser"), FPSTR(name));
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

        default:
            return false;
    }

    String valTempl = F("{{ value_json.boiler.# }}");
    valTempl.replace("#", FPSTR(name));
    haDisc.setValueTemplate(valTempl);
        
    return mqtt.publish(haDisc.topic, haDisc.doc);
}

void OTValue::setValue(uint16_t val) {
    value = val;
    isSet = true;
    enabled = true;

    if (!discFlag) {
        discFlag = sendDiscovery();
    }
}

void OTValue::enable(const bool enabled) {
    this->enabled = enabled;
}

const char *OTValue::getName() const {
    return name;
}

void OTValue::getJson(JsonObject &obj) const {
    if (!enabled)
        return;

    if (!isSet)
        obj[FPSTR(name)] = (char*) NULL;
    else
        getValue(obj);        
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
        OTValue(ID_STR_STATUS, OpenThermMessageID::Status, 0) {
    enabled = false;
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

    /*JsonObject masterStatus = obj[F("masterStatus")].to<JsonObject>();
    masterStatus[F("ch_enable")] = (bool) (value & (1<<8));
    masterStatus[F("dhw_enable")] = (bool) (value & (1<<9));
    masterStatus[F("cooling_enable")] = (bool) (value & (1<<10));
    masterStatus[F("otc_active")] = (bool) (value & (1<<11));
    masterStatus[F("ch2_enable")] = (bool) (value & (1<<12));*/
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

OTValueSlaveConfigMember::OTValueSlaveConfigMember():
        OTValue(ID_STR_SLAVE_CONFIG_MEMBER, OpenThermMessageID::SConfigSMemberIDcode, 0) {
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

OTValueProductVersion::OTValueProductVersion(const char *name, const OpenThermMessageID id):
        OTValue(name, id, 0) {

}

void OTValueProductVersion::getValue(JsonObject &obj) const {
    String v = String(value >> 8);
    v += '.';
    v += String(value & 0xFF);
    obj[FPSTR(name)] = v;
}


void IRAM_ATTR handleIrqMaster() {
    otmaster.handleInterrupt();
}

void IRAM_ATTR handleIrqSlave() {
    otslave.handleInterrupt();
}

void IRAM_ATTR handleTimerIrqMaster() {
    otmaster.handleTimerIrq();
}

void IRAM_ATTR handleTimerIrqSlave() {
    otslave.handleTimerIrq();
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
        otMode(OTMODE_MASTER),
        loopbackData(0) {
}

void OTControl::begin() {
    // +24V enable for room unit (OT slave)
    pinMode(10, OUTPUT);

    // relay
    pinMode(20, OUTPUT);

    setMode(otMode);

    otmaster.begin(handleIrqMaster, otCbMaster, 1, handleTimerIrqMaster);
    otslave.begin(handleIrqSlave, otCbSlave, 0, handleTimerIrqSlave);
}

void OTControl::setMode(const OTMode mode) {
    otMode = mode;

    // set bypass rely
    digitalWrite(20, mode != OTMODE_BYPASS);

    // set +24V
    digitalWrite(10, (mode == OTMODE_GATEWAY) || (mode == OTMODE_LOOPBACKTEST) || true); // TODO temp for boiler emu

    for (auto *valobj: boilerValues)
        valobj->enable(mode == OTMODE_MASTER);
}

void OTControl::loop() {
    otmaster.process();
    otslave.process();

    if (millis() > lastRxMaster + 100)
        digitalWrite(2, HIGH);

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
            otmaster.sendRequestAsync(req);
            command.sendOtEvent('T', req);
            return;
        }

        static uint32_t lastBoilerTemp = -10000;
        if (millis() > lastBoilerTemp + 10000) {
            lastBoilerTemp = millis();
            uint32_t req = otmaster.buildSetBoilerTemperatureRequest(54);
            otmaster.sendRequestAsync(req);
            command.sendOtEvent('T', req);
            return;
        }

        static uint32_t lastDHWSet = -10000;
        if (millis() > lastDHWSet + 10000) {
            lastDHWSet = millis();
            unsigned int data = OpenTherm::temperatureToData(50);
            uint32_t req = otmaster.buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet, data); //otmaster.setDHWSetpoint(50);
            otmaster.sendRequestAsync(req);
            command.sendOtEvent('T', req);
            return;
        }
        break;

    case OTMODE_LOOPBACKTEST:
        if (millis() > lastMillis + 500) {
            lastMillis = millis();
            uint32_t req = otmaster.buildRequest(OpenThermMessageType::READ, OpenThermMessageID::Status, ++loopbackData);
            otmaster.sendRequestAsync(req);
        }
        break;
    }
}

void OTControl::OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status) {
    if (status == OpenThermResponseStatus::TIMEOUT)
        return;

    digitalWrite(2, LOW);
    lastRxMaster = millis();


    // received response from boiler
    auto id = OpenTherm::getDataID(msg);
    OTValue *otval = nullptr;
    for (auto *valobj: boilerValues) {
        if (valobj->getId() == id) {
            otval = valobj;
            break;
        }
    }

    unsigned long newMsg = msg;

    switch (otMode) {
    case OTMODE_LOOPBACKTEST:
        Serial.print("Master RX ");
        Serial.println((int) status);
        command.sendOtEvent('B', msg);
        if (loopbackData == (msg & 0xFFFF)) {
            Serial.println("LOOPBACK CHECK PASS");
        }
        else
            Serial.println("LOOPBACK CHECK FAIL!");
        return;

    case OTMODE_GATEWAY:
        // forward reply from boiler to room unit
        // TODO manipulate some frames, e. g. outsidetemp 
        switch (id) {
        case OpenThermMessageID::Toutside:
            newMsg = otmaster.buildRequest(OpenThermMessageType::READ_ACK, id, 0 << 8 | 127);
            break;
        }
        otslave.sendResponse(newMsg);
    }

    switch (status) {
    case OpenThermResponseStatus::SUCCESS: {
        if (newMsg == msg)
            command.sendOtEvent('B', msg);
        if (otval)
            otval->setValue(newMsg & 0xFFFF);
        break;
    }

    case OpenThermResponseStatus::INVALID: {
        if (newMsg == msg)
            command.sendOtEvent('E', msg);
        if (otval) {
            OpenThermMessageType mt = OpenTherm::getMessageType(msg);
            if (mt >= OpenThermMessageType::DATA_INVALID)
                otval->enable(false);
        }
        break;
    }
    default:
        break;
    }
    if (msg != newMsg)
        command.sendOtEvent('A', newMsg);
}

void OTControl::OnRxSlave(const unsigned long msg, const OpenThermResponseStatus status) {
    // received request from thermostat
    OpenThermMessageID id = OpenTherm::getDataID(msg);

    OTValue *otval = nullptr;
    for (auto *valobj: thermostatValues) {
        if (valobj->getId() == id) {
            otval = valobj;
            break;
        }
    }

    switch (otMode) {
    case OTMODE_MASTER: {
        // TODO temporary, boiler emulation for tests!
        /*unsigned long response;
        switch (id) {
        case OpenThermMessageID::Tboiler:
            response = otslave.buildResponse(OpenThermMessageType::READ_ACK, id, 45 << 8);
            otslave.status = OpenThermStatus::READY;
            otslave.sendResponse(response);
            break;

        case OpenThermMessageID::RelModLevel:
            response = otslave.buildResponse(OpenThermMessageType::READ_ACK, id, 33 << 8);
            otslave.status = OpenThermStatus::READY;
            otslave.sendResponse(response);
            break;
        }*/

        break;
    }

    case OTMODE_GATEWAY: {
        // we received a request from the room unit, forward it to boiler
        // TODO check for special commands, e. g. outside temp

        unsigned long newMsg = msg;
        switch (id) {
        case OpenThermMessageID::TSet:
            newMsg = otmaster.buildRequest(OpenThermMessageType::WRITE_DATA, id, 57 << 8);
            break;

        case OpenThermMessageID::Status:
            newMsg = otmaster.buildRequest(OpenThermMessageType::READ_DATA, id, msg & 0xFFFF | 0x100<<3); // activate OTC
            break;
        }

        if (msg != newMsg)
            command.sendOtEvent('R', newMsg);
        else
            command.sendOtEvent('T', msg);

        otmaster.sendRequestAsync(newMsg);

        if (otval)
            otval->setValue(newMsg & 0xFFFF);
        break;
    }

    case OTMODE_LOOPBACKTEST: {
        auto id = OpenTherm::getDataID(msg);
        uint32_t reply = otslave.buildResponse(OpenThermMessageType::READ_ACK, id, msg & 0xFFFF);
        otslave.status = OpenThermStatus::READY;
        otslave.sendResponse(reply);
        break;
    }

    default:
        break;
    }
}

void OTControl::getJson(JsonObject &obj) const {
    JsonObject boiler = obj[F("boiler")].to<JsonObject>();
    for (auto *valobj: boilerValues)
        valobj->getJson(boiler);

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
    boiler[F("connected")] = slaveConnected;


    JsonObject thermostat = obj[F("thermostat")].to<JsonObject>();
    for (auto *valobj: thermostatValues)
        valobj->getJson(thermostat);

}

void OTControl::setChCtrlConfig(ChControlConfig &config) {

}