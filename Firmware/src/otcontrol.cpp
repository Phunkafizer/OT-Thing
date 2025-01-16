#include "otcontrol.h"
#include "command.h"
#include "HADiscLocal.h"
#include "mqtt.h"
#include "hwdef.h"
#include "outsidetemp.h"
#include "portal.h"

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
const char ID_STR_ROOM_T[] PROGMEM = "room_t";
const char ID_STR_DHW_FLOW_RATE[] PROGMEM = "dhw_flow_rate";
const char ID_STR_EXHAUST_T[] PROGMEM = "exhaust_t";
const char ID_STR_BURNER_STARTS[] PROGMEM = "burner_starts";
const char ID_STR_BURNER_OP_HOURS[] PROGMEM = "burner_op_hours";
const char ID_STR_RETURN_T[] PROGMEM = "return_t";
const char ID_STR_CH_PUMP_STARTS[] PROGMEM = "ch_pump_starts";
const char ID_STR_SLAVE_PROD_VERSION[] PROGMEM = "slave_prod_version";
const char ID_STR_DHW_SET_T[] PROGMEM = "dhw_set_t";
const char ID_STR_CH_SET_T[] PROGMEM = "ch_set_t";
const char ID_STR_CAPACITY_MODULATION[] PROGMEM = "cap_mod";

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
    new OTValueProductVersion(  ID_STR_SLAVE_PROD_VERSION,      OpenThermMessageID::SlaveVersion),
    new OTValueFloat(           ID_STR_DHW_SET_T,               OpenThermMessageID::TdhwSet,                -1),
    new OTValueFloat(           ID_STR_CH_SET_T,                OpenThermMessageID::TSet,                   -1),
    new OTValueCapacityModulation()
    //new OTValueu16(     STR_CH_PUMP_OP_HOURS,       OpenThermMessageID::CHPumpOperationHours,   60),
    //new OTValueu16("dhw_pumpt_valve_op_hours", DHWPumpValveOperationHours, 60),
    //new OTValueu16("dhw_burner_op_hours", DHWBurnerOperationHours, 60),
    //new OTValueFloat(str_max_rel_mod, MaxRelModLevelSetting, 0),
    ////new OTValue("max_boiler_cap_min_rel_mod", MaxCapacityMinModLevel, 0),
    //new OTValueFloat(str_ot_slave_version, OpenThermVersionSlave, 0),
};

OTValue *thermostatValues[] = {
    new OTValueFloat(       ID_STR_CH_SET_T,        OpenThermMessageID::TSet,                   0),
    new OTValueFloat(       ID_STR_ROOM_T,                  OpenThermMessageID::Tr,                     0)
};

constexpr uint16_t floatToOT(double f) {
    return (((int) f) << 8) | (int) ((f - (int) f) * 256);
}

const struct {
    OpenThermMessageID id;
    uint16_t value;
} loopbackTestData[] PROGMEM = {
    {OpenThermMessageID::Status,                    0x0000},
    {OpenThermMessageID::Tboiler,                   floatToOT(48.5)},
    {OpenThermMessageID::Texhaust,                  96},
    {OpenThermMessageID::RelModLevel,               floatToOT(33.3)},
    {OpenThermMessageID::CHPressure,                floatToOT(1.25)},
    {OpenThermMessageID::DHWFlowRate,               floatToOT(2.4)},
    {OpenThermMessageID::Tret,                      floatToOT(41.7)},
    {OpenThermMessageID::Tdhw,                      floatToOT(37.5)},
    {OpenThermMessageID::SuccessfulBurnerStarts,    9999},
    {OpenThermMessageID::BurnerOperationHours,      8888}
};

/**
 * @param interval -1: never query. 0: only query once. >0: query every interval seconds
 */
OTValue::OTValue(const char *name, const OpenThermMessageID id, const int interval):
        id(id),
        interval(interval),
        name(name),
        value(0),
        enabled(interval != -1),
        isSet(false),
        discFlag(false) {
}

bool OTValue::process() {
    if (!enabled || (interval == -1))
        return false;

    if (isSet && (interval == 0))
        return false;

    if ((lastTransfer > 0) && ((millis() - lastTransfer) / 1000 < interval))
        return false;

    unsigned long request = otmaster.buildRequest(OpenThermMessageType::READ_DATA, id, value);
    otcontrol.master.sendRequest('T', request);
    lastTransfer = millis();
    return true;
}

OpenThermMessageID OTValue::getId() const {
    return id;
}

bool OTValue::sendDiscovery() {
    switch (id) {
        case OpenThermMessageID::Tboiler:
            haDisc.createTempSensor(F("Vorlauftemperatur"), FPSTR(name));
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

        case OpenThermMessageID::TSet:
            haDisc.createTempSensor(F("Vorlaufsolltemperatur"), FPSTR(name));
            break;

        // TODO
        case OpenThermMessageID::DHWFlowRate:
        case OpenThermMessageID::TflowCH2:
        case OpenThermMessageID::Tdhw2:
        case OpenThermMessageID::Tret:
        case OpenThermMessageID::Texhaust:
        case OpenThermMessageID::CHPumpStarts:
        case OpenThermMessageID::SlaveVersion:
        case OpenThermMessageID::TdhwSet:
            return false;

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

void OTValue::disable() {
    enabled = false;
    isSet = false;
}

void OTValue::init(const bool enabled) {
    this->enabled = enabled;
    isSet = false;
}

const char *OTValue::getName() const {
    return name;
}

void OTValue::getJson(JsonObject &obj) const {
    if (enabled) {
        if (isSet)
            getValue(obj);        
        else
            obj[FPSTR(name)] = (char*) NULL;
    }
}

OTValueu16::OTValueu16(const char *name, const OpenThermMessageID id, const int interval):
        OTValue(name, id, interval) {
}

uint16_t OTValueu16::getValue() const {
    return value;
}

void OTValueu16::getValue(JsonObject &obj) const {
    obj[FPSTR(name)] = getValue();
}


OTValuei16::OTValuei16(const char *name, const OpenThermMessageID id, const int interval):
        OTValue(name, id, interval) {
}

int16_t OTValuei16::getValue() const {
    return (int16_t) value;
}

void OTValuei16::getValue(JsonObject &obj) const {
    obj[FPSTR(name)] = getValue();
}


OTValueFloat::OTValueFloat(const char *name, const OpenThermMessageID id, const int interval):
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
        OTValue(ID_STR_STATUS, OpenThermMessageID::Status, -1) {
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

OTValueCapacityModulation::OTValueCapacityModulation():
        OTValue(nullptr, OpenThermMessageID::MaxCapacityMinModLevel, 0) {
}

bool OTValueCapacityModulation::sendDiscovery() {
    return true;
}

void OTValueCapacityModulation::getValue(JsonObject &obj) const {
    obj[F("max_capacity")] = value >> 8;
    obj[F("min_modulation")] = value & 0xFF;
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

OTControl::OT::OT(OpenTherm &ot):
        ot(ot) {
    resetCounters();
}

void OTControl::OT::sendRequest(const char source, const unsigned long msg) {
    command.sendOtEvent('T', msg);
    ot.sendRequestAsync(msg);
    txCount++;
}

void OTControl::OT::resetCounters() {
    txCount = 0;
    rxCount = 0;
}

void OTControl::OT::onReceive(const char source, const unsigned long msg) {
    command.sendOtEvent(source, msg);
    rxCount++;
}

void OTControl::OT::sendResponse(const unsigned long msg) {
    ot.sendResponse(msg);
    txCount++;
}

OTControl::OTControl():
        lastMillis(millis()),
        otMode(OTMODE_LOOPBACKTEST),
        discFlag(false),
        nextDHWSet(0),
        nextBoilerTemp(0),
        master(otmaster),
        slave(otslave) {
}

void OTControl::begin() {
    // +24V enable for room unit (OT slave)
    pinMode(GPIO_STEPUP_ENABLE, OUTPUT);

    // relay
    pinMode(GPIO_BYPASS_RELAY, OUTPUT);

    setOTMode(otMode);

    otmaster.begin(handleIrqMaster, otCbMaster, 1, handleTimerIrqMaster);
    otslave.begin(handleIrqSlave, otCbSlave, 0, handleTimerIrqSlave);
}

void OTControl::setOTMode(const OTMode mode) {
    otMode = mode;

    // set bypass rely
    digitalWrite(GPIO_BYPASS_RELAY, mode != OTMODE_BYPASS);

    // set +24V steup up
    digitalWrite(GPIO_STEPUP_ENABLE, (mode == OTMODE_GATEWAY) || (mode == OTMODE_LOOPBACKTEST));

    for (auto *valobj: boilerValues)
        valobj->init((mode == OTMODE_MASTER) || (mode == OTMODE_LOOPBACKTEST));
}

void OTControl::loop() {
    if (!discFlag) {
        discFlag = true;

        haDisc.createNumber(F("Außentemperatur"), FPSTR(MQTTSETVAR_OUTSIDETEMP), mqtt.getVarSetTopic(MQTTSETVAR_OUTSIDETEMP));
        haDisc.setValueTemplate(F("{{ value_json.outsideTemp }}"));
        haDisc.setMinMax(-20, 20, 0.1);
        discFlag &= mqtt.publish(haDisc.topic, haDisc.doc);

        haDisc.createClima(F("Brauchwasser"), FPSTR(MQTTSETVAR_DHWSETTEMP), mqtt.getVarSetTopic(MQTTSETVAR_DHWSETTEMP));
        haDisc.setMinMaxTemp(25, 70, 1);
        haDisc.setCurrentTemperatureTemplate(F("{{ value_json.boiler.dhw_t }}"));
        haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
        haDisc.setInitial(50);
        haDisc.setIcon(F("mdi:shower"));
        haDisc.setRetain(true);
        discFlag &= mqtt.publish(haDisc.topic, haDisc.doc);

        haDisc.createClima(F("Vorlaufsolltemperatur"), FPSTR(MQTTSETVAR_CHSETTEMP), mqtt.getVarSetTopic(MQTTSETVAR_CHSETTEMP));
        haDisc.setMinMaxTemp(25, 90, 0.5);
        haDisc.setCurrentTemperatureTemplate(F("{{ value_json.boiler.flow_t }}"));
        haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
        haDisc.setInitial(30);
        haDisc.setModeCommandTopic(mqtt.getVarSetTopic(MQTTSETVAR_CHMODE));
        haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
        haDisc.setTemperatureStateTemplate(F("{{ value_json.boiler.ch_set_t }}"));
        haDisc.setOptimistic(true);
        haDisc.setIcon(F("mdi:heating-coil"));
        haDisc.setRetain(true);
        discFlag &= mqtt.publish(haDisc.topic, haDisc.doc);
    }
    otmaster.process();
    otslave.process();

    if (millis() > lastRxMaster + 100)
        digitalWrite(GPIO_OTMASTER_LED, HIGH);

    if (millis() > lastRxSlave + 100)
        digitalWrite(GPIO_OTSLAVE_LED, LOW);

    if (!otmaster.isReady())
        return;

    switch (otMode) {
    case OTMODE_MASTER:
    case OTMODE_LOOPBACKTEST:
        for (auto *valobj: boilerValues) {
            if (valobj->process())
                return;
        }

        if (millis() > lastMillis + 1000) {
            lastMillis = millis();
            uint32_t req = otmaster.buildSetBoilerStatusRequest(
                heatingParams[0].chOn, 
                dhwOn, 
                false, 
                false, 
                heatingParams[1].chOn);
            master.sendRequest('T', req);
            return;
        }

        if (millis() > nextBoilerTemp) {
            Serial.println("Calcing flow temp!");

            HeatingParams &hp = heatingParams[0];
            double flow = heatingParams[0].flowDefault;

            switch (heatingParams[0].ctrlMode) {
            case CTRLMODE_ON:
                flow = heatingParams[0].flow;
                break;

            case CTRLMODE_AUTO: {
                // TODO check if outside temp available, if not -> default flow
                double minOutside = hp.roomSet - (hp.flowMax - hp.roomSet) / hp.gradient;
                double c1 = (hp.flowMax - hp.roomSet) / pow(hp.roomSet - minOutside, 1.0 / hp.exponent);
                flow = hp.roomSet + c1 * pow(hp.roomSet - outsideTemp.temp, 1.0 / hp.exponent) + hp.offset;
                if (flow > hp.flowMax)
                    flow = hp.flowMax;
                break;
            }

            case CTRLMODE_OFF:
                flow = 0;
                break;
            
            default:
                break;
            }

            Serial.println(flow);
            if (flow > 0) {
                uint32_t req = otmaster.buildSetBoilerTemperatureRequest(flow);
                master.sendRequest('T', req);
            }

            nextBoilerTemp = millis() + 10000;
            return;
        }

        if (millis() > nextDHWSet) {
            nextDHWSet = millis() +  30000;
            uint16_t data = OpenTherm::temperatureToData(dhwTemp);
            uint32_t req = otmaster.buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet, data); //otmaster.setDHWSetpoint(50);
            master.sendRequest('T', req);
            return;
        }
        break;
    }
}

void OTControl::OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status) {
    if (status == OpenThermResponseStatus::TIMEOUT) {
        if (otMode == OTMODE_LOOPBACKTEST)
            portal.textAll(F("LOOPBACK failure (timeout)"));
        return;
    }
        
    digitalWrite(GPIO_OTMASTER_LED, LOW);
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
        for (unsigned int i = 0; i< sizeof(loopbackTestData) / sizeof(loopbackTestData[0]); i++) {
            if (loopbackTestData[i].id == id) {
                if (loopbackTestData[i].value == (msg & 0xFFFF))
                    Serial.println("LOOPBACK OK");
                else
                    Serial.println("LOOPBACK failure");
                break;
            }
        }
        break;

    case OTMODE_GATEWAY:
        // forward reply from boiler to room unit
        // TODO manipulate some frames, e. g. outsidetemp 
        switch (id) {
        case OpenThermMessageID::Toutside:
            //newMsg = otmaster.buildRequest(OpenThermMessageType::READ_ACK, id, 0 << 8 | 127);
            break;
        }
        slave.sendResponse(newMsg);
    }

    switch (status) {
    case OpenThermResponseStatus::SUCCESS: {
        if (newMsg == msg)
            master.onReceive('B', msg);
        if (otval)
            otval->setValue(newMsg & 0xFFFF);
        break;
    }

    case OpenThermResponseStatus::INVALID: {
        if (newMsg == msg)
            master.onReceive('E', msg);
        if (otval) {
            OpenThermMessageType mt = OpenTherm::getMessageType(msg);
            if (mt >= OpenThermMessageType::DATA_INVALID)
                otval->disable();
        }
        break;
    }
    default:
        break;
    }
    if (msg != newMsg)
        master.onReceive('A', newMsg);

    if (!otval)
        portal.textAll(F("no otval!"));
}

void OTControl::OnRxSlave(const unsigned long msg, const OpenThermResponseStatus status) {
    digitalWrite(GPIO_OTSLAVE_LED, HIGH);
    lastRxSlave = millis();

    OpenThermMessageID id = OpenTherm::getDataID(msg);

    OTValue *otval = nullptr;
    for (auto *valobj: thermostatValues) {
        if (valobj->getId() == id) {
            otval = valobj;
            break;
        }
    }

    switch (otMode) {
    case OTMODE_GATEWAY: {
        // we received a request from the room unit, forward it to boiler
        // TODO check for special commands, e. g. outside temp

        unsigned long newMsg = msg;
        switch (id) {
        case OpenThermMessageID::TSet:
            //newMsg = otmaster.buildRequest(OpenThermMessageType::WRITE_DATA, id, 57 << 8);
            break;

        case OpenThermMessageID::Status:
            //newMsg = otmaster.buildRequest(OpenThermMessageType::READ_DATA, id, msg & 0xFFFF | 0x100<<3); // activate OTC
            break;
        }

        if (msg != newMsg)
            master.sendRequest('R', newMsg);
        else
            master.sendRequest('T', newMsg);

        if (otval)
            otval->setValue(newMsg & 0xFFFF);
        break;
    }

    case OTMODE_LOOPBACKTEST: {
        // we received a request from OT master
        OpenThermMessageType msgType = OpenTherm::getMessageType(msg);
        switch (msgType) {
        case OpenThermMessageType::WRITE_DATA: {
            uint32_t reply = otslave.buildResponse(OpenThermMessageType::WRITE_ACK, id, msg & 0xFFFF);
            otslave.status = OpenThermStatus::READY;
            slave.sendResponse(reply);
            break;
        }

        case OpenThermMessageType::READ_DATA: {
            uint32_t reply = otslave.buildResponse(OpenThermMessageType::UNKNOWN_DATA_ID, id, msg & 0xFFFF);

            for (unsigned int i = 0; i< sizeof(loopbackTestData) / sizeof(loopbackTestData[0]); i++) {
                if (loopbackTestData[i].id == id) {
                    reply = otslave.buildResponse(OpenThermMessageType::READ_ACK, id, loopbackTestData[i].value);
                    break;
                }
            }
            otslave.status = OpenThermStatus::READY;
            slave.sendResponse(reply);
            break;
        }
        default:
            break;
        }
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
    boiler[F("txCount")] = master.txCount;
    boiler[F("rxCount")] = master.rxCount;

    switch (otMode) {
    case OTMODE_GATEWAY:
    case OTMODE_LOOPBACKTEST: {
        JsonObject thermostat = obj[F("thermostat")].to<JsonObject>();
        for (auto *valobj: thermostatValues)
            valobj->getJson(thermostat);
        thermostat[F("txCount")] = master.txCount;
        thermostat[F("rxCount")] = master.rxCount;
        break;
    }
    default:
        break;
    }
}

void OTControl::resetDiscovery() {
    for (auto *valobj: boilerValues)
        valobj->discFlag = false;

    discFlag = false;
}

void OTControl::setDhwTemp(double temp) {
    dhwTemp = temp;
    nextDHWSet = 0;
}

void OTControl::setChCtrlConfig(JsonObject &config) {
    OTMode mode = OTMODE_BYPASS;

    if (config[F("otMode")].is<JsonInteger>())
        mode = (OTMode) (int) config[F("otMode")];

    JsonObject boiler = config[F("boiler")];

    for (int i=0; i<sizeof(heatingParams) / sizeof(heatingParams[0]); i++) {
        JsonObject hpObj = boiler[F("heating")][i];

        heatingParams[i].chOn = hpObj[F("chOn")];
        heatingParams[i].roomSet = hpObj[F("roomSet")] | 21.0;
        heatingParams[i].flowMax = hpObj[F("flowMax")] | 40;
        heatingParams[i].exponent = hpObj[F("exponent")] | 1.0;
        heatingParams[i].gradient = hpObj[F("gradient")] | 1.0;
        heatingParams[i].offset = hpObj[F("offset")] | 0;
        heatingParams[i].flowDefault = hpObj[F("flow")] | 35;
        heatingParams[i].flow = heatingParams[i].flowDefault;
        heatingParams[i].override = false;
        heatingParams[i].ctrlMode = CTRLMODE_AUTO;
    }

    dhwOn = boiler[F("dhwOn")];
    dhwTemp = config[F("boiler")][F("dhwTemperature")] | 45;

    setOTMode(mode);

    nextDHWSet = 0;
}

void OTControl::setChCtrlMode(const CtrlMode mode) {
    heatingParams[0].ctrlMode = mode;
    nextBoilerTemp = 0;
}

void OTControl::setChTemp(const double temp) {
    if (temp == 0)
        heatingParams[0].ctrlMode = CTRLMODE_AUTO;
    else
        heatingParams[0].flow = temp;
    nextBoilerTemp = 0;
}

