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

// Testdata for local OT slave, can be read by a connected master
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
    {OpenThermMessageID::TrOverride2,               0},
    {OpenThermMessageID::TdhwSetUBTdhwSetLB,        nib(60, 40)}, // 60 °C upper bound, 40 C° lower bound
    {OpenThermMessageID::MaxTSetUBMaxTSetLB,        nib(60, 25)}, // 60 °C upper bound, 20 C° lower bound
    {OpenThermMessageID::SuccessfulBurnerStarts,    9999},
    {OpenThermMessageID::CHPumpStarts,              7777},
    {OpenThermMessageID::BurnerOperationHours,      8888},
    {OpenThermMessageID::OpenThermVersionSlave,     nib(2, 2)},
    {OpenThermMessageID::SlaveVersion,              nib(4, 4)},
    {OpenThermMessageID::StatusVentilationHeatRecovery, 0x001E},
    {OpenThermMessageID::RelVentLevel,              55}, // relative ventilation 0..100 %
    {OpenThermMessageID::RHexhaust,                 45},
    {OpenThermMessageID::CO2exhaust,                1450}, // PPM
    {OpenThermMessageID::Tsi,                       floatToOT(22.1)},
    {OpenThermMessageID::Tso,                       floatToOT(22.2)},
    {OpenThermMessageID::Tei,                       floatToOT(22.3)},
    {OpenThermMessageID::Teo,                       floatToOT(22.1)},
    {OpenThermMessageID::RPMexhaust,                2300},
    {OpenThermMessageID::RPMsupply,                 2400},
    {OpenThermMessageID::ASFflagsOEMfaultCodeVentilationHeatRecovery,   0x0F33},
    {OpenThermMessageID::OpenThermVersionVentilationHeatRecovery,       0x0105},
    {OpenThermMessageID::VentilationHeatRecoveryVersion,                0x0107},
    {OpenThermMessageID::RemoteOverrideFunction,    0x0000},
    {OpenThermMessageID::UnsuccessfulBurnerStarts,  19},
    {OpenThermMessageID::FlameSignalTooLowNumber,   4},
    {OpenThermMessageID::OEMDiagnosticCode,         123},
    {OpenThermMessageID::DHWBurnerOperationHours,   196},
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
    mutex = xSemaphoreCreateMutex();
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
        lastBoilerStatus(0),
        lastVentStatus(0),
        otMode(OTMODE_LOOPBACKTEST),
        master(GPIO_OTMASTER_IN, GPIO_OTMASTER_OUT, false),
        slave(GPIO_OTSLAVE_IN, GPIO_OTSLAVE_OUT, true),
        setBoilerRequest{OTWRSetBoilerTemp(0), OTWRSetBoilerTemp(1)},
        setRoomTemp{OTWRSetRoomTemp(0), OTWRSetRoomTemp(1)},
        setRoomSetPoint{OTWRSetRoomSetPoint(0), OTWRSetRoomSetPoint(1)},
        slaveApp(SLAVEAPP_HEATCOOL) {
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

void OTControl::setOTMode(const OTMode mode, const bool enableSlave) {
    otMode = mode;

    // set bypass relay
    digitalWrite(GPIO_BYPASS_RELAY, mode != OTMODE_BYPASS);

    // set +24V stepup up
    slaveEnabled = (mode == OTMODE_REPEATER) || (mode == OTMODE_LOOPBACKTEST) || enableSlave;
    digitalWrite(GPIO_STEPUP_ENABLE, slaveEnabled);

    for (auto *valobj: slaveValues)
        valobj->init((mode == OTMODE_MASTER) || (mode == OTMODE_LOOPBACKTEST));

    for (auto *valobj: thermostatValues)
        valobj->init(false);

    master.hal.setAlwaysReceive(mode == OTMODE_REPEATER);
    discFlag = false;
}

double OTControl::getFlow(const uint8_t channel) {
    HeatingConfig &hc = heatingConfig[channel];
    double flow = hc.flow;

    switch (heatingCtrl[channel].mode) {
    case CTRLMODE_ON:
        flow = heatingCtrl[channel].flowTemp;
        break;

    case CTRLMODE_AUTO: {
        double outTmp;
        if (outsideTemp.get(outTmp)) {
            double roomSet = hc.roomSet; // default room set point
            roomSetPoint[channel].get(roomSet);

            double minOutside = roomSet - (hc.flowMax - roomSet) / hc.gradient;
            double c1 = (hc.flowMax - roomSet) / pow(roomSet - minOutside, 1.0 / hc.exponent);
            flow = roomSet + c1 * pow(roomSet - outTmp, 1.0 / hc.exponent) + hc.offset;
        }
        double rst, rt;
        // room temperature compensation
        if ( roomSetPoint[channel].get(rst) && roomTemp[channel].get(rt) ) {
            flow += (rst - rt) * hc.roomTempComp;
        }
        if (flow > hc.flowMax)
            flow = hc.flowMax;
        if (flow < 0)
            flow = 0;
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

    if (xSemaphoreTake(master.mutex, (TickType_t) 50 / portTICK_PERIOD_MS) != pdTRUE)
        return;

    switch (otMode) {
    case OTMODE_LOOPBACKTEST: {
        for (int ch=0; ch<2; ch++) {
            if (setRoomTemp[ch]) {
                setRoomTemp[ch].send(OpenTherm::temperatureToData(21.5));
                xSemaphoreGive(master.mutex);
                return;
            }
            if (setRoomSetPoint[ch]) {
                setRoomSetPoint[ch].send(OpenTherm::temperatureToData(21.3));
                xSemaphoreGive(master.mutex);
                return;
            }
        }
        // no break!!

    }

    case OTMODE_MASTER:
        for (auto *valobj: slaveValues) {
            if (valobj->process()) {
                xSemaphoreGive(master.mutex);
                return;
            }
        }

        for (int ch=0; ch<2; ch++) {
            double temp;
            if (setRoomTemp[ch] && roomTemp[ch].get(temp)) {
                setRoomTemp[ch].send(OpenTherm::temperatureToData(temp));
                xSemaphoreGive(master.mutex);
                return;
            }
            if (setRoomSetPoint[ch] && roomSetPoint[ch].get(temp)) {
                setRoomSetPoint[ch].send(OpenTherm::temperatureToData(temp));
                xSemaphoreGive(master.mutex);
                return;
            }
        }

        switch (slaveApp) {
        case SLAVEAPP_HEATCOOL:
            for (int ch=0; ch<2; ch++) {
                if (heatingCtrl[ch].chOn && setBoilerRequest[ch]) {
                    double flow = getFlow(ch);

                    if (flow > 0) {
                        setBoilerRequest[ch].send(OpenTherm::temperatureToData(flow));
                        xSemaphoreGive(master.mutex);
                        return;
                    }
                }
            }

            if (setDhwRequest) {
                setDhwRequest.send(OpenTherm::temperatureToData(boilerCtrl.dhwTemp));
                xSemaphoreGive(master.mutex);
                return;
            }

            if (millis() > lastBoilerStatus + 800) {
                lastBoilerStatus = millis();

                unsigned long req = OpenTherm::buildSetBoilerStatusRequest(
                    heatingCtrl[0].chOn, 
                    boilerCtrl.dhwOn,
                    boilerConfig.coolOn,
                    boilerConfig.otc, 
                    heatingCtrl[1].chOn,
                    boilerConfig.summerMode,
                    boilerConfig.dhwBlocking);
                req |= statusReqOvl;
                sendRequest('T', req);
                xSemaphoreGive(master.mutex);
                return;
            }    
            break;

        case SLAVEAPP_VENT:
            if (setVentSetpointRequest) {
                setVentSetpointRequest.send(ventCtrl.setpoint);
                xSemaphoreGive(master.mutex);
                return;
            }

            if (millis() > lastVentStatus + 800) {
                lastVentStatus = millis();
                uint8_t hb = 0;
                if (ventCtrl.ventEnable)
                    hb |= 1<<0;
                if (ventCtrl.openBypass)
                    hb |= 1<<1;
                if (ventCtrl.autoBypass)
                    hb |= 1<<2;
                if (ventCtrl.freeVentEnable)
                    hb |= 1<<3;
                unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::StatusVentilationHeatRecovery, hb << 8);
                sendRequest('T', req);
                xSemaphoreGive(master.mutex);
                return;
            }
            break;
        default:
            break;
        }

        if (setMasterConfigMember) {
            setMasterConfigMember.send((1<<8) | masterMemberId);
            xSemaphoreGive(master.mutex);
            return;
        }
        break;
    }
    xSemaphoreGive(master.mutex);
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
 
    // we received response from connected slave (boiler, ventilation, solar storage)
    auto id = OpenTherm::getDataID(msg);
    auto mt = OpenTherm::getMessageType(msg);
    auto *otval = OTValue::getSlaveValue(id);

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
            if (boilerCtrl.overrideDhw) {
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

    master.onReceive((newMsg == msg) ? 'B' : 'A', msg);

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

    // we received a request from the room unit
    switch (otMode) {
    case OTMODE_MASTER: {
        unsigned long resp = OpenTherm::buildResponse(OpenThermMessageType::UNKNOWN_DATA_ID, id, 0x0000);
        slave.onReceive('S', msg);
        switch (mt) {
        case OpenThermMessageType::READ_DATA: {
            OTValue *otval = OTValue::getSlaveValue(id);
            switch (id) {
            case OpenThermMessageID::Toutside: {
                double t;
                if (outsideTemp.get(t))
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, OpenTherm::temperatureToData(t));
                break;
            }

            default:
                if ((otval != nullptr) && otval->isSet)
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, otval->getValue());
            }

            slave.sendResponse(resp, 'P');
            break;
        }
        case OpenThermMessageType::WRITE_DATA: {
            resp = OpenTherm::buildResponse(OpenThermMessageType::WRITE_ACK, id, 0x0000);
            slave.sendResponse(resp, 'P');

            switch (id) {
            case OpenThermMessageID::TSet:
                if (heatingCtrl[0].overrideFlow) {
                    heatingCtrl[0].mode = CTRLMODE_ON;
                    heatingCtrl[0].flowTemp = OpenTherm::getFloat(msg);
                    setBoilerRequest[0].force();
                }
                break;
            default:
                break;
            }
        }
        default:
            break;
        }
        break;
    }

    case OTMODE_REPEATER: {
        // forward received request to boiler
        // WRITE commands to boiler can be modified here

        switch (id) {
        case OpenThermMessageID::TSet:
            if ( (heatingCtrl[0].overrideFlow) && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(getFlow(0)));
            break;

        case OpenThermMessageID::TsetCH2:
            if ( (heatingCtrl[1].overrideFlow) && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(getFlow(1)));
            break;

        case OpenThermMessageID::TdhwSet:
            if ( boilerCtrl.overrideDhw && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(boilerCtrl.dhwTemp));
            break;

        case OpenThermMessageID::Status:
            if (heatingCtrl[0].overrideFlow) {
                if (heatingCtrl[0].mode == CtrlMode::CTRLMODE_OFF)
                    newMsg &= ~(1<<8); // CH1 disable
                else
                    newMsg |= 1<<8; // CH1 enable
            }
            if (heatingCtrl[1].overrideFlow) {
                if (heatingCtrl[1].mode == CtrlMode::CTRLMODE_OFF)
                    newMsg &= ~(1<<12); // CH2 disable
                else
                    newMsg |= 1<<12; // CH2 enable
            }
            if (boilerCtrl.overrideDhw) {
                if (boilerCtrl.dhwOn)
                    newMsg |= 1<<9; // DHW enable
                else
                    newMsg &= ~(1<<9); // DHW disable
            }                            
            
            newMsg = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, id, newMsg & 0xFFFF);
            break;
        }
        master.sendRequest(0, newMsg);
        slave.onReceive((msg == newMsg) ? 'T' : 'R', msg);
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

    if ( (mt == OpenThermMessageType::WRITE_DATA) || (id == OpenThermMessageID::Status) || (id == OpenThermMessageID::StatusVentilationHeatRecovery) ) {
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
        if (otMode != OTMODE_MASTER)
            if (!setThermostatVal(newMsg))
                portal.textAll(F("T no otval!"));
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
    JsonObject jSlave = obj[F("slave")].to<JsonObject>();
    for (auto *valobj: slaveValues)
        valobj->getJson(jSlave);

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
    jSlave[F("connected")] = slaveConnected;
    jSlave[F("txCount")] = master.txCount;
    jSlave[F("rxCount")] = master.rxCount;
    if ( (otMode == OTMODE_MASTER) || (otMode == OTMODE_LOOPBACKTEST) )
        jSlave[F("timeouts")] = master.timeoutCount;

    JsonObject thermostat = obj[F("thermostat")].to<JsonObject>();
    for (auto *valobj: thermostatValues)
        valobj->getJson(thermostat);

    if (slaveEnabled) {
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
}

bool OTControl::sendDiscovery() {
    for (auto *valobj: slaveValues)
        valobj->refreshDisc();

    for (auto *valobj: thermostatValues)
        valobj->refreshDisc();

    bool discFlag = true;

    haDisc.createNumber(F("Outside temperature"), Mqtt::getTopicString(Mqtt::TOPIC_OUTSIDETEMP), mqtt.getCmdTopic(Mqtt::TOPIC_OUTSIDETEMP));
    haDisc.setValueTemplate(F("{{ value_json.outsideTemp }}"));
    haDisc.setMinMax(-20, 20, 0.1);
    haDisc.setRetain(true);
    if (!outsideTemp.isMqttSource())
        haDisc.clearDoc();
    discFlag &= haDisc.publish();

    haDisc.createClima(F("DHW"), Mqtt::getTopicString(Mqtt::TOPIC_DHWSETTEMP), mqtt.getCmdTopic(Mqtt::TOPIC_DHWSETTEMP));
    if (slaveApp == SLAVEAPP_HEATCOOL) {
        haDisc.setMinMaxTemp(25, 70, 1);
        haDisc.setCurrentTemperatureTemplate(F("{{ value_json.slave.dhw_t }}"));
        haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
        haDisc.setInitial(45);
        haDisc.setModeCommandTopic(mqtt.getCmdTopic(Mqtt::TOPIC_DHWMODE));
        haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
        haDisc.setTemperatureStateTemplate(F("{{ value_json.thermostat.dhw_set_t }}"));
        haDisc.setOptimistic(true);
        haDisc.setIcon(F("mdi:shower"));
        haDisc.setRetain(true);
        haDisc.setModes(0x03);
    }
    else
        haDisc.clearDoc();
    discFlag &= haDisc.publish();

    haDisc.createClima(F("flow set temperature"), Mqtt::getTopicString(Mqtt::TOPIC_CHSETTEMP1), mqtt.getCmdTopic(Mqtt::TOPIC_CHSETTEMP1));
    if (slaveApp == SLAVEAPP_HEATCOOL) {
        haDisc.setMinMaxTemp(25, 90, 0.5);
        haDisc.setCurrentTemperatureTemplate(F("{{ value_json.slave.flow_t }}"));
        haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
        haDisc.setInitial(35);
        haDisc.setModeCommandTopic(mqtt.getCmdTopic(Mqtt::TOPIC_CHMODE1));
        haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
        haDisc.setTemperatureStateTemplate(F("{{ value_json.thermostat.ch_set_t }}"));
        haDisc.setOptimistic(true);
        haDisc.setIcon(F("mdi:heating-coil"));
        haDisc.setRetain(true);
    }
    else
        haDisc.clearDoc();
    discFlag &= haDisc.publish();

    haDisc.createClima(F("Room set temperature 1"), F("clima_room1"), mqtt.getCmdTopic(Mqtt::TOPIC_ROOMSETPOINT1));
    haDisc.setMinMaxTemp(12, 27, 0.5);
    haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
    haDisc.setCurrentTemperatureTemplate(F("{{ value_json.heatercircuit[0].roomtemp }}"));
    haDisc.setInitial(20);
    haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
    haDisc.setTemperatureStateTemplate(F("{{ value_json.heatercircuit[0].roomsetpoint }}"));
    haDisc.setOptimistic(true);
    haDisc.setRetain(true);
    if (!heatingConfig[0].chOn)
        haDisc.clearDoc();
    discFlag &= haDisc.publish();

    haDisc.createClima(F("Room set temperature 2"), F("clima_room2"), mqtt.getCmdTopic(Mqtt::TOPIC_ROOMSETPOINT2));
    haDisc.setMinMaxTemp(12, 27, 0.5);
    haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
    haDisc.setCurrentTemperatureTemplate(F("{{ value_json.heatercircuit[1].roomtemp }}"));
    haDisc.setInitial(20);
    haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
    haDisc.setTemperatureStateTemplate(F("{{ value_json.heatercircuit[1].roomsetpoint }}"));
    haDisc.setOptimistic(true);
    haDisc.setRetain(true);
    if (!heatingConfig[1].chOn)
        haDisc.clearDoc();
    discFlag &= haDisc.publish();
    
    haDisc.createNumber(F("Room temperature 1"), Mqtt::getTopicString(Mqtt::TOPIC_ROOMTEMP1), mqtt.getCmdTopic(Mqtt::TOPIC_ROOMTEMP1));
    haDisc.setValueTemplate(F("{{ value_json.heatercircuit[0].roomtemp }}"));
    haDisc.setMinMax(5, 27, 0.1);
    if (!roomTemp[0].isMqttSource()) {
        haDisc.clearDoc();
    }
    discFlag &= haDisc.publish();

    haDisc.createNumber(F("room temperature 2"), Mqtt::getTopicString(Mqtt::TOPIC_ROOMTEMP2), mqtt.getCmdTopic(Mqtt::TOPIC_ROOMTEMP2));
    haDisc.setValueTemplate(F("{{ value_json.heatercircuit[1].roomtemp }}"));
    haDisc.setMinMax(5, 27, 0.1);
    if (!roomTemp[1].isMqttSource())
        haDisc.clearDoc();
    discFlag &= haDisc.publish();

    haDisc.createNumber(F("ventilation set point"), Mqtt::getTopicString(Mqtt::TOPIC_VENTSETPOINT), mqtt.getCmdTopic(Mqtt::TOPIC_VENTSETPOINT));
    if (slaveApp == SLAVEAPP_VENT) {
        haDisc.setValueTemplate(F("{{ value_json.slave.rel_vent }}"));
        haDisc.setMinMax(0, 100, 1);
        haDisc.setOptimistic(true);
        haDisc.setRetain(true);
    }
    else
        haDisc.clearDoc();
    discFlag &= haDisc.publish();

    haDisc.createSwitch(F("ventilation enable"), Mqtt::TOPIC_VENTENABLE);
    haDisc.setValueTemplate(F("{{ value_json.slave.vent_status.vent_active }}"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_VENT);

    haDisc.createSwitch(F("open bypass"), Mqtt::TOPIC_OPENBYPASS);
    haDisc.setValueTemplate(F("{{ value_json.slave.vent_status.bypass_open }}"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_VENT);

    haDisc.createSwitch(F("auto bypass"), Mqtt::TOPIC_AUTOBYPASS);
    haDisc.setValueTemplate(F("{{ value_json.slave.vent_status.bypass_auto }}"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_VENT);

    haDisc.createSwitch(F("enable free vent."), Mqtt::TOPIC_FREEVENTENABLE);
    haDisc.setValueTemplate(F("{{ value_json.slave.vent_status.free_vent }}"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_VENT);

    haDisc.createSwitch(F("override CH1"), Mqtt::TOPIC_OVERRIDECH1);
    discFlag &= haDisc.publish(otMode == OTMODE_REPEATER);

    haDisc.createSwitch(F("override CH2"), Mqtt::TOPIC_OVERRIDECH2);
    discFlag &= haDisc.publish(otMode == OTMODE_REPEATER);

    haDisc.createSwitch(F("override DHW"), Mqtt::TOPIC_OVERRIDEDHW);
    discFlag &= haDisc.publish(otMode == OTMODE_REPEATER);

    return discFlag;
}

void OTControl::setDhwTemp(double temp) {
    boilerCtrl.dhwTemp = temp;
    setDhwRequest.force();
}

void OTControl::setDhwCtrlMode(const CtrlMode mode) {
    boilerCtrl.dhwOn = (mode != CtrlMode::CTRLMODE_OFF);
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

    for (int i=0; i<sizeof(heatingConfig) / sizeof(heatingConfig[0]); i++) {
        JsonObject hpObj = config[F("heating")][i];
        heatingConfig[i].chOn = hpObj[F("chOn")];
        heatingConfig[i].roomSet = hpObj[F("roomsetpoint")][F("temp")] | 21.0; // default room set point
        heatingConfig[i].flowMax = hpObj[F("flowMax")] | 40;
        heatingConfig[i].exponent = hpObj[F("exponent")] | 1.0;
        heatingConfig[i].gradient = hpObj[F("gradient")] | 1.0;
        heatingConfig[i].offset = hpObj[F("offset")] | 0;
        heatingConfig[i].flow = hpObj[F("flow")] | 35;
        heatingConfig[i].roomTempComp = hpObj[F("roomtempcomp")] | 0;
        heatingConfig[i].flow = heatingConfig[i].flow;
        
        heatingCtrl[i].flowTemp = heatingConfig[i].flow;
        heatingCtrl[i].chOn = heatingConfig[i].chOn;
        heatingCtrl[i].overrideFlow = hpObj[F("overrideFlow")] | false;
    }

    JsonObject ventObj = config[F("vent")];
    ventCtrl.ventEnable = ventObj["ventEnable"] | false;
    ventCtrl.openBypass = ventObj["openBypass"] | false;
    ventCtrl.autoBypass = ventObj["autoBypass"] | false;
    ventCtrl.freeVentEnable = ventObj["freeVentEnable"] | false;
    ventCtrl.setpoint = ventObj["setpoint"] | 0;

    JsonObject boiler = config[F("boiler")];
    boilerCtrl.dhwOn = boiler[F("dhwOn")];
    boilerCtrl.dhwTemp = boiler[F("dhwTemperature")] | 45;
    boilerCtrl.overrideDhw = boiler[F("overrideDhw")] | false;
    statusReqOvl = boiler[F("statusReq")] | 0x0000;
    boilerConfig.otc = boiler[F("otc")] | false;
    boilerConfig.summerMode = boiler[F("summerMode")] | false;
    boilerConfig.dhwBlocking = boiler[F("dhwBlocking")] | false;

    masterMemberId = config[F("masterMemberId")] | 22;

    slaveApp = (SlaveApplication) ((int) config[F("slaveApp")] | 0);

    setOTMode(mode, config[F("enableSlave")] | false);

    setDhwRequest.force();
    setBoilerRequest[0].force();
    setBoilerRequest[1].force();

    master.resetCounters();
    slave.resetCounters();
}

void OTControl::setChCtrlMode(const CtrlMode mode, const uint8_t channel) {
    heatingCtrl[channel].mode = mode;
    switch (mode) {
    case CTRLMODE_AUTO:
        heatingCtrl[channel].chOn = heatingConfig[channel].chOn;
        break;
    case CTRLMODE_OFF:
        heatingCtrl[channel].chOn = false;
        break;
    case CTRLMODE_ON:
        heatingCtrl[channel].chOn = true;
    }
    setBoilerRequest[channel].force();
}

void OTControl::setOverrideCh(const bool ovrd, const uint8_t channel) {
    heatingCtrl[channel].overrideFlow = ovrd;
    setBoilerRequest[channel].force();
}

void OTControl::setOverrideDhw(const bool ovrd) {
    boilerCtrl.overrideDhw = ovrd;
    setDhwRequest.force();
}

void OTControl::setChTemp(const double temp, const uint8_t channel) {
    if (temp == 0)
        heatingCtrl[channel].mode = CTRLMODE_AUTO;
    else
        heatingCtrl[channel].flowTemp = temp;
    setBoilerRequest[channel].force();
}

void OTControl::forceFlowCalc(const uint8_t channel) {
    setBoilerRequest[channel].force();
}

void OTControl::setVentSetpoint(const uint8_t v) {
    ventCtrl.setpoint = v;
    setVentSetpointRequest.force();
}

void OTControl::setVentEnable(const bool en) {
    ventCtrl.ventEnable = en;
}

unsigned long OTControl::slaveRequest(OpenThermMessageID id, OpenThermMessageType ty, uint16_t data) {
    unsigned long res = 0;

    if (xSemaphoreTake(master.mutex, (TickType_t)100 / portTICK_PERIOD_MS) == pdTRUE) {
        while (!master.hal.isReady()) {
            master.hal.process();
            yield();
        }
        unsigned long req = OpenTherm::buildRequest(ty, id, data);
        sendRequest('T', req);
        while (!master.hal.isReady()) {
            master.hal.process();
            slave.hal.process(); // in loopback mode slave objekt has to process our request!
            yield();
        }
        res = master.hal.getLastResponse();
        xSemaphoreGive(master.mutex);
    }
    return res;
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

OTWRSetVentSetpoint::OTWRSetVentSetpoint():
        OTWriteRequest(OpenThermMessageID::Vset, 60) {
}

OTWRSetRoomTemp::OTWRSetRoomTemp(const uint8_t ch):
        OTWriteRequest((ch == 0) ? OpenThermMessageID::Tr : OpenThermMessageID::TrCH2, 60) {
}

OTWRSetRoomSetPoint::OTWRSetRoomSetPoint(const uint8_t ch):
        OTWriteRequest((ch == 0) ? OpenThermMessageID::TrSet : OpenThermMessageID::TrSetCH2, 60) {
}