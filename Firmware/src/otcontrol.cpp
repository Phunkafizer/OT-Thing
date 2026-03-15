#include <WiFi.h>
#include "otcontrol.h"
#include "otvalues.h"
#include "command.h"
#include "HADiscLocal.h"
#include "mqtt.h"
#include "hwdef.h"
#include "portal.h"
#include "sensors.h"

const int PI_INTERVAL = 60; // seconds
const char SLAVE_BRAND[] PROGMEM = "Seegel Systeme";

using enum OpenThermMessageID;

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
    {SConfigSMemberIDcode,      nib(1<<0 | 1<<5, 1)}, // DHW & CH2 present, Member ID 1
    {ASFflags,                  0x0000}, // no error flags, oem error code 0
    {RBPflags,                  0x0101},
    {TrOverride,                0},
    {MaxCapacityMinModLevel,    nib(20, 5)}, // 20 kW / 5 %
    {RelModLevel,               floatToOT(33.3)},
    {CHPressure,                floatToOT(1.25)},
    {DHWFlowRate,               floatToOT(2.4)},
    {Tboiler,                   floatToOT(48.5)},
    {Tdhw,                      floatToOT(37.5)},
    {Toutside,                  floatToOT(3.5)},
    {Tret,                      floatToOT(41.7)},
    {TflowCH2,                  floatToOT(48.6)},
    {Tdhw2,                     floatToOT(37.6)},
    {Texhaust,                  90},
    {TrOverride2,               0},
    {TdhwSetUBTdhwSetLB,        nib(60, 40)}, // 60 °C upper bound, 40 C° lower bound
    {MaxTSetUBMaxTSetLB,        nib(60, 25)}, // 60 °C upper bound, 20 C° lower bound
    {PowerCycles,               159},
    {SuccessfulBurnerStarts,    9999},
    {CHPumpStarts,              7777},
    {DHWPumpValveStarts,        5544},
    {DHWBurnerStarts,           9955},
    {BurnerOperationHours,      8888},
    {CHPumpOperationHours,      6666},
    {DHWPumpValveOperationHours,5555},
    {DHWBurnerOperationHours,   2222},
    {OpenThermVersionSlave,     nib(2, 2)},
    {SlaveVersion,              nib(4, 4)},
    {StatusVentilationHeatRecovery, 0x001E},
    {RelVentLevel,              55}, // relative ventilation 0..100 %
    {RHexhaust,                 45},
    {CO2exhaust,                1450}, // PPM
    {Tsi,                       floatToOT(22.1)},
    {Tso,                       floatToOT(22.2)},
    {Tei,                       floatToOT(22.3)},
    {Teo,                       floatToOT(22.1)},
    {RPMexhaust,                2300},
    {RPMsupply,                 2400},
    {ASFflagsOEMfaultCodeVentilationHeatRecovery,   0x0F33},
    {OpenThermVersionVentilationHeatRecovery,       0x0105},
    {VentilationHeatRecoveryVersion,                0x0107},
    {RemoteOverrideFunction,    0x0000},
    {UnsuccessfulBurnerStarts,  19},
    {FlameSignalTooLowNumber,   4},
    {OEMDiagnosticCode,         123},
    {TboilerHeatExchanger,      floatToOT(48.5)},
    {BoilerFanSpeedSetpointAndActual, nib(20, 21)},
    {FlameCurrent,              floatToOT(96.8)},
};


void clip(double &d, const double min, const double max) {
    if (d < min)
        d = min;
    if (d > max)
        d = max;
}

class SemMaster {
private:
    BaseType_t result;
public:
    SemMaster(const uint16_t timeout) {
        result = xSemaphoreTakeRecursive(otcontrol.master.mutex, (TickType_t) timeout / portTICK_PERIOD_MS);
        wait();
    }

    ~SemMaster() {
        if (result == true) {
            wait();
            xSemaphoreGiveRecursive(otcontrol.master.mutex);
        }
    }

    operator bool() {
        return (result == pdTRUE) && otcontrol.master.hal.isReady();
    }

    void wait() {
        if (result == pdTRUE) {
            TickType_t start = xTaskGetTickCount();

            while (!otcontrol.master.hal.isReady()) {
                if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(2000))
                    break;

                otcontrol.hwYield();
            }
        }
    }
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
    mutex = xSemaphoreCreateRecursiveMutex();
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


void OTControl::FlameStats::set(const bool flame) {
    if (currentFlame != flame) {
        if (lastLoop == 0) {
            // this is first flame on within 1st minute
            memset(on.buf, 60, sizeof(on.buf));
            on.sum = BUFSIZE_MINUTES * 60UL;
        }

        if (flame) {
            cycles.current++;
            offTimes.current = (millis() - lastEdge) / 1000;
            if ((lastEdge > 0) && !offTimesInit) {
                // this is first flame on after 1st flame off, initialize off times buffer
                for (int i=0; i<BUFSIZE_CYCLES; i++)
                    offTimes.buf[i] = offTimes.current;
                offTimes.sum = offTimes.current * BUFSIZE_CYCLES;
                offTimesInit = true;
            }
            offTimes.update(idxCycles);
        }
        else {
            onTimes.current = (millis() - lastEdge) / 1000;
            if ((lastEdge > 0) && !onTimesInit) {
                // this is first flame off after 1st flame on, initialize on times buffer
                for (int i=0; i<BUFSIZE_CYCLES; i++)
                    onTimes.buf[i] = onTimes.current;
                onTimes.sum = onTimes.current * BUFSIZE_CYCLES;
                onTimesInit = true;
            }
            onTimes.update(idxCycles);
            idxCycles = (idxCycles + 1) % BUFSIZE_CYCLES;
        }
        
        update();
        lastEdge = millis();
        currentFlame = flame;
    }
}

void OTControl::FlameStats::update() {
    if (currentFlame) {
        uint8_t diff = 0;
        if (lastEdge > lastLoop)
            diff = (millis() - lastEdge) / 1000;
        else
            diff = (millis() - lastLoop) / 1000;
        on.current += diff;
    }
}

uint8_t OTControl::FlameStats::getDuty() const {
    return 100 * on.sum / BUFSIZE_MINUTES / 60;
}

double OTControl::FlameStats::getFreq() const {
    return round(cycles.sum / (BUFSIZE_MINUTES / 60.0) * 10) / 10.0;
}

double OTControl::FlameStats::getOnTime() const {
    return round(onTimes.sum / BUFSIZE_CYCLES / 60.0 * 10) / 10.0;
}

double OTControl::FlameStats::getOffTime() const {
    return round(offTimes.sum / BUFSIZE_CYCLES / 60.0 * 10) / 10.0;
}

void OTControl::FlameStats::loop() {
    OTValueStatus *ots = static_cast<OTValueStatus*>(OTValue::getSlaveValue(Status));
    if (ots)
        set(ots->getFlame());

    if (millis() >= lastLoop + 60000) {
        update();

        on.update(idxMinutes);
        cycles.update(idxMinutes);
        
        idxMinutes = (idxMinutes + 1) % BUFSIZE_MINUTES;
        lastLoop = millis();
    }
}

template <typename T1, size_t T2>
void OTControl::FlameStats::Ringbuf<T1, T2>::update(const uint8_t idx) {
    sum -= buf[idx]; // remove old value
    buf[idx] = current;
    sum += current;
    current = 0;
}

void OTControl::FlameStats::writeJson(JsonObject &obj) const {
    JsonObject fs = obj[F("flameStats")].to<JsonObject>();
    fs["duty"] = getDuty();
    fs["freq"] = getFreq();
    if (onTimesInit)
        fs["onTime"] = getOnTime();
    if (offTimesInit)
        fs["offTime"] = getOffTime();
}


OTControl::OTControl():
        lastBoilerStatus(0),
        lastVentStatus(0),
        otMode(OTMODE_LOOPBACKTEST),
        slaveApp(SLAVEAPP_HEATCOOL),
        setBoilerRequest{OTWRSetBoilerTemp(0), OTWRSetBoilerTemp(1)},
        setRoomTemp{OTWRSetRoomTemp(0), OTWRSetRoomTemp(1)},
        setRoomSetPoint{OTWRSetRoomSetPoint(0), OTWRSetRoomSetPoint(1)},
        master(GPIO_OTMASTER_IN, GPIO_OTMASTER_OUT, false),
        slave(GPIO_OTSLAVE_IN, GPIO_OTSLAVE_OUT, true) {
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

uint16_t OTControl::tmpToData(const double tmpf) {
    if (tmpf > 100)
        return 100<<8;
    if (tmpf < -100)
        return - (int) (100<<8);
    
    return (int16_t) (tmpf * 256);
}

void OTControl::setOTMode(const OTMode mode) {
    otMode = mode;

    // set bypass relay
    digitalWrite(GPIO_BYPASS_RELAY, (mode != OTMODE_BYPASS) && !bypass);

    // set +24V stepup up
    digitalWrite(GPIO_STEPUP_ENABLE, enableSlave && !bypass);

    for (auto *valobj: slaveValues)
        valobj->init((mode == OTMODE_MASTER) || (mode == OTMODE_LOOPBACKTEST));

    for (auto *valobj: thermostatValues)
        valobj->init(false);

    master.hal.setAlwaysReceive(mode == OTMODE_REPEATER);
    discFlag = false;
}
void OTControl::setBypass(const bool bypass) {
    this->bypass = bypass;
    setOTMode(otMode);
}

double OTControl::getFlow(const uint8_t channel) {
    HeatingConfig &hc = heatingLogic[channel].config;
    HeatingControl &hctrl = heatingCtrl[channel];
    double flow = hc.flow;
    double outTmp = 0.0;
    double roomSet = hc.baseTemp;

    switch (hctrl.mode) {
    case CTRLMODE_ON:
        flow = hctrl.flowTemp;
        break;

    case CTRLMODE_AUTO: {
        static double lastOutsideTemp[NUM_HEATCIRCUITS] = {0};
        static bool lastOutsideValid[NUM_HEATCIRCUITS] = {false};
        double outTemp;
        if (outsideTemp.get(outTemp)) {
            lastOutsideTemp[channel] = outTemp;
            lastOutsideValid[channel] = true;
        }
        if (lastOutsideValid[channel]) {
            outTmp = lastOutsideTemp[channel];
            roomSetPoint[channel].get(roomSet);
            hc.baseTemp = roomSet;
            heatingLogic[channel].updateOutdoorTemp(outTmp);
            flow = heatingLogic[channel].getCalculatedSetpoint();
        }
        break;
    }

    case CTRLMODE_OFF:
        return 0;
    
    default:
        break;
    }

    if (hctrl.suspended)
        return 0;

    // room temperature compensation
    flow += hctrl.piCtrl.deltaT;

    if (hc.minSuspend && (flow <= hctrl.flowMin))
        return 0;

    clip(flow, hctrl.flowMin, hc.tMax);

    return flow;
}

void OTControl::hwYield() {
    vTaskDelay(1);
    master.hal.process();
    slave.hal.process();
    
    if (otMode == OTMODE_MASTER) {
        // in OTMASTER mode use OT LEDs as master TX & RX
        if (millis() > master.lastTx + 50)
            setLedOTRed(false);
    }
}

void OTControl::loop() {
    if (millis() > nextPiCtrl) {
        loopPiCtrl();
        nextPiCtrl = millis() + PI_INTERVAL * 1000;
    }

    if (!discFlag)
        discFlag = sendDiscovery();

    flameStats.loop();

    SemMaster sem(10);
    if (!sem)
        return;
    
    bool hasDHW = false;
    bool hasCh2 = false;
    OTValueSlaveConfigMember *sc = OTValue::getSlaveConfig();
    if (sc != nullptr) {
        hasDHW = sc->hasDHW();
        hasCh2 = sc->hasCh2();
    }

    auto isChOn = [&](const uint8_t channel) {
        const HeatingControl &hctrl = heatingCtrl[channel];
        if (!hctrl.chOn)
            return false;
        if (hctrl.mode == CTRLMODE_OFF)
            return false;
        if (hctrl.suspended)
            return false;
        return true;
    };

    switch (otMode) {
    case OTMODE_LOOPBACKTEST: {
        for (int ch=0; ch<(hasCh2 ? 2 : 1); ch++) {
            if (setRoomTemp[ch]) {
                setRoomTemp[ch].sendFloat(20.1 + ch);
                return;
            }
            if (setRoomSetPoint[ch]) {
                setRoomSetPoint[ch].sendFloat(21.3 + ch);
                return;
            }
        }
    }
    [[fallthrough]];

    case OTMODE_MASTER:
        if (setProdVersion) {
            setProdVersion.send(0x0100);
            return;
        }

        if (setOTVersion) {
            setOTVersion.send(0x0402);
            return;
        }

        if (setMasterConfigMember) {
            setMasterConfigMember.send((1<<8) | masterMemberId);
            return;
        }
        
        for (auto *valobj: slaveValues) {
            if (valobj->process()) {
                return;
            }
        }

        for (int ch=0; ch<(hasCh2 ? 2 : 1); ch++) {
            double temp;
            if (setRoomTemp[ch] && roomTemp[ch].get(temp)) {
                setRoomTemp[ch].sendFloat(temp);
                return;
            }
            if (setRoomSetPoint[ch] && roomSetPoint[ch].get(temp)) {
                setRoomSetPoint[ch].sendFloat(temp);
                return;
            }
        }

        if ( (slaveApp == SLAVEAPP_HEATCOOL) || (otMode == OTMODE_LOOPBACKTEST) ) {
            for (int ch=0; ch<(hasCh2 ? 2 : 1); ch++) {
                if (heatingCtrl[ch].chOn && setBoilerRequest[ch]) {
                    double flow = getFlow(ch);
                    if (flow > 0) {
                        setBoilerRequest[ch].sendFloat(flow);
                        return;
                    }
                }
            }

            if (hasDHW && setDhwRequest) {
                setDhwRequest.sendFloat(boilerCtrl.dhwTemp);
                return;
            }

            if (!outsideTemp.isOtSource() && setOutsideTemp) {
                double t;
                if (outsideTemp.get(t)) {
                    setOutsideTemp.sendFloat(t);
                    return;
                }
            }

            if (setMaxModulation) {
                setMaxModulation.sendFloat(boilerCtrl.maxModulation);
                return;
            }

            if (setMaxCh) {
                double maxCh = heatingLogic[0].config.tMax;
                if (hasCh2 && (heatingLogic[1].config.tMax > maxCh))
                    maxCh = heatingLogic[1].config.tMax;
                setMaxCh.sendFloat(maxCh);
            }

            if (millis() > lastBoilerStatus + 800) {
                lastBoilerStatus = millis();
                unsigned long req = OpenTherm::buildSetBoilerStatusRequest(
                    isChOn(0),
                    boilerCtrl.dhwOn,
                    boilerConfig.coolOn,
                    boilerConfig.otc, 
                    isChOn(1),
                    boilerConfig.summerMode,
                    boilerConfig.dhwBlocking);
                req |= statusReqOvl;
                sendRequest('T', req);
                return;
            }  
        }

        if ( (slaveApp == SLAVEAPP_VENT) || (otMode == OTMODE_LOOPBACKTEST) ) {
            if (setVentSetpointRequest) {
                setVentSetpointRequest.send(ventCtrl.setpoint);
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
                unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, StatusVentilationHeatRecovery, hb << 8);
                sendRequest('T', req);
                return;
            }
        }

        break;

    default:
        break;
    }
}

void OTControl::loopPiCtrl() {
    OTValueStatus *ots = static_cast<OTValueStatus*>(OTValue::getSlaveValue(Status));
    for (int i=0; i<NUM_HEATCIRCUITS; i++) {
        setBoilerRequest[i].force();

        HeatingControl::PiCtrl &pictrl = heatingCtrl[i].piCtrl;
        HeatingConfig &hc = heatingLogic[i].config;
        double rt, rsp; // roomtemp, roomsetpoint

        if (!hc.enableHyst)
            heatingCtrl[i].suspended = false;

        pictrl.deltaT = 0;

        if (!roomTemp[i].get(rt))
            continue;

        if (!roomSetPoint[i].get(rsp))
            continue;

        if (pictrl.init)
            pictrl.roomTempFilt = 0.1 * rt + 0.9 * pictrl.roomTempFilt;
        else {
            pictrl.roomTempFilt = rt;
            pictrl.rspPrev = rsp;
        }
        pictrl.init = true;


        if (hc.enableHyst) {
            if (heatingCtrl[i].suspended) {
                if (rt < rsp - hc.hysteresis)
                    heatingCtrl[i].suspended = false;
            }
            else {
                if (rt > rsp + hc.hysteresis + hc.suspOffset)
                    heatingCtrl[i].suspended = true;
            }
        }

        if (!pictrl.enabled) {
            pictrl.integState = 0;
            pictrl.deltaT = 0;
            continue;
        }

        double e = rsp - rt; // error
        if ((e > -0.2) && (e < 0.2)) // deadband
            e = 0;

        // proportional part of PI controller
        double p = hc.roomComp.p * e; // Kp * e
        
        // integral part of PI controller
        pictrl.integState += rsp - pictrl.rspPrev;
        pictrl.rspPrev = rsp;
        if ( (ots != nullptr) && ots->getChActive(i) && !heatingCtrl[i].suspended ) {
            if (e > 0)
                pictrl.integState += hc.roomComp.i * e * PI_INTERVAL / 3600.0; // Ki * e * ts, ts = 60 s
            else
                pictrl.integState += hc.roomComp.i * e * 0.3 * PI_INTERVAL / 3600.0; // slower as cooling takes more time
        }
        else
            pictrl.integState = pictrl.integState * 0.95; // decay

        // anti windup
        clip(pictrl.integState, -5, 5);

        double boost = 0;
        if (e > 1.0)
            boost = e * hc.roomComp.boost; // e * Kb

        pictrl.deltaT = p + pictrl.integState + boost;
        // clipping
        clip(pictrl.deltaT, -5, 12);
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
    if (status == OpenThermResponseStatus::TIMEOUT) {
        master.timeoutCount++;
        command.sendAll(FPSTR("RX master timeout"));
        return;
    }
  
    // we received response from connected slave (boiler, ventilation, solar storage)
    auto id = OpenTherm::getDataID(msg);
    auto mt = OpenTherm::getMessageType(msg);
    auto *otval = OTValue::getSlaveValue(id);
    unsigned long newMsg = msg;

    switch (mt) {
    case OpenThermMessageType::READ_DATA:
    case OpenThermMessageType::WRITE_DATA: {
        String log = FPSTR("RX master invalid: 0x");
        log += String(msg, HEX);
        command.sendAll(log);
        return;
    }
    default:
        break;
    }

    if (otMode == OTMODE_REPEATER) {
        // forward reply from boiler to room unit
        // replies can be modified here

        switch (id) {
        case Toutside: {
            double ost;
            if ( !outsideTemp.isOtSource() && outsideTemp.get(ost) && (mt != OpenThermMessageType::WRITE_ACK) )
                newMsg = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmpToData(ost));
            break;
        }
        case TdhwSet: {
            if (boilerCtrl.overrideDhw && (mt == OpenThermMessageType::READ_ACK)) {
                // roomunit tried to read dhw set temp. Catch it in order to force writing DHW setpoint by roomunit.
                newMsg = OpenTherm::buildResponse(OpenThermMessageType::DATA_INVALID, id, 0x0000);
            }
            break;
        }
        default:
            break;
        }

        slave.sendResponse(newMsg);
    }


    char c;
    if ((status == OpenThermResponseStatus::INVALID) && (otMode != OTMODE_REPEATER))
        c = 'E';
    else
        c = (newMsg == msg) ? 'B' : 'A';
    master.onReceive(c, msg);

    if (otval) {
        otval->setValue(mt, newMsg & 0xFFFF);
        switch (mt) {
        case OpenThermMessageType::READ_ACK:
            switch (id) {
            case Toutside:
                outsideTemp.set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            case Tr:
                roomTemp[0].set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            case TrCH2:
                roomTemp[1].set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            case TrSet:
                roomSetPoint[0].set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            case TrSetCH2:
                roomSetPoint[1].set(OpenTherm::getFloat(msg), OutsideTemp::SOURCE_OT);
                break;
            default:
                break;
            }
            break;

        default:
            break;
        }
    }

    if (!otval && (mt == OpenThermMessageType::READ_ACK))
        portal.textAll(F("no slave val!"));
}

unsigned long OTControl::buildBrandResponse(const OpenThermMessageID id, const String &str, const uint8_t idx) {
    uint16_t msg = (str.length() + 1) << 8;
    if ((idx) < str.length())
        msg |= str[idx];

    return OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, msg);
}

void OTControl::OnRxSlave(const unsigned long msg, const OpenThermResponseStatus status) {
    if (status == OpenThermResponseStatus::INVALID) {
        slave.invalidCount++;
        return;
    }

    // we received a request from connected room unit

    auto id = OpenTherm::getDataID(msg);
    auto mt = OpenTherm::getMessageType(msg);
    unsigned long newMsg = msg;

    switch (mt) {
    case OpenThermMessageType::READ_DATA:
    case OpenThermMessageType::WRITE_DATA: {
        break;
    }
    default:
        String log = FPSTR("RX slave invalid: 0x");
        log += String(msg, HEX);
        command.sendAll(log);
        return;
    }

    switch (otMode) {
    case OTMODE_MASTER: {
        unsigned long resp = OpenTherm::buildResponse(OpenThermMessageType::UNKNOWN_DATA_ID, id, 0x0000);
        slave.onReceive('S', msg);
        switch (mt) {
        case OpenThermMessageType::READ_DATA: {
            OTValue *otval = OTValue::getSlaveValue(id);
            switch (id) {
            case Toutside: {
                double t;
                if (outsideTemp.get(t))
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmpToData(t));
                break;
            }

            case DayTime: {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    const uint16_t tmp = ((((timeinfo.tm_wday + 1) % 7) + 1) << 13) | (timeinfo.tm_hour << 8) | timeinfo.tm_min;
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmp);
                }
                break;
            }

            case Date: {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    const uint16_t tmp = ((timeinfo.tm_mon + 1) << 8) | timeinfo.tm_mday;
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmp);
                }
                break;
            }

            case Year: {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    const uint16_t tmp = timeinfo.tm_year + 1900;
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmp);
                }
                break;
            }

            case Brand: {
                String brand = PSTR(SLAVE_BRAND);
                resp = buildBrandResponse(id, brand, msg >> 8);
                break;
            }

            case BrandVersion: {
                String brandVersion = PSTR(BUILD_VERSION);
                resp = buildBrandResponse(id, brandVersion, msg >> 8);
                break;
            }

            case BrandSerialNumber: {
                String mac = WiFi.macAddress();
                resp = buildBrandResponse(id, mac, msg >> 8);
                break;
            }

            default:
                if ((otval != nullptr) && otval->hasReply())
                    resp = OpenTherm::buildResponse(otval->getLastMsgType(), id, otval->getValue());
            }

            slave.sendResponse(resp, 'P');
            break;
        }
        case OpenThermMessageType::WRITE_DATA: {
            resp = OpenTherm::buildResponse(OpenThermMessageType::WRITE_ACK, id, 0x0000);
            slave.sendResponse(resp, 'P');

            switch (id) {
            case TSet:
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
        case TSet:
            if ( (heatingCtrl[0].overrideFlow) && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(getFlow(0)));
            break;

        case TsetCH2:
            if ( (heatingCtrl[1].overrideFlow) && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(getFlow(1)));
            break;

        case TdhwSet:
            if ( boilerCtrl.overrideDhw && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(boilerCtrl.dhwTemp));
            break;

        case Status:
            for (int i=0; i<NUM_HEATCIRCUITS; i++) {
                if (heatingCtrl[i].overrideFlow) {
                    const uint8_t bit = (i == 0) ? 8 : 12;
                    bool chOn = heatingCtrl[i].chOn &&
                        (heatingCtrl[i].mode != CTRLMODE_OFF) &&
                        !heatingCtrl[i].suspended;
                    if (chOn)
                        newMsg |= 1<<bit; // CHx enable
                    else
                        newMsg &= ~(1<<bit); // CHx disable
                }
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

        default:
            break;
        }
        slave.onReceive((msg == newMsg) ? 'T' : 'R', msg);
        SemMaster sem(500);
        if (sem)
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

            switch (id) {
            case Status: {
                uint8_t temp = millis() / 206723;
                uint8_t x = ((temp % 3) == 0) ? 0 : 1;
                uint16_t data = x<<3; // flame on
                if ((msg & (1<<8)) != 0)
                    data |= 1<<1; // CH1 enable -> CH1 active
                if ((msg & (1<<9)) != 0)
                    data |= 1<<2;  // DHW enable -> DHW active
                if ((msg & (1<<10)) != 0)
                    data |= 1<<4;  // Cooling enable -> cooling active
                if ((msg & (1<<12)) != 0)
                    data |= 1<<5;  // CH2 enable -> CH2 active
                reply = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, data);
                break;
            }

            case Brand: {
                String brand = PSTR(SLAVE_BRAND);
                reply = buildBrandResponse(id, brand, msg >> 8);
                break;
            }

            case BrandVersion: {
                String brandVersion = PSTR(BUILD_VERSION);
                reply = buildBrandResponse(id, brandVersion, msg >> 8);
                break;
            }

            case BrandSerialNumber: {
                String mac = WiFi.macAddress();
                reply = buildBrandResponse(id, mac, msg >> 8);
                break;
            }

            default:
                for (unsigned int i = 0; i< sizeof(loopbackTestData) / sizeof(loopbackTestData[0]); i++) {
                    if (loopbackTestData[i].id == id) {
                        reply = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, loopbackTestData[i].value);
                        break;
                    }
                }
                break;
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

    if ( (mt == OpenThermMessageType::WRITE_DATA) || 
         (id == Status) || 
         (id == StatusVentilationHeatRecovery) ||
         (id == TrSet) // roomunit "RAM 786" sends TrSet as READ command (out of spec!)
       ) {
        double d = OpenTherm::getFloat(newMsg);
        switch (id) {
        case Tr:
            roomTemp[0].set(d, Sensor::SOURCE_OT);
            break;
        case TrSet:
            roomSetPoint[0].set(d, Sensor::SOURCE_OT);
            break;
        case TrCH2:
            roomTemp[1].set(d, Sensor::SOURCE_OT);
            break;
        case TrSetCH2:
            roomSetPoint[1].set(d, Sensor::SOURCE_OT);
            break;
        default:
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
            valobj->setValue(OpenThermMessageType::WRITE_DATA, msg & 0xFFFF);
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
    
    if (OTValue::getSlaveValue(Status)->isSet())
        flameStats.writeJson(jSlave);

    JsonObject thermostat = obj[F("thermostat")].to<JsonObject>();
    for (auto *valobj: thermostatValues)
        valobj->getJson(thermostat);

    JsonArray hcarr = obj[F("heatercircuit")].to<JsonArray>();
    for (int i=0; i<NUM_HEATCIRCUITS; i++) {
        JsonObject hc = hcarr.add<JsonObject>();
        HeatingControl &hctrl = heatingCtrl[i];
        double d;

        if (roomSetPoint[i].get(d)) {
            hc[F("roomsetpoint")] = d;
            hc[F("roomTempFilt")] = hctrl.piCtrl.roomTempFilt;
        }
        
        if (roomTemp[i].get(d))
            hc[F("roomtemp")] = d;

        const double smoothed = heatingLogic[i].getSmoothedTemp();
        if (smoothed > -900.0)
            hc[F("outsideTempSmoothed")] = smoothed;
        hc[F("summerMode")] = heatingLogic[i].isSummer();

        hc[F("ovrdFlow")] = hctrl.overrideFlow;
        hc[F("flowMin")] = hctrl.flowMin;
        hc[F("mode")] = (int) hctrl.mode;
        hc[F("integState")] = round(hctrl.piCtrl.integState * 100) / 100.0;
        if (heatingLogic[i].config.enableHyst)
            hc[F("suspended")] = hctrl.suspended;
    }

    if (enableSlave) {
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

    obj[F("bypass")] = bypass;
}

bool OTControl::sendDiscovery() {
    for (auto *valobj: slaveValues)
        valobj->refreshDisc();

    for (auto *valobj: thermostatValues)
        valobj->refreshDisc();

    bool discFlag = true;

    haDisc.createNumber(F("outside temperature"), Mqtt::getTopicString(Mqtt::TOPIC_OUTSIDETEMP), mqtt.getCmdTopic(Mqtt::TOPIC_OUTSIDETEMP));
    haDisc.setValueTemplate(F("{{ value_json.outsideTemp | default(None) }}"));
    haDisc.setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    haDisc.setUnit(FPSTR(HA_UNIT_CELSIUS));
    haDisc.setMinMax(-25, 20, 0.1);
    haDisc.setRetain(true);
    discFlag &= haDisc.publish(outsideTemp.isMqttSource());

    discFlag &= sendChDiscoveries(0, true);

    haDisc.createNumber(F("ventilation set point"), Mqtt::getTopicString(Mqtt::TOPIC_VENTSETPOINT), mqtt.getCmdTopic(Mqtt::TOPIC_VENTSETPOINT));
    haDisc.setValueTemplate(F("{{ value_json.slave.rel_vent }}"));
    haDisc.setMinMax(0, 100, 1);
    haDisc.setOptimistic(true);
    haDisc.setRetain(true);
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_VENT);

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

    haDisc.createNumber(F("Max. modulation"), Mqtt::getTopicString(Mqtt::TOPIC_MAXMODULATION), mqtt.getCmdTopic(Mqtt::TOPIC_MAXMODULATION));
    haDisc.setMinMax(0, 100, 1);
    haDisc.setValueTemplate(F("{{ value_json.thermostat.max_rel_mod | default(None) }}"));
    haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
    discFlag &= haDisc.publish();

    haDisc.createSensor(F("flame ratio"), F("flame_ratio"));
    haDisc.setValueTemplate(F("{{ value_json.slave.flameStats.duty | default(None) }}"));
    haDisc.setDeviceClass(F("power_factor"));
    haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("burner starts /h"), F("flame_freq"));
    haDisc.setValueTemplate(F("{{ value_json.slave.flameStats.freq | default(None) }}"));
    haDisc.setUnit(F("/h"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("flametime per cycle"), F("flame_on"));
    haDisc.setValueTemplate(F("{{ value_json.slave.flameStats.onTime | default(None) }}"));
    haDisc.setUnit(F("min"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("pausetime per cycle"), F("flame_off"));
    haDisc.setValueTemplate(F("{{ value_json.slave.flameStats.offTime | default(None) }}"));
    haDisc.setUnit(F("min"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSwitch(F("bypass"), Mqtt::TOPIC_BYPASS);
    haDisc.setValueTemplate(F("{{ value_json.bypass }}"));
    discFlag &= haDisc.publish();

    return discFlag;
}

bool OTControl::sendChDiscoveries(const uint8_t ch, const bool en) {
    auto replace = [](const char *str, const uint8_t val, const uint8_t ommit = -1) {
        String result = FPSTR(str);
        if (val == ommit)
            result.replace("#", "");
        else
            result.replace("#", String(val));

        result.replace("?", F("| default(None)"));
        result.trim();
        return result;
    };

    auto topic = [](const Mqtt::MqttTopic topic, const uint8_t ch) {
        return (Mqtt::MqttTopic) ((int) topic + ch);
    };

    String str = replace(PSTR("flow temperature #"), ch + 1, 1);
    Mqtt::MqttTopic tp = topic(Mqtt::TOPIC_CHSETTEMP1, ch);
    haDisc.createClima(str, Mqtt::getTopicString(tp), mqtt.getCmdTopic(tp));
    haDisc.setMinMaxTemp(20, heatingLogic[ch].config.tMax, 0.5);
    str = replace(PSTR("{{ value_json.slave.flow_t# ? }}"), ch + 1, 1);
    haDisc.setCurrentTemperatureTemplate(str);
    haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
    haDisc.setInitial(35);
    haDisc.setModeCommandTopic(mqtt.getCmdTopic(topic(Mqtt::TOPIC_CHMODE1, ch)));
    haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
    str = replace(PSTR("{{ value_json.thermostat.ch_set_t# ? }}"), ch + 1, 1);
    haDisc.setTemperatureStateTemplate(str);
    haDisc.setOptimistic(true);
    haDisc.setIcon(F("mdi:heating-coil"));
    haDisc.setRetain(true);
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("room temperature #"), ch + 1, 1);
    tp = topic(Mqtt::TOPIC_ROOMSETPOINT1, ch);
    haDisc.createClima(str, Mqtt::getTopicString(tp), mqtt.getCmdTopic(tp));
    haDisc.setMinMaxTemp(10, 30, 0.5);
    haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
    str = replace(PSTR("{{ value_json.heatercircuit[#].roomtemp ? }}"), ch);
    haDisc.setCurrentTemperatureTemplate(str);
    haDisc.setInitial(20);
    tp = topic(Mqtt::TOPIC_ROOMCOMP1, ch);
    haDisc.setModeCommandTopic(mqtt.getCmdTopic(tp));
    haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
    str = replace(PSTR("{{ value_json.heatercircuit[#].roomsetpoint ? }}"), ch);
    haDisc.setTemperatureStateTemplate(str);
    haDisc.setOptimistic(true);
    haDisc.setRetain(true);
    haDisc.setModes(0x06);
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("room setpoint #"), ch + 1, 1);
    tp = topic(Mqtt::TOPIC_ROOMSETPOINT1, ch);
    haDisc.createTempSensor(str, Mqtt::getTopicString(tp));
    haDisc.setStateTopic(mqtt.getCmdTopic(tp));
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("room temperature #"), ch + 1, 1);
    tp = topic(Mqtt::TOPIC_ROOMTEMP1, ch);
    haDisc.createNumber(str, Mqtt::getTopicString(tp), mqtt.getCmdTopic(tp));
    str = replace(PSTR("{{ value_json.heatercircuit[#].roomtemp ? }}"), ch);
    haDisc.setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    haDisc.setUnit(FPSTR(HA_UNIT_CELSIUS));
    haDisc.setValueTemplate(str);
    haDisc.setMinMax(0, 30, 0.1);
    if (!haDisc.publish(roomTemp[ch].isMqttSource() && en))
        return false;

    str = replace(PSTR("room temperature #"), ch + 1, 1);
    String id = replace(PSTR("current_room_temp#"), ch + 1);
    haDisc.createTempSensor(str, id);
    str = replace(PSTR("{{ value_json.heatercircuit[#].roomtemp ? }}"), ch);
    haDisc.setValueTemplate(str);
    if (!haDisc.publish(en))
        return false;

    str = replace(PSTR("integrator state #"), ch + 1, 1);
    id = replace(PSTR("integ_state_ch#"), ch + 1);
    haDisc.createSensor(str, id);
    str = replace(PSTR("{{ value_json.heatercircuit[#].integState ? }}"), ch);
    haDisc.setValueTemplate(str);
    haDisc.setUnit(FPSTR(HA_UNIT_KELVIN));
    if (!haDisc.publish((heatingLogic[ch].config.roomComp.enabled) && en))
        return false;

    str = replace(PSTR("suspend CH #"), ch + 1, 1);
    id = replace(PSTR("ch_susp#"), ch + 1, 1);
    haDisc.createBinarySensor(str, id, "");
    str = replace(PSTR("{{ None if value_json.heatercircuit[#].suspended is not defined else 'ON' if value_json.heatercircuit[#].suspended else 'OFF' }}"), ch);
    haDisc.setValueTemplate(str);
    if (!haDisc.publish(heatingLogic[ch].config.enableHyst && en))
        return false;

    str = replace(PSTR("min. flow temperature #"), ch + 1, 1);
    tp = topic(Mqtt::TOPIC_CHMINTEMP1, ch);
    haDisc.createNumber(str, Mqtt::getTopicString(tp), mqtt.getCmdTopic(tp));
    str = replace(PSTR("{{ value_json.heatercircuit[#].flowMin ? }}"), ch);
    haDisc.setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    haDisc.setUnit(FPSTR(HA_UNIT_CELSIUS));
    haDisc.setValueTemplate(str);
    haDisc.setMinMax(10, 50, 1);
    if (!haDisc.publish(en))
        return false;

    bool ovr = (otMode == OTMODE_REPEATER) || ( (otMode == OTMODE_MASTER) && enableSlave );
    str = replace(PSTR("override CH #"), ch + 1, 1);
    tp = topic(Mqtt::TOPIC_OVERRIDECH1, ch);
    haDisc.createSwitch(str, tp);
    if (!haDisc.publish(ovr && en))
        return false;

    return true;
}

bool OTControl::sendCapDiscoveries() {
    OTValueSlaveConfigMember *vsc = OTValue::getSlaveConfig();
    if ((vsc == nullptr) || !(vsc->isSet()))
        return true;
        
    haDisc.createClima(F("DHW"), Mqtt::getTopicString(Mqtt::TOPIC_DHWSETTEMP), mqtt.getCmdTopic(Mqtt::TOPIC_DHWSETTEMP));
    haDisc.setMinMaxTemp(5, 65, 1);
    haDisc.setCurrentTemperatureTemplate(F("{{ value_json.slave.dhw_t | default(None) }}"));
    haDisc.setCurrentTemperatureTopic(haDisc.defaultStateTopic);
    haDisc.setInitial(45);
    haDisc.setModeCommandTopic(mqtt.getCmdTopic(Mqtt::TOPIC_DHWMODE));
    haDisc.setTemperatureStateTopic(haDisc.defaultStateTopic);
    haDisc.setTemperatureStateTemplate(F("{{ value_json.thermostat.dhw_set_t | default(None) }}"));
    haDisc.setOptimistic(true);
    haDisc.setIcon(F("mdi:water-heater"));
    haDisc.setRetain(true);
    haDisc.setModes(0x03);
    if (!haDisc.publish(vsc->hasDHW()))
        return false;

    bool ovr = (otMode == OTMODE_REPEATER) || ( (otMode == OTMODE_MASTER) && enableSlave );
    haDisc.createSwitch(F("override DHW"), Mqtt::TOPIC_OVERRIDEDHW);
    if (!haDisc.publish(ovr && vsc->hasDHW()))
        return false;

    return sendChDiscoveries(1, vsc->hasCh2());
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

    for (int i=0; i<NUM_HEATCIRCUITS; i++) {
        JsonObject hpObj = config[F("heating")][i];
        HeatingConfig &hc = heatingLogic[i].config;
        HeatingControl &hctrl = heatingCtrl[i];
        hc.chOn = hpObj[F("chOn")];
        hc.baseTemp = hpObj[F("roomsetpoint")][F("temp")] | 21.0; // default room set point
        hc.tMin = hpObj[F("flowMin")] | hpObj[F("minFlow")] | 20.0;
        hc.tMax = hpObj[F("flowMax")] | 40;
        hc.exponent = hpObj[F("exponent")] | 1.0;
        hc.linearSlope = hpObj[F("gradient")] | 1.0;
        hc.linearOffset = hpObj[F("offset")] | hpObj[F("linearOffset")] | 0.0;
        CurveMode curveMode = (CurveMode) ((int) hpObj[F("curveMode")] | (int) CURVE_LINEAR);
        hc.active = (curveMode == CURVE_FOUR_POINT);
        hc.flow = hpObj[F("flow")] | 35;
        hc.minSuspend = hpObj[F("minSuspend")] | false;
        hc.suspOffset = hpObj[F("suspOffset")] | 0.0;

        if (hc.tMin > hc.tMax)
            hc.tMin = hc.tMax;
        JsonObject roomComp = hpObj[F("roomComp")];
        hc.roomComp.enabled = roomComp[F("enabled")] | false;
        hc.roomComp.p = roomComp[F("p")] | 0.0;
        hc.roomComp.i = roomComp[F("i")] | 0.0;
        hc.roomComp.boost = roomComp[F("boost")] | 3.0;
        hc.hysteresis = hpObj[F("hysteresis")] | 0.1;
        hc.enableHyst = hpObj[F("enableHyst")] | false;
        
        hctrl.flowTemp = hc.flow;
        hctrl.flowMin = hc.tMin;
        hctrl.chOn = hc.chOn;
        hctrl.overrideFlow = hpObj[F("overrideFlow")] | false;
        hctrl.piCtrl.enabled = hc.roomComp.enabled;
        if (hc.roomComp.i == 0)
            hctrl.piCtrl.integState = 0;

        if (!roomSetPoint[i]) {
            roomSetPoint[i].set(hc.baseTemp, Sensor::SOURCE_NA);
            heatingCtrl[i].piCtrl.rspPrev = hc.baseTemp;
        }

        const double outsideDefaults[6] = {20.0, 10.0, 0.0, -10.0, -20.0, -30.0};
        auto calcLinearFlow = [&](const double outTemp) {
            double ft = hc.baseTemp + hc.linearSlope * (hc.baseTemp - outTemp) + hc.linearOffset;
            clip(ft, hc.tMin, hc.tMax);
            return ft;
        };

        HeatingConfig::CurvePoint curvePoints[6];
        bool hasCurve = hpObj[F("curvePoints")].is<JsonArray>();
        JsonArray cp = hpObj[F("curvePoints")].as<JsonArray>();
        for (int p = 0; p < 6; p++) {
            double outTemp = outsideDefaults[p];
            double flowTemp = calcLinearFlow(outTemp);
            if (hasCurve && p < (int) cp.size()) {
                JsonObject obj = cp[p];
                outTemp = obj[F("out")] | outTemp;
                flowTemp = obj[F("flow")] | flowTemp;
            }
            curvePoints[p].out = outTemp;
            curvePoints[p].flow = flowTemp;
        }

        for (int a = 0; a < 5; a++) {
            for (int b = 0; b < 5 - a; b++) {
                if (curvePoints[b].out > curvePoints[b + 1].out) {
                    auto tmp = curvePoints[b];
                    curvePoints[b] = curvePoints[b + 1];
                    curvePoints[b + 1] = tmp;
                }
            }
        }

        for (int p = 1; p < 6; p++) {
            if (curvePoints[p].flow > curvePoints[p - 1].flow)
                curvePoints[p].flow = curvePoints[p - 1].flow;
        }
        hc.points[0] = curvePoints[5];
        hc.points[1] = curvePoints[4];
        hc.points[2] = curvePoints[3];
        hc.points[3] = curvePoints[2];
        hc.points[4] = curvePoints[1];
        hc.points[5] = curvePoints[0];
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
    boilerCtrl.maxModulation = boiler[F("maxModulation")] | 100;
    statusReqOvl = boiler[F("statusReq")] | 0x0000;
    boilerConfig.otc = boiler[F("otc")] | false;
    boilerConfig.summerMode = boiler[F("summerMode")] | false;
    boilerConfig.dhwBlocking = boiler[F("dhwBlocking")] | false;
    masterMemberId = config[F("masterMemberId")] | 22;

    slaveApp = (SlaveApplication) ((int) config[F("slaveApp")] | 0);

    enableSlave = config[F("enableSlave")] | false;
    enableSlave |= (mode == OTMODE_REPEATER) || (mode == OTMODE_LOOPBACKTEST);
    setOTMode(mode);

    setDhwRequest.force();
    setBoilerRequest[0].force();
    setBoilerRequest[1].force();
    setMasterConfigMember.force();
    setVentSetpointRequest.force();
    setMaxModulation.force();
    setProdVersion.force();
    setOTVersion.force();
    setMaxCh.force();

    master.resetCounters();
    slave.resetCounters();
}

void OTControl::setChCtrlMode(const CtrlMode mode, const uint8_t channel) {
    heatingCtrl[channel].mode = mode;
    switch (mode) {
    case CTRLMODE_AUTO:
        heatingCtrl[channel].chOn = heatingLogic[channel].config.chOn;
        break;
    case CTRLMODE_OFF:
        heatingCtrl[channel].chOn = false;
        break;
    case CTRLMODE_ON:
        heatingCtrl[channel].chOn = true;
    default:
        break;
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

void OTControl::setMaxMod(const int mm) {
    boilerCtrl.maxModulation = mm;
    setMaxModulation.force();
}

void OTControl::setRoomComp(const bool en, const uint8_t channel) {
    heatingCtrl[channel].piCtrl.enabled = en;
    setBoilerRequest[channel].force();
}

void OTControl::setChTemp(const double temp, const uint8_t channel) {
    if (temp == 0)
        heatingCtrl[channel].mode = CTRLMODE_AUTO;
    else
        heatingCtrl[channel].flowTemp = temp;
    setBoilerRequest[channel].force();
}

void OTControl::setFlowMin(const double flowMin, const uint8_t channel) {
    heatingLogic[channel].config.tMin = flowMin;
    if (heatingLogic[channel].config.tMin > heatingLogic[channel].config.tMax)
        heatingLogic[channel].config.tMin = heatingLogic[channel].config.tMax;
    heatingCtrl[channel].flowMin = heatingLogic[channel].config.tMin;
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

bool OTControl::slaveRequest(SlaveRequestStruct &srs) {
    SemMaster sem(2000);
    if (!sem)
        return false;

    unsigned long req = OpenTherm::buildRequest(srs.typeReq, srs.idReq, srs.dataReq);
    sendRequest('T', req);
    sem.wait();

    unsigned long resp = master.hal.getLastResponse();
    srs.typeResp = OpenTherm::getMessageType(resp);
    srs.dataResp = resp & 0xFFFF;
    
    return (master.hal.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS);
}