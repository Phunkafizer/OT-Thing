#include "otcontrol.h"
#include "otvalues.h"
#include "command.h"
#include "HADiscLocal.h"
#include "mqtt.h"
#include "hwdef.h"
#include "outsidetemp.h"
#include "portal.h"

static OpenTherm otmaster(GPIO_OTMASTER_IN, GPIO_OTMASTER_OUT);
static OpenTherm otslave(GPIO_OTSLAVE_IN, GPIO_OTSLAVE_OUT, true);
OTControl otcontrol;

constexpr uint16_t floatToOT(double f) {
    return (((int) f) << 8) | (int) ((f - (int) f) * 256);
}

const struct {
    OpenThermMessageID id;
    uint16_t value;
} loopbackTestData[] PROGMEM = {
    {OpenThermMessageID::Status,                    0x0000},
    {OpenThermMessageID::Tboiler,                   floatToOT(48.5)},
    {OpenThermMessageID::TflowCH2,                  floatToOT(48.6)},
    {OpenThermMessageID::Texhaust,                  90},
    {OpenThermMessageID::RelModLevel,               floatToOT(33.3)},
    {OpenThermMessageID::CHPressure,                floatToOT(1.25)},
    {OpenThermMessageID::DHWFlowRate,               floatToOT(2.4)},
    {OpenThermMessageID::Tret,                      floatToOT(41.7)},
    {OpenThermMessageID::Tdhw,                      floatToOT(37.5)},
    {OpenThermMessageID::Tdhw2,                     floatToOT(37.6)},
    {OpenThermMessageID::Toutside,                  floatToOT(3.5)},
    {OpenThermMessageID::SuccessfulBurnerStarts,    9999},
    {OpenThermMessageID::BurnerOperationHours,      8888},
    {OpenThermMessageID::SlaveVersion,              0x0404},
    {OpenThermMessageID::SConfigSMemberIDcode,      0x0101},
    {OpenThermMessageID::MaxCapacityMinModLevel,    0x1240},
    {OpenThermMessageID::TdhwSetUBTdhwSetLB,        0x0540},
    {OpenThermMessageID::CHPumpStarts,              7777}
};


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
    command.sendOtEvent(source, msg);
    ot.sendRequestAsync(msg);
    txCount++;
}

void OTControl::OT::resetCounters() {
    txCount = 0;
    rxCount = 0;
}

void OTControl::OT::onReceive(const char source, const unsigned long msg) {
    if (source)
        command.sendOtEvent(source, msg);
    rxCount++;
    lastRx = millis();
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

    otmaster.begin(handleIrqMaster, otCbMaster, 1, handleTimerIrqMaster);
    otslave.begin(handleIrqSlave, otCbSlave, 0, handleTimerIrqSlave);

    setOTMode(otMode);
}

void OTControl::setOTMode(const OTMode mode) {
    otMode = mode;

    // set bypass relay
    digitalWrite(GPIO_BYPASS_RELAY, mode != OTMODE_BYPASS);

    // set +24V stepup up
    digitalWrite(GPIO_STEPUP_ENABLE, (mode == OTMODE_GATEWAY) || (mode == OTMODE_LOOPBACKTEST) || (mode == OTMODE_ETEST));

    for (auto *valobj: boilerValues)
        valobj->init((mode == OTMODE_MASTER) || (mode == OTMODE_LOOPBACKTEST));

    for (auto *valobj: thermostatValues)
        valobj->init(false);

    resetDiscovery();
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

    if (otMode == OTMODE_MASTER) {
        // in OTMASTER mode use OT LEDs as master TX & RX
        if (millis() > master.lastRx + 100)
            digitalWrite(GPIO_OTRED_LED, HIGH);
        digitalWrite(GPIO_OTGREEN_LED, digitalRead(GPIO_OTMASTER_IN));
    }
    else {
        // in all other modes use OT LEDs as master / slave receive
        digitalWrite(GPIO_OTRED_LED, !digitalRead(GPIO_OTMASTER_IN));
        digitalWrite(GPIO_OTGREEN_LED, digitalRead(GPIO_OTSLAVE_IN));
    }

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
            unsigned long req = otmaster.buildSetBoilerStatusRequest(
                heatingParams[0].chOn, 
                dhwOn, 
                false, 
                false, 
                heatingParams[1].chOn);
            sendRequest('T', req);
            return;
        }

        if (millis() > nextBoilerTemp) {
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
                sendRequest('T', req);
            }

            nextBoilerTemp = millis() + 10000;
            return;
        }

        if (millis() > nextDHWSet) {
            nextDHWSet = millis() +  30000;
            uint16_t data = OpenTherm::temperatureToData(dhwTemp);
            unsigned long req = otmaster.buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet, data); //otmaster.setDHWSetpoint(50);
            sendRequest('T', req);
            return;
        }
        break;
    }
}

void OTControl::sendRequest(const char source, const unsigned long msg) {
    master.sendRequest(source, msg);
    if (otMode == OTMODE_MASTER) {
        setThermostatVal(msg);
        digitalWrite(GPIO_OTRED_LED, LOW); // when we're OTMASTER use red LED as TX LED
        master.lastRx = millis();
    }
}

void OTControl::OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status) {
    if (status == OpenThermResponseStatus::TIMEOUT) {
        if (otMode == OTMODE_LOOPBACKTEST)
            portal.textAll(F("LOOPBACK failure (timeout)"));
        return;
    }

    // we received response from boiler
    auto id = OpenTherm::getDataID(msg);
    auto mt = OpenTherm::getMessageType(msg);
    auto *otval = OTValue::getBoilerValue(id);

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
        if (otval && ( (mt == OpenThermMessageType::READ_ACK)))
            otval->setValue(newMsg & 0xFFFF);
        break;
    }

    case OpenThermResponseStatus::INVALID: {
        if (newMsg == msg)
            master.onReceive('E', msg);
        if (otval) {
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

    if (!otval && (mt == OpenThermMessageType::READ_ACK))
        portal.textAll(F("no otval!"));
}

void OTControl::OnRxSlave(const unsigned long msg, const OpenThermResponseStatus status) {
    //digitalWrite(GPIO_OTSLAVE_LED, HIGH);
    slave.onReceive(0, msg);

    auto id = OpenTherm::getDataID(msg);
    auto mt = OpenTherm::getMessageType(msg);
    unsigned long newMsg = msg;

    switch (otMode) {
    case OTMODE_GATEWAY: {
        // we received a request from the room unit, forward it to boiler
        // TODO check for special commands, e. g. outside temp

        switch (id) {
        case OpenThermMessageID::TSet:
            //newMsg = otmaster.buildRequest(OpenThermMessageType::WRITE_DATA, id, 57 << 8);
            break;

        case OpenThermMessageID::Status:
            //newMsg = otmaster.buildRequest(OpenThermMessageType::READ_DATA, id, msg & 0xFFFF | 0x100<<3); // activate OTC
            break;
        }

        master.sendRequest((msg == newMsg) ? 'T' : 'R', newMsg);
        break;
    }

    case OTMODE_LOOPBACKTEST: {
        // we received a request from OT master
        switch (mt) {
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

    if (!setThermostatVal(newMsg) && (mt == OpenThermMessageType::WRITE)) {
        portal.textAll(F("no otval!"));
        Serial.println(F("no otval!"));
    }
}

bool OTControl::setThermostatVal(const unsigned long msg) {
    auto id = OpenTherm::getDataID(msg);
    //auto mt = OpenTherm::getMessageType(msg);

    for (auto *valobj: thermostatValues) {
        if (valobj->getId() == id) {
            valobj->setValue(msg & 0xFFFF);
            return true;
        }
    }
    return false;
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

    JsonObject thermostat = obj[F("thermostat")].to<JsonObject>();
    for (auto *valobj: thermostatValues)
        valobj->getJson(thermostat);

    switch (otMode) {
    case OTMODE_GATEWAY:
    case OTMODE_LOOPBACKTEST: {
        thermostat[F("txCount")] = slave.txCount;
        thermostat[F("rxCount")] = slave.rxCount;
        break;
    }
    default:
        break;
    }
}

void OTControl::resetDiscovery() {
    for (auto *valobj: boilerValues)
        valobj->discFlag = false;

    for (auto *valobj: thermostatValues)
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

