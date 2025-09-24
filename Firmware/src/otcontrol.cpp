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

constexpr uint16_t nib(uint8_t hb, uint8_t lb) {
    return (hb << 8) | lb;
}

// Testdata for OT slave, can be read by a connected master
const struct {
    OpenThermMessageID id;
    uint16_t value;
} loopbackTestData[] PROGMEM = {
    {OpenThermMessageID::Status,                    0x000E},
    {OpenThermMessageID::SConfigSMemberIDcode,      0x2501}, // DHW present, cooling present, CH2 present
    {OpenThermMessageID::ASFflags,                  0x0000}, // no error flags, oem error code 0
    {OpenThermMessageID::RBPflags,                  0x0101},
    {OpenThermMessageID::TrOverride,                0},
    {OpenThermMessageID::MaxCapacityMinModLevel,    nib(20, 5)}, // 20 kW / 5 %
    {OpenThermMessageID::RelModLevel,               floatToOT(33.3)},
    {OpenThermMessageID::CHPressure,                floatToOT(1.25)},
    {OpenThermMessageID::DHWFlowRate,               floatToOT(2.4)},
    {OpenThermMessageID::Tboiler,                   floatToOT(48.5)},
    {OpenThermMessageID::Tdhw,                      floatToOT(37.5)},
    {OpenThermMessageID::Toutside,                  floatToOT(3.5)},
    {OpenThermMessageID::Tret,                      floatToOT(41.7)},
    {OpenThermMessageID::TflowCH2,                  floatToOT(48.6)},
    {OpenThermMessageID::Tdhw2,                     floatToOT(37.6)},
    {OpenThermMessageID::Texhaust,                  90},
    {OpenThermMessageID::TdhwSetUBTdhwSetLB,        nib(60, 40)}, // 60 °C upper bound, 40 C° lower bound
    {OpenThermMessageID::MaxTSetUBMaxTSetLB,        nib(60, 25)}, // 60 °C upper bound, 20 C° lower bound
    {OpenThermMessageID::SuccessfulBurnerStarts,    9999},
    {OpenThermMessageID::CHPumpStarts,              7777},
    {OpenThermMessageID::BurnerOperationHours,      8888},
    {OpenThermMessageID::OpenThermVersionSlave,     nib(2, 2)},
    {OpenThermMessageID::SlaveVersion,              nib(4, 4)}
};

void IRAM_ATTR handleIrqMaster() {
    otcontrol.masterPinIrq();
}

void IRAM_ATTR handleIrqSlave() {
    otcontrol.slavePinIrq();
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
    hal.sendRequestAsync(msg);
    
    if (source)
        command.sendOtEvent(source, msg);
    
    txCount++;
    lastTx = millis();
    lastTxMsg = msg;
}

void OTControl::OTInterface::resetCounters() {
    txCount = 0;
    rxCount = 0;
    timeoutCount = 0;
    invalidCount = 0;
}

void OTControl::OTInterface::onReceive(const char source, const unsigned long msg) {
    if (source)
        command.sendOtEvent(source, msg);
    rxCount++;
    lastRx = millis();
}

void OTControl::OTInterface::sendResponse(const unsigned long msg, const char source) {
    uint32_t temp = millis();

    while (true) {
        if (hal.sendResponse(msg)) {
            if (source)
                command.sendOtEvent(source, msg);
        
            txCount++;
            lastTx = millis();
            lastTxMsg = msg;
            break;
        }
        if (millis() - temp > 300)
            break;

        hal.process();
        yield();
    }
}


OTControl::OTControl():
        lastMillis(millis()),
        otMode(OTMODE_LOOPBACKTEST),
        master(GPIO_OTMASTER_IN, GPIO_OTMASTER_OUT, false),
        slave(GPIO_OTSLAVE_IN, GPIO_OTSLAVE_OUT, true),
        setBoilerRequest{OTWRSetBoilerTemp(0), OTWRSetBoilerTemp(1)} {
}

void OTControl::begin() {
    pinMode(GPIO_OTRED_LED, OUTPUT);
    pinMode(GPIO_OTGREEN_LED, OUTPUT);
    pinMode(GPIO_STEPUP_ENABLE, OUTPUT); // +24V enable for room unit (OT slave)
    pinMode(GPIO_BYPASS_RELAY, OUTPUT); // relay
    
    setLedOTGreen(false);
    setLedOTRed(false);

    master.hal.begin(handleIrqMaster, otCbMaster);
    slave.hal.begin(handleIrqSlave, otCbSlave);

    setOTMode(otMode);
}

void OTControl::masterPinIrq() {
    bool state = digitalRead(GPIO_OTMASTER_IN);

    if (otMode == OTMODE_MASTER) // in master mode green LED is used for RX, red LED for TX
        setLedOTGreen(state);
    else
        setLedOTRed(state);
    
    master.hal.handleInterrupt();
}

void OTControl::slavePinIrq() {
    const bool state = digitalRead(GPIO_OTSLAVE_IN);
    setLedOTGreen(state);
    slave.hal.handleInterrupt();
}

void OTControl::setOTMode(const OTMode mode) {
    otMode = mode;

    // set bypass relay
    digitalWrite(GPIO_BYPASS_RELAY, mode != OTMODE_BYPASS);

    // set +24V stepup up
    digitalWrite(GPIO_STEPUP_ENABLE, (mode == OTMODE_REPEATER) || (mode == OTMODE_LOOPBACKTEST));

    for (auto *valobj: boilerValues)
        valobj->init((mode == OTMODE_MASTER) || (mode == OTMODE_LOOPBACKTEST));

    for (auto *valobj: thermostatValues)
        valobj->init(false);

    master.hal.setAlwaysReceive(mode == OTMODE_REPEATER);
    discFlag = false;
}

double OTControl::getFlow(const uint8_t channel) {
    HeatingParams &hp = heatingParams[channel];
    double flow = hp.flowDefault;

    switch (hp.ctrlMode) {
    case CTRLMODE_ON:
        flow = hp.flow;
        break;

    case CTRLMODE_AUTO: {
        double outTmp;
        if (outsideTemp.get(outTmp)) {
            double roomSet = hp.roomSet; // default room set point
            roomSetPoint[channel].get(roomSet);

            double minOutside = roomSet - (hp.flowMax - roomSet) / hp.gradient;
            double c1 = (hp.flowMax - roomSet) / pow(roomSet - minOutside, 1.0 / hp.exponent);
            flow = roomSet + c1 * pow(roomSet - outTmp, 1.0 / hp.exponent) + hp.offset;
            if (flow > hp.flowMax)
                flow = hp.flowMax;
        }
        else
            flow = heatingParams[channel].flow;
        break;
    }

    case CTRLMODE_OFF:
        flow = 0;
        break;
    }

    return flow;
}

void OTControl::loop() {
    master.hal.process();
    slave.hal.process();

    if (!discFlag)
        discFlag = sendDiscovery();

    if (otMode == OTMODE_MASTER) {
        // in OTMASTER mode use OT LEDs as master TX & RX
        if (millis() > master.lastTx + 50)
            setLedOTRed(false);
    }

    if (!master.hal.isReady()) {
        return;
    }

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
            if (heatingParams[ch].chOn && setBoilerRequest[ch]) {
                double flow = getFlow(ch);

                if (flow > 0) {
                    setBoilerRequest[ch].send(OpenTherm::temperatureToData(flow));
                    return;
                }
            }
        }

        if (setDhwRequest) {
            setDhwRequest.send(OpenTherm::temperatureToData(dhwTemp));
            return;
        }

        if (setMasterConfigMember) {
            setMasterConfigMember.send((1<<8) | masterMemberId);
            return;
        }

        if (millis() > lastMillis + 800) {
            lastMillis = millis();
            bool chOn[2] = {false, false};
            for (int i=0; i<2; i++) {
                switch (heatingParams[i].ctrlMode) {
                case CtrlMode::CTRLMODE_OFF:
                    chOn[i] = false;
                    break;
                case CtrlMode::CTRLMODE_ON:
                    chOn[i] = true;
                    break;
                case CtrlMode::CTRLMODE_AUTO:
                    chOn[i] = heatingParams[i].chOn;
                    break;
                default:
                    break;
                }       
            }

            unsigned long req = OpenTherm::buildSetBoilerStatusRequest(
                chOn[0], 
                dhwOn, 
                false, 
                false, 
                chOn[1]);
            sendRequest('T', req);
            return;
        }

        static uint8_t testId = 0; // set >=1 to test all IDs
        if (testId) {
            unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, (OpenThermMessageID) testId, 0);
            sendRequest('T', req);
            testId++;
        }

        break;
    }
}

void OTControl::sendRequest(const char source, const unsigned long msg) {
    master.sendRequest(source, msg);
    if (otMode == OTMODE_MASTER) {
        setThermostatVal(msg);
        setLedOTRed(true); // when we're OTMASTER use red LED as TX LED
    }
}

void OTControl::OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status) {
    switch (status) {
    case OpenThermResponseStatus::TIMEOUT:
        master.timeoutCount++;
        return;

    case OpenThermResponseStatus::INVALID:
        if (otMode != OTMODE_REPEATER) {
            master.onReceive('E', msg);
            return;
        }
        break;

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
        break;

    case OTMODE_REPEATER:
        // forward reply from boiler to room unit
        // READ replies can be modified here
        switch (id) {
        case OpenThermMessageID::Toutside: {
            double ost;
            if ( !outsideTemp.isOtSource() && outsideTemp.get(ost))
                newMsg = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, OpenTherm::temperatureToData(ost));
            break;
        }
        case OpenThermMessageID::TdhwSet: {
            if (overrideDhw) {
                if (mt == OpenThermMessageType::READ_ACK)
                    // roomunit tried to read dhw set temp. Catch it in order to force writing DHW setpoint by roomunit.
                    newMsg = OpenTherm::buildResponse(OpenThermMessageType::DATA_INVALID, id, 0x0000);
            }
            break;
        }
        default:
            break;
        }
        slave.sendResponse(newMsg);
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
    if (status == OpenThermResponseStatus::INVALID) {
        slave.invalidCount++;
        return;
    }

    auto id = OpenTherm::getDataID(msg);
    auto mt = OpenTherm::getMessageType(msg);
    unsigned long newMsg = msg;

    switch (otMode) {
    case OTMODE_REPEATER: {
        // we received a request from the room unit, forward it to boiler
        // WRITE commands to boiler can be modified here

        switch (id) {
        case OpenThermMessageID::TSet:
            if ( (heatingParams[0].overrideFlow) && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(getFlow(0)));
            break;

        case OpenThermMessageID::TsetCH2:
            if ( (heatingParams[1].overrideFlow) && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(getFlow(1)));
            break;

        case OpenThermMessageID::TdhwSet:
            if ( overrideDhw && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(dhwTemp));
            break;

        case OpenThermMessageID::Status:
            if (heatingParams[0].overrideFlow) {
                if (heatingParams[0].ctrlMode == CtrlMode::CTRLMODE_OFF)
                    newMsg &= ~(1<<8); // CH1 disable
                else
                    newMsg |= 1<<8; // CH1 enable
            }
            if (heatingParams[1].overrideFlow) {
                if (heatingParams[1].ctrlMode == CtrlMode::CTRLMODE_OFF)
                    newMsg &= ~(1<<12); // CH2 disable
                else
                    newMsg |= 1<<12; // CH2 enable
            }
            if (overrideDhw) {
                if (dhwOn)
                    newMsg |= 1<<9; // DHW enable
                else
                    newMsg &= ~(1<<9); // DHW disable
            }                            
            
            newMsg = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, id, newMsg & 0xFFFF);
            break;
        }
        master.sendRequest(0, newMsg);
        break;
    }

    case OTMODE_LOOPBACKTEST: {
        slave.onReceive('S', msg);
        // we received a request from OT master
        switch (mt) {
        case OpenThermMessageType::WRITE_DATA: {
            uint32_t reply = OpenTherm::buildResponse(OpenThermMessageType::WRITE_ACK, id, msg & 0xFFFF);
            slave.sendResponse(reply, 'P');
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

            slave.sendResponse(reply, 'P');
            break;
        }

        default:
            break;
        }
        break;
    }

    default:
        break;
    }

    if (otMode != OTMODE_LOOPBACKTEST)
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
    for (auto *valobj: boilerValues)
        valobj->getJson(boiler);

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
    case OTMODE_REPEATER:
    case OTMODE_LOOPBACKTEST: {
        thermostat[F("txCount")] = slave.txCount;
        thermostat[F("rxCount")] = slave.rxCount;
        thermostat[F("invalidCount")] = slave.invalidCount;

        String sp;
        switch (slave.hal.getSmartPowerState()) {
        case OpenThermSmartPower::SMART_POWER_LOW:
            sp = F("low");
            break;
        case OpenThermSmartPower::SMART_POWER_MEDIUM:
            sp = F("medium");
            break;
        case OpenThermSmartPower::SMART_POWER_HIGH:
            sp = F("high");
            break;
        }
        thermostat[F("smartPower")] = sp;
    }

    default:
        break;
    }
}

bool OTControl::sendDiscovery() {
    for (auto *valobj: boilerValues)
        valobj->discFlag = false;

    for (auto *valobj: thermostatValues)
        valobj->discFlag = false;

    bool discFlag = true;

    haDisc.createNumber(F("Outside temperature"), FPSTR(MQTTSETVAR_OUTSIDETEMP), mqtt.getVarSetTopic(MQTTSETVAR_OUTSIDETEMP));
    haDisc.setValueTemplate(F("{{ value_json.outsideTemp }}"));
    haDisc.setMinMax(-20, 20, 0.1);
    haDisc.setRetain(true);
    if (!outsideTemp.isMqttSource())
        haDisc.clearDoc();
    discFlag &= haDisc.publish();

    haDisc.createClima(F("DHW"), FPSTR(MQTTSETVAR_DHWSETTEMP), mqtt.getVarSetTopic(MQTTSETVAR_DHWSETTEMP));
    haDisc.setMinMaxTemp(25, 70, 1);
    haDisc.setCurrentTemperatureTemplate(F("{{ value_json.boiler.dhw_t }}"));
    haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
    haDisc.setInitial(45);
    haDisc.setModeCommandTopic(mqtt.getVarSetTopic(MQTTSETVAR_DHWMODE));
    haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
    haDisc.setTemperatureStateTemplate(F("{{ value_json.thermostat.dhw_set_t }}"));
    haDisc.setOptimistic(true);
    haDisc.setIcon(F("mdi:shower"));
    haDisc.setRetain(true);
    haDisc.setModes(0x03);
    discFlag &= haDisc.publish();

    haDisc.createClima(F("flow set temperature"), FPSTR(MQTTSETVAR_CHSETTEMP1), mqtt.getVarSetTopic(MQTTSETVAR_CHSETTEMP1));
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

    haDisc.createClima(F("Room set temperature 1"), F("clima_room1"), mqtt.getVarSetTopic(MQTTSETVAR_ROOMSETPOINT1));
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

    haDisc.createClima(F("Room set temperature 2"), F("clima_room2"), mqtt.getVarSetTopic(MQTTSETVAR_ROOMSETPOINT2));
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
    
    haDisc.createNumber(F("Room temperature 1"), FPSTR(MQTTSETVAR_ROOMTEMP1), mqtt.getVarSetTopic(MQTTSETVAR_ROOMTEMP1));
    haDisc.setValueTemplate(F("{{ value_json.heatercircuit[0].roomtemp }}"));
    haDisc.setMinMax(5, 27, 0.1);
    if (!roomTemp[0].isMqttSource()) {
        haDisc.clearDoc();
    }
    discFlag &= haDisc.publish();

    haDisc.createNumber(F("room temperature 2"), FPSTR(MQTTSETVAR_ROOMTEMP2), mqtt.getVarSetTopic(MQTTSETVAR_ROOMTEMP2));
    haDisc.setValueTemplate(F("{{ value_json.heatercircuit[1].roomtemp }}"));
    haDisc.setMinMax(5, 27, 0.1);
    if (!roomTemp[1].isMqttSource())
        haDisc.clearDoc();
    discFlag &= haDisc.publish();

    return discFlag;
}

void OTControl::setDhwTemp(double temp) {
    dhwTemp = temp;
    setDhwRequest.force();
}

void OTControl::setDhwCtrlMode(const CtrlMode mode) {
    dhwOn = (mode != CtrlMode::CTRLMODE_OFF);
    setDhwRequest.force();
}

void OTControl::setConfig(JsonObject &config) {
    while (!master.hal.isReady()) {
        master.hal.process();
        yield();
    }
    while (!slave.hal.isReady()) {
        slave.hal.process();
        yield();
    }

    OTMode mode = OTMODE_BYPASS;

    if (config[F("otMode")].is<JsonInteger>())
        mode = (OTMode) (int) config[F("otMode")];

    JsonObject boiler = config[F("boiler")];

    for (int i=0; i<sizeof(heatingParams) / sizeof(heatingParams[0]); i++) {
        JsonObject hpObj = boiler[F("heating")][i];
        heatingParams[i].chOn = hpObj[F("chOn")];
        heatingParams[i].roomSet = hpObj[F("roomsetpoint")][F("temp")] | 21.0; // default room set point
        heatingParams[i].flowMax = hpObj[F("flowMax")] | 40;
        heatingParams[i].exponent = hpObj[F("exponent")] | 1.0;
        heatingParams[i].gradient = hpObj[F("gradient")] | 1.0;
        heatingParams[i].offset = hpObj[F("offset")] | 0;
        heatingParams[i].flowDefault = hpObj[F("flow")] | 35;
        heatingParams[i].overrideFlow = hpObj[F("overrideFlow")] | false;
        heatingParams[i].flow = heatingParams[i].flowDefault;
        heatingParams[i].ctrlMode = CTRLMODE_AUTO;
    }

    dhwOn = boiler[F("dhwOn")];
    dhwTemp = boiler[F("dhwTemperature")] | 45;
    overrideDhw = boiler[F("overrideDhw")] | false;

    masterMemberId = config[F("masterMemberId")] | 22;

    setOTMode(mode);

    setDhwRequest.force();
    setBoilerRequest[0].force();
    setBoilerRequest[1].force();

    master.resetCounters();
    slave.resetCounters();
}

void OTControl::setChCtrlMode(const CtrlMode mode, const uint8_t channel) {
    heatingParams[channel].chOn = (mode != CtrlMode::CTRLMODE_OFF);
    heatingParams[channel].ctrlMode = mode;
    setBoilerRequest[channel].force();
}

void OTControl::setChTemp(const double temp, const uint8_t channel) {
    if (temp == 0)
        heatingParams[channel].ctrlMode = CTRLMODE_AUTO;
    else
        heatingParams[channel].flow = temp;
    setBoilerRequest[channel].force();
}

void OTControl::forceFlowCalc(const uint8_t channel) {
    setBoilerRequest[channel].force();
}


OTWriteRequest::OTWriteRequest(OpenThermMessageID id, uint16_t intervalS):
        id(id),
        interval(intervalS) {
}

void OTWriteRequest::send(const uint16_t data) {
    nextMillis = millis() + interval * 1000;

    unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, id, data);
    otcontrol.sendRequest('T', req);
}

void OTWriteRequest::force() {
    nextMillis = 0;
}

OTWriteRequest::operator bool() {
    return (millis() > nextMillis);
}


OTWRSetDhw::OTWRSetDhw():
        OTWriteRequest(OpenThermMessageID::TdhwSet, 30) {
}

OTWRSetBoilerTemp::OTWRSetBoilerTemp(const uint8_t ch):
        OTWriteRequest(OpenThermMessageID::TSet, 10) {
    if (ch == 1)
        id = OpenThermMessageID::TsetCH2;
}

OTWRMasterConfigMember::OTWRMasterConfigMember():
        OTWriteRequest(OpenThermMessageID::MConfigMMemberIDcode, 60) {
}