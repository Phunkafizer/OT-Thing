#include "otcontrol.h"
#include "otvalues.h"
#include "command.h"
#include "HADiscLocal.h"
#include "mqtt.h"
#include "hwdef.h"
#include "portal.h"
#include "sensors.h"

OTControl otcontrol;

constexpr uint16_t floatToOT(double f) {
    return (((int) f) << 8) | (int) ((f - (int) f) * 256);
}

// Testdata for OT slave, can be read by a connected master
const struct {
    OpenThermMessageID id;
    uint16_t value;
} loopbackTestData[] PROGMEM = {
    {OpenThermMessageID::Status,                    0x000E},
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
    {OpenThermMessageID::TdhwSetUBTdhwSetLB,        0x0540},
    {OpenThermMessageID::CHPumpStarts,              7777},
    {OpenThermMessageID::RBPflags,                  0x0101},
    {OpenThermMessageID::TrOverride,                0},
    {OpenThermMessageID::OpenThermVersionSlave,     0x0202},
    {OpenThermMessageID::TdhwSetUBTdhwSetLB,        60<<8 | 40}
};


void IRAM_ATTR handleIrqMaster() {
    otcontrol.masterPinIrq();
}

void IRAM_ATTR handleIrqSlave() {
    otcontrol.slavePinIrq();
}

void IRAM_ATTR handleTimerIrqMaster() {
    otcontrol.master.hal.handleTimerIrq();
}

void IRAM_ATTR handleTimerIrqSlave() {
    otcontrol.slave.hal.handleTimerIrq();
}

//OpenTherm master callback
void otCbMaster(unsigned long response, OpenThermResponseStatus status) {
    otcontrol.OnRxMaster(response, status);
}

//OpenTherm slave callback
void otCbSlave(unsigned long response, OpenThermResponseStatus status) {
    otcontrol.OnRxSlave(response, status);
}

OTControl::OTInterface::OTInterface(const uint8_t inPin, const uint8_t outPin, const bool isSlave):
        hal(inPin, outPin, isSlave) {
    resetCounters();
}

void OTControl::OTInterface::sendRequest(const char source, const unsigned long msg) {
    if (source)
        command.sendOtEvent(source, msg);
    hal.sendRequestAsync(msg);
    txCount++;
    lastTx = millis();
    lastTxMsg = msg;
}

void OTControl::OTInterface::resetCounters() {
    txCount = 0;
    rxCount = 0;
    timeoutCount = 0;
}

void OTControl::OTInterface::onReceive(const char source, const unsigned long msg) {
    if (source)
        command.sendOtEvent(source, msg);
    rxCount++;
    lastRx = millis();
}

void OTControl::OTInterface::sendResponse(const unsigned long msg) {
    hal.sendResponse(msg);
    txCount++;
    lastTx = millis();
    lastTxMsg = msg;
}


OTControl::OTControl():
        lastMillis(millis()),
        otMode(OTMODE_LOOPBACKTEST),
        discFlag(false),
        nextDHWSet(0),
        nextBoilerTemp({}),
        master(GPIO_OTMASTER_IN, GPIO_OTMASTER_OUT, false),
        slave(GPIO_OTSLAVE_IN, GPIO_OTSLAVE_OUT, true) {
}

void OTControl::begin() {
    // +24V enable for room unit (OT slave)
    pinMode(GPIO_STEPUP_ENABLE, OUTPUT);

    // relay
    pinMode(GPIO_BYPASS_RELAY, OUTPUT);

    master.hal.begin(handleIrqMaster, otCbMaster, 1, handleTimerIrqMaster);
    slave.hal.begin(handleIrqSlave, otCbSlave, 0, handleTimerIrqSlave);

    setOTMode(otMode);
}

void OTControl::masterPinIrq() {
    bool state = digitalRead(GPIO_OTMASTER_IN);

    if (otMode == OTMODE_REPEATER)
        digitalWrite(GPIO_OTSLAVE_OUT, !state); // repeat current from master to slave

    if (otMode == OTMODE_MASTER) // in master mode green LED is used for RX, red LED for TX
        digitalWrite(GPIO_OTGREEN_LED, state);
    else
        digitalWrite(GPIO_OTRED_LED, !state);
    
    master.hal.handleInterrupt();
}

void OTControl::slavePinIrq() {
    const bool state = digitalRead(GPIO_OTSLAVE_IN);
    if (otMode == OTMODE_REPEATER)
        digitalWrite(GPIO_OTMASTER_OUT, !state); // repeat voltage from slave to master

    digitalWrite(GPIO_OTGREEN_LED, state);
    slave.hal.handleInterrupt();
}

void OTControl::setOTMode(const OTMode mode) {
    otMode = mode;

    // set bypass relay
    digitalWrite(GPIO_BYPASS_RELAY, mode != OTMODE_BYPASS);

    // set +24V stepup up
    digitalWrite(GPIO_STEPUP_ENABLE, (mode == OTMODE_GATEWAY) || (mode == OTMODE_LOOPBACKTEST) || (mode == OTMODE_REPEATER));

    for (auto *valobj: boilerValues)
        valobj->init((mode == OTMODE_MASTER) || (mode == OTMODE_LOOPBACKTEST));

    for (auto *valobj: thermostatValues)
        valobj->init(false);

    master.hal.alwaysReceive = (mode == OTMODE_GATEWAY) || (mode == OTMODE_REPEATER);

    resetDiscovery();
}

void OTControl::loop() {
    if (!discFlag) {
        discFlag = true;

        // TODO add only when outside temp source is MQTT
        haDisc.createNumber(F("Außentemperatur"), FPSTR(MQTTSETVAR_OUTSIDETEMP), mqtt.getVarSetTopic(MQTTSETVAR_OUTSIDETEMP));
        haDisc.setValueTemplate(F("{{ value_json.outsideTemp }}"));
        haDisc.setMinMax(-20, 20, 0.1);
        discFlag &= haDisc.publish();

        haDisc.createClima(F("Brauchwasser"), FPSTR(MQTTSETVAR_DHWSETTEMP), mqtt.getVarSetTopic(MQTTSETVAR_DHWSETTEMP));
        haDisc.setMinMaxTemp(25, 70, 1);
        haDisc.setCurrentTemperatureTemplate(F("{{ value_json.boiler.dhw_t }}"));
        haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
        haDisc.setInitial(45);
        haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
        haDisc.setTemperatureStateTemplate(F("{{ value_json.thermostat.dhw_set_t }}"));
        haDisc.setOptimistic(true);
        haDisc.setIcon(F("mdi:shower"));
        haDisc.setRetain(true);
        discFlag &= haDisc.publish();

        haDisc.createClima(F("Vorlaufsolltemperatur"), FPSTR(MQTTSETVAR_CHSETTEMP1), mqtt.getVarSetTopic(MQTTSETVAR_CHSETTEMP1));
        haDisc.setMinMaxTemp(25, 90, 0.5);
        haDisc.setCurrentTemperatureTemplate(F("{{ value_json.boiler.flow_t }}"));
        haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
        haDisc.setInitial(35);
        haDisc.setModeCommandTopic(mqtt.getVarSetTopic(MQTTSETVAR_CHMODE1));
        haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
        haDisc.setTemperatureStateTemplate(F("{{ value_json.thermostat.ch_set_t }}"));
        haDisc.setOptimistic(true);
        haDisc.setIcon(F("mdi:heating-coil"));
        haDisc.setRetain(true);
        discFlag &= haDisc.publish();

        haDisc.createClima(F("Raumtemperatur 1"), F("clima_room1"), mqtt.getVarSetTopic(MQTTSETVAR_ROOMSETPOINT1));
        haDisc.setMinMaxTemp(12, 27, 0.5);
        haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
        haDisc.setCurrentTemperatureTemplate(F("{{ value_json.heatercircuit[0].roomtemp }}"));
        haDisc.setInitial(20);
        haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
        haDisc.setTemperatureStateTemplate(F("{{ value_json.heatercircuit[0].roomsetpoint }}"));
        haDisc.setOptimistic(true);
        haDisc.setRetain(true);
        if (!heatingParams[0].chOn)
            haDisc.clearDoc();
        discFlag &= haDisc.publish();

        haDisc.createClima(F("Raumtemperatur 2"), F("clima_room2"), mqtt.getVarSetTopic(MQTTSETVAR_ROOMSETPOINT2));
        haDisc.setMinMaxTemp(12, 27, 0.5);
        haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
        haDisc.setCurrentTemperatureTemplate(F("{{ value_json.heatercircuit[1].roomtemp }}"));
        haDisc.setInitial(20);
        haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
        haDisc.setTemperatureStateTemplate(F("{{ value_json.heatercircuit[1].roomsetpoint }}"));
        haDisc.setOptimistic(true);
        haDisc.setRetain(true);
        if (!heatingParams[1].chOn)
            haDisc.clearDoc();
        discFlag &= haDisc.publish();
        
        haDisc.createNumber(F("Raumtemperatur 1"), FPSTR(MQTTSETVAR_ROOMTEMP1), mqtt.getVarSetTopic(MQTTSETVAR_ROOMTEMP1));
        haDisc.setValueTemplate(F("{{ value_json.heatercircuit[0].roomtemp }}"));
        haDisc.setMinMax(5, 27, 0.1);
        if (!roomTemp[0].isMqttSource()) {
            haDisc.clearDoc();
        }
        discFlag &= haDisc.publish();

        haDisc.createNumber(F("Raumtemperatur 2"), FPSTR(MQTTSETVAR_ROOMTEMP2), mqtt.getVarSetTopic(MQTTSETVAR_ROOMTEMP2));
        haDisc.setValueTemplate(F("{{ value_json.heatercircuit[1].roomtemp }}"));
        haDisc.setMinMax(5, 27, 0.1);
        if (!roomTemp[1].isMqttSource())
            haDisc.clearDoc();
        discFlag &= haDisc.publish();
    }

    master.hal.process();
    slave.hal.process();

    if (otMode == OTMODE_MASTER) {
        // in OTMASTER mode use OT LEDs as master TX & RX
        if (millis() > master.lastTx + 50)
            digitalWrite(GPIO_OTRED_LED, HIGH);
    }

    if (!master.hal.isReady())
        return;

    switch (otMode) {
    case OTMODE_LOOPBACKTEST: {
        static uint32_t nextRoomTemps[2] = {0, 0};
        static uint32_t nextRoomSetPoints[2] = {0, 0};
        for (int i=0; i<2; i++) {
            if (millis() > nextRoomTemps[i]) {
                uint32_t req = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, (i == 0) ? OpenThermMessageID::Tr : OpenThermMessageID::TrCH2, 19<<8 | (26 + i * 26));
                sendRequest('T', req);
                nextRoomTemps[i] = millis() + 10000;
                return;
            }
            if (millis() > nextRoomSetPoints[i]) {
                uint32_t req = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, (i == 0) ? OpenThermMessageID::TrSet : OpenThermMessageID::TrSetCH2, 20<<8 | (26 + i * 26));
                sendRequest('T', req);
                nextRoomSetPoints[i] = millis() + 10000;
                return;
            }
        }
        // no break!!
    }

    case OTMODE_MASTER:
        for (auto *valobj: boilerValues) {
            if (valobj->process())
                return;
        }

        for (int ch=0; ch<2; ch++) {
            if (!heatingParams[ch].chOn)
                continue;

            if (millis() > nextBoilerTemp[ch]) {
                HeatingParams &hp = heatingParams[ch];
                double flow = hp.flowDefault;

                switch (heatingParams[ch].ctrlMode) {
                case CTRLMODE_ON:
                    flow = heatingParams[ch].flow;
                    break;

                case CTRLMODE_AUTO: {
                    double outTmp;
                    if ((hp.ctrlMode == CTRLMODE_AUTO) && outsideTemp.get(outTmp)) {
                        double roomSet = hp.roomSet; // default room set point
                        roomSetPoint[ch].get(roomSet);

                        double minOutside = roomSet - (hp.flowMax - roomSet) / hp.gradient;
                        double c1 = (hp.flowMax - roomSet) / pow(roomSet - minOutside, 1.0 / hp.exponent);
                        flow = roomSet + c1 * pow(roomSet - outTmp, 1.0 / hp.exponent) + hp.offset;
                        if (flow > hp.flowMax)
                            flow = hp.flowMax;
                    }
                    else
                        flow = heatingParams[ch].flow;
                    break;
                }

                case CTRLMODE_OFF:
                    flow = 0;
                    break;
                
                default:
                    break;
                }

                if (flow > 0) {
                    uint32_t req = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, (ch == 0) ? OpenThermMessageID::TSet : OpenThermMessageID::TsetCH2, OpenTherm::temperatureToData(flow));
                    sendRequest('T', req);
                }

                nextBoilerTemp[ch] = millis() + 10000;
                return;
            }
        }

        if (millis() > nextDHWSet) {
            nextDHWSet = millis() + 30000;
            uint16_t data = OpenTherm::temperatureToData(dhwTemp);
            unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet, data);
            sendRequest('T', req);
            return;
        }

        if (millis() > lastMillis + 1000) {
            lastMillis = millis();
            unsigned long req = OpenTherm::buildSetBoilerStatusRequest(
                heatingParams[0].chOn, 
                dhwOn, 
                false, 
                false, 
                heatingParams[1].chOn);
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
    }
}

void OTControl::OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status) {
    switch (status) {
    case OpenThermResponseStatus::TIMEOUT:
        master.timeoutCount++;
        return;

    case OpenThermResponseStatus::INVALID:
        master.onReceive('E', msg);
        return;

    case OpenThermResponseStatus::SUCCESS:
        break;

    default:
        return;
    }
 
    // we received response from boiler
    auto id = OpenTherm::getDataID(msg);
    auto mt = OpenTherm::getMessageType(msg);
    auto *otval = OTValue::getBoilerValue(id);

    unsigned long newMsg = msg;

    switch (otMode) {
    case OTMODE_LOOPBACKTEST:
        if (slave.lastTxMsg != msg) {
            Serial.println(F("looperror slave->master !"));
            portal.textAll(F("looperror slave->master !"));
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
        break;

    case OTMODE_REPEATER:
        // we implicitely have sent a frame on slave
        slave.txCount++;
        break;
    }

    if (newMsg == msg)
        master.onReceive('B', msg);
    
    if (otval) {
        switch (mt) {
        case OpenThermMessageType::READ_ACK:
            otval->setValue(newMsg & 0xFFFF);
            switch (id) {
            case OpenThermMessageID::Toutside:
                outsideTemp.set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            case OpenThermMessageID::Tr:
                roomTemp[0].set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            case OpenThermMessageID::TrCH2:
                roomTemp[1].set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            case OpenThermMessageID::TrSet:
                roomSetPoint[0].set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            case OpenThermMessageID::TrSetCH2:
                roomSetPoint[1].set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            default:
                break;
            }
            break;

        case OpenThermMessageType::UNKNOWN_DATA_ID:
            otval->disable();
            break;

        default:
            break;
        }
    }

    if (msg != newMsg)
        master.onReceive('A', newMsg);

    if (!otval && (mt == OpenThermMessageType::READ_ACK))
        portal.textAll(F("no otval!"));
}

void OTControl::OnRxSlave(const unsigned long msg, const OpenThermResponseStatus status) {
    if (status != OpenThermResponseStatus::SUCCESS) {
        return;
    }

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
        master.sendRequest(0, newMsg);
        break;
    }

    case OTMODE_LOOPBACKTEST: {
        // we received a request from OT master
        switch (mt) {
        case OpenThermMessageType::WRITE_DATA: {
            uint32_t reply = OpenTherm::buildResponse(OpenThermMessageType::WRITE_ACK, id, msg & 0xFFFF);
            while (!slave.hal.isReady()) {
                slave.hal.process();
                yield();
            }
            slave.sendResponse(reply);
            break;
        }

        case OpenThermMessageType::READ_DATA: {
            uint32_t reply = OpenTherm::buildResponse(OpenThermMessageType::UNKNOWN_DATA_ID, id, msg & 0xFFFF);

            for (unsigned int i = 0; i< sizeof(loopbackTestData) / sizeof(loopbackTestData[0]); i++) {
                if (loopbackTestData[i].id == id) {
                    reply = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, loopbackTestData[i].value);
                    break;
                }
            }

            while (!slave.hal.isReady()) {
                slave.hal.process();
                yield();
            }
            slave.sendResponse(reply);
            break;
        }

        default:
            break;
        }
        break;
    }

    case OTMODE_REPEATER:
        // we implicitly sent frame on master
        master.txCount++;
        master.lastTx = millis();
        break;

    default:
        break;
    }

    if (otMode == OTMODE_LOOPBACKTEST)
        slave.onReceive(0, msg);
    else
        slave.onReceive((msg == newMsg) ? 'T' : 'R', msg);

    if ( (mt == OpenThermMessageType::WRITE_DATA) || (id == OpenThermMessageID::Status) ) {
        double d = OpenTherm::getFloat(newMsg);
        switch (id) {
        case OpenThermMessageID::Tr:
            roomTemp[0].set(d, Sensor::SOURCE_OT);
            break;
        case OpenThermMessageID::TrSet:
            roomSetPoint[0].set(d, Sensor::SOURCE_OT);
            break;
        case OpenThermMessageID::TrCH2:
            roomTemp[1].set(d, Sensor::SOURCE_OT);
            break;
        case OpenThermMessageID::TrSetCH2:
            roomSetPoint[1].set(d, Sensor::SOURCE_OT);
            break;
        }
        if (!setThermostatVal(newMsg))
            portal.textAll(F("no otval!"));
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

void OTControl::getJson(JsonObject &obj) {
    JsonObject boiler = obj[F("boiler")].to<JsonObject>();
    for (auto *valobj: boilerValues) {
        valobj->getJson(boiler);
    }

    static bool slaveConnected = false;
    switch (master.hal.getLastResponseStatus()) {
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
    if ( (otMode == OTMODE_MASTER) || (otMode == OTMODE_LOOPBACKTEST) )
        boiler[F("timeouts")] = master.timeoutCount;

    JsonObject thermostat = obj[F("thermostat")].to<JsonObject>();
    for (auto *valobj: thermostatValues)
        valobj->getJson(thermostat);

    switch (otMode) {
    case OTMODE_GATEWAY:
    case OTMODE_LOOPBACKTEST:
    case OTMODE_REPEATER:
        thermostat[F("txCount")] = slave.txCount;
        thermostat[F("rxCount")] = slave.rxCount;
        break;

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
        heatingParams[i].roomSet = hpObj[F("roomSet")] | 21.0; // default room set point
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
    nextBoilerTemp[0] = 0;
    nextBoilerTemp[1] = 0;

    master.resetCounters();
    slave.resetCounters();
}

void OTControl::setChCtrlMode(const CtrlMode mode, const uint8_t channel) {
    heatingParams[channel].ctrlMode = mode;
    nextBoilerTemp[channel] = 0;
}

void OTControl::setChTemp(const double temp, const uint8_t channel) {
    if (temp == 0)
        heatingParams[channel].ctrlMode = CTRLMODE_AUTO;
    else
        heatingParams[channel].flow = temp;
    nextBoilerTemp[channel] = 0;
}

