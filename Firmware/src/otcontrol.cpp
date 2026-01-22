#include <WiFi.h>
#include "otcontrol.h"
#include "otvalues.h"
#include "command.h"
#include "HADiscLocal.h"
#include "mqtt.h"
#include "hwdef.h"
#include "portal.h"
#include "sensors.h"
#include "esp_task_wdt.h"


const int PI_INTERVAL = 60; // seconds
const char SLAVE_BRAND[] PROGMEM = "Seegel Systeme";

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
    {OpenThermMessageID::SConfigSMemberIDcode,      0x2101}, // DHW & CH2 present
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
    {OpenThermMessageID::CHPumpOperationHours,      6666},
    {OpenThermMessageID::DHWPumpValveOperationHours,5555},
    {OpenThermMessageID::DHWBurnerOperationHours,   2222},
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
    {OpenThermMessageID::TboilerHeatExchanger,      floatToOT(48.5)},
    {OpenThermMessageID::BoilerFanSpeedSetpointAndActual, nib(20, 21)},
    {OpenThermMessageID::FlameCurrent,              floatToOT(96.8)},
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


void OTControl::FlameRatio::set(const bool flame) {
    if (currentFlame != flame) {
        if (!init) {
            // this is first flame on within 1st minute
            init = true;
            memset(on.buf, 60, sizeof(on.buf));
            on.sum = FLAMERAT_BUFSIZE * 60UL;
        }

        if (flame)
            cycles.current++;
        
        update();
        currentFlame = flame;
    }
}

uint8_t OTControl::FlameRatio::getDuty() const {
    return 100 * on.sum / FLAMERAT_BUFSIZE / 60;
}

double OTControl::FlameRatio::getFreq() const {
    return round(cycles.sum / (FLAMERAT_BUFSIZE / 60.0) * 10) / 10.0;
}

void OTControl::FlameRatio::update() {
    if (currentFlame) {
        uint8_t diff = 0;
        diff = (millis() - lastEdge) / 1000;
        on.current += diff;
    }
    lastEdge = millis();
}

void OTControl::FlameRatio::loop() {
    OTValueStatus *ots = static_cast<OTValueStatus*>(OTValue::getSlaveValue(OpenThermMessageID::Status));
    if (ots)
        set(ots->getFlame());

    if (millis() >= lastInc + 60000) {
        update();

        on.update(idx);
        cycles.update(idx);
        
        idx = (idx + 1) % FLAMERAT_BUFSIZE;
        lastInc = millis();
        init = true;
    }
}

void OTControl::FlameRatio::Ringbuf::update(const uint8_t idx) {
    sum -= buf[idx]; // remove old value
    buf[idx] = current;
    sum += current;
    current = 0;
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
        break;

    default:
        break;
    }

    case CTRLMODE_OFF:
        return 0;
    }

    // room temperature compensation
    flow += heatingCtrl->piCtrl.deltaT;
    clip(flow, 0, hc.flowMax);

    return flow;
}

void OTControl::hwYield() {
    //yield();
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
    hwYield();

    if (millis() > nextPiCtrl) {
        loopPiCtrl();
        nextPiCtrl = millis() + PI_INTERVAL * 1000;
    }

    if (!discFlag)
        discFlag = sendDiscovery();

    flameRatio.loop();

    SemMaster sem(10);
    if (!sem)
        return;
    
    bool hasDHW = false;
    bool hasCh2 = false;
    OTValueSlaveConfigMember *sc = static_cast<OTValueSlaveConfigMember*>(OTValue::getSlaveValue(OpenThermMessageID::SConfigSMemberIDcode));
    if (sc != nullptr) {
        hasDHW = sc->hasDHW();
        hasCh2 = sc->hasCh2();
    }

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
        // no break!!
    }

    case OTMODE_MASTER:
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

            if (millis() > lastBoilerStatus + 800) {
                lastBoilerStatus = millis();
                const bool ch1 = heatingCtrl[0].chOn && 
                                !(heatingConfig[0].enableHyst && heatingCtrl[0].suspended);

                const bool ch2 = heatingCtrl[1].chOn && 
                                !(heatingConfig[1].enableHyst && heatingCtrl[1].suspended);

                unsigned long req = OpenTherm::buildSetBoilerStatusRequest(
                    ch1,
                    boilerCtrl.dhwOn,
                    boilerConfig.coolOn,
                    boilerConfig.otc, 
                    ch2,
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
                unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, OpenThermMessageID::StatusVentilationHeatRecovery, hb << 8);
                sendRequest('T', req);
                return;
            }
        }

        if (setMasterConfigMember) {
            setMasterConfigMember.send((1<<8) | masterMemberId);
            return;
        }
        break;

    default:
        break;
    }
}

void OTControl::loopPiCtrl() {
    OTValueStatus *ots = static_cast<OTValueStatus*>(OTValue::getSlaveValue(OpenThermMessageID::Status));
    for (int i=0; i<2; i++) {
        setBoilerRequest[i].force();

        HeatingControl::PiCtrl &pictrl = heatingCtrl[i].piCtrl;
        
        double rt, rsp; // roomtemp, roomsetpoint

        if (!heatingConfig[i].enableHyst)
            heatingCtrl[i].suspended = false;

        pictrl.deltaT = 0;

        if (!roomTemp[i].get(rt))
            continue;

        if (pictrl.init)
            pictrl.roomTempFilt = 0.1 * rt + 0.9 * pictrl.roomTempFilt;
        else
            pictrl.roomTempFilt = rt;
        pictrl.init = true;

        if (!roomSetPoint[i].get(rsp))
            continue;

        if (heatingConfig[i].enableHyst) {
            if (heatingCtrl[i].suspended) {
                if (rt < rsp - heatingConfig[i].hysteresis)
                    heatingCtrl[i].suspended = false;
            }
            else {
                if (rt > rsp + heatingConfig[i].hysteresis)
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
        double p = heatingConfig[i].roomComp.p * e; // Kp * e
        
        // integral part of PI controller
        const bool setJump = (fabs(rsp - pictrl.rspPrev)) > 0.3;
        pictrl.rspPrev = rsp;
        if ( (ots != nullptr) && ots->getChActive(i) && !heatingCtrl[i].suspended ) {
            if (!setJump) {
                if (e > 0)
                    pictrl.integState += heatingConfig[i].roomComp.i * e * PI_INTERVAL / 3600.0; // Ki * e * ts, ts = 60 s
                else
                    pictrl.integState += heatingConfig[i].roomComp.i * e * 0.3 * PI_INTERVAL / 3600.0; // slower as cooling takes more time
            }
            // else freeze integrator
        }
        else
            pictrl.integState = pictrl.integState * 0.95; // decay

        // anti windup
        clip(pictrl.integState, -5, 5);

        double boost = 0;
        if (e > 1.0)
            boost = e * heatingConfig[i].roomComp.boost; // e * Kb

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
                newMsg = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmpToData(ost));
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

    default:
        break;
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

        default:
            break;
        }
    }

    if (!otval && (mt == OpenThermMessageType::READ_ACK))
        portal.textAll(F("no slave val!"));
}

unsigned long OTControl::buildBrandResponse(const OpenThermMessageID id, String &str, const uint8_t idx) {
    if ((idx) < str.length())
        return OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, (str.length() << 8) | str[idx]);
    
    return OpenTherm::buildResponse(OpenThermMessageType::DATA_INVALID, id, str.length() << 8);
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
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmpToData(t));
                break;
            }

            case OpenThermMessageID::DayTime: {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    const uint16_t tmp = ((((timeinfo.tm_wday + 1) % 7) + 1) << 13) | (timeinfo.tm_hour << 8) | timeinfo.tm_min;
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmp);
                }
                break;
            }

            case OpenThermMessageID::Date: {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    const uint16_t tmp = ((timeinfo.tm_mon + 1) << 8) | timeinfo.tm_mday;
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmp);
                }
                break;
            }

            case OpenThermMessageID::Year: {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    const uint16_t tmp = timeinfo.tm_year + 1900;
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmp);
                }
                break;
            }

            case OpenThermMessageID::Brand: {
                String brand = PSTR(SLAVE_BRAND);
                resp = buildBrandResponse(id, brand, msg >> 8);
                break;
            }

            case OpenThermMessageID::BrandVersion: {
                String brandVersion = PSTR(BUILD_VERSION);
                resp = buildBrandResponse(id, brandVersion, msg >> 8);
                break;
            }

            case OpenThermMessageID::BrandSerialNumber: {
                String mac = WiFi.macAddress();
                resp = buildBrandResponse(id, mac, msg >> 8);
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
            case OpenThermMessageID::Status: {
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

            case OpenThermMessageID::Brand: {
                String brand = PSTR(SLAVE_BRAND);
                reply = buildBrandResponse(id, brand, msg >> 8);
                break;
            }

            case OpenThermMessageID::BrandVersion: {
                String brandVersion = PSTR(BUILD_VERSION);
                reply = buildBrandResponse(id, brandVersion, msg >> 8);
                break;
            }

            case OpenThermMessageID::BrandSerialNumber: {
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
         (id == OpenThermMessageID::Status) || 
         (id == OpenThermMessageID::StatusVentilationHeatRecovery) ||
         (id == OpenThermMessageID::TrSet) // roomunit "RAM 786" sends TrSet as READ command (out of spec!)
       ) {
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
    if (OTValue::getSlaveValue(OpenThermMessageID::Status)->isSet) {
        jSlave[F("flameRatio")] = flameRatio.getDuty();
        jSlave[F("flameFreq")] = flameRatio.getFreq();
    }

    JsonObject thermostat = obj[F("thermostat")].to<JsonObject>();
    for (auto *valobj: thermostatValues)
        valobj->getJson(thermostat);

    
    JsonArray hcarr = obj[F("heatercircuit")].to<JsonArray>();
    for (int i=0; i<2; i++) {
        JsonObject hc = hcarr.add<JsonObject>();
        double d;

        if (roomSetPoint[i].get(d)) {
            hc[F("roomsetpoint")] = d;
            hc[F("roomTempFilt")] = heatingCtrl[i].piCtrl.roomTempFilt;
        }
        
        if (roomTemp[i].get(d))
            hc[F("roomtemp")] = d;

        hc[F("ovrdFlow")] = heatingCtrl[i].overrideFlow;
        hc[F("mode")] = (int) heatingCtrl[i].mode;
        hc[F("integState")] = round(heatingCtrl[i].piCtrl.integState * 100) / 100.0;
        if (heatingConfig[i].enableHyst)
            hc[F("suspended")] = heatingCtrl[i].suspended;
    }

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

    haDisc.createNumber(F("outside temperature"), Mqtt::getTopicString(Mqtt::TOPIC_OUTSIDETEMP), mqtt.getCmdTopic(Mqtt::TOPIC_OUTSIDETEMP));
    haDisc.setValueTemplate(F("{{ value_json.outsideTemp | default(None) }}"));
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

    bool ovr = (otMode == OTMODE_REPEATER) || ( (otMode == OTMODE_MASTER) && slaveEnabled );
    haDisc.createSwitch(F("override CH1"), Mqtt::TOPIC_OVERRIDECH1);
    discFlag &= haDisc.publish(ovr);

    haDisc.createSwitch(F("override CH2"), Mqtt::TOPIC_OVERRIDECH2);
    discFlag &= haDisc.publish(ovr);

    haDisc.createSwitch(F("override DHW"), Mqtt::TOPIC_OVERRIDEDHW);
    discFlag &= haDisc.publish(ovr);

    haDisc.createNumber(F("Max. modulation"), Mqtt::getTopicString(Mqtt::TOPIC_MAXMODULATION), mqtt.getCmdTopic(Mqtt::TOPIC_MAXMODULATION));
    haDisc.setMinMax(0, 100, 1);
    haDisc.setValueTemplate(F("{{ value_json.thermostat.max_rel_mod | default(None) }}"));
    discFlag &= haDisc.publish();

    haDisc.createSensor(F("flame ratio"), F("flame_ratio"));
    haDisc.setValueTemplate(F("{{ value_json.slave.flameRatio | default(None) }}"));
    haDisc.setDeviceClass(F("power_factor"));
    haDisc.setUnit(F("%"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("burner starts /h"), F("flame_freq"));
    haDisc.setValueTemplate(F("{{ value_json.slave.flameFreq | default(None) }}"));
    haDisc.setUnit(F("/h"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

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
    haDisc.setMinMaxTemp(20, heatingConfig[ch].flowMax, 0.5);
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
    haDisc.setValueTemplate(str);
    haDisc.setMinMax(0, 30, 0.1);
    if (!haDisc.publish(roomTemp[ch].isMqttSource() && en))
        return false;

    str = replace(PSTR("integrator state #"), ch + 1, 1);
    String id = replace(PSTR("integ_state_ch#"),ch + 1);
    haDisc.createSensor(str, id);
    str = replace(PSTR("{{ value_json.heatercircuit[#].integState ? }}"), ch);
    haDisc.setValueTemplate(str);
    haDisc.setUnit(F("K"));
    if (!haDisc.publish((heatingConfig[ch].roomComp.enabled) && en))
        return false;

    str = replace(PSTR("suspend CH #"), ch + 1, 1);
    id = replace(PSTR("ch_susp#"), ch + 1, 1);
    haDisc.createBinarySensor(str, id, "");

    str = replace(PSTR("{{ None if value_json.heatercircuit[#].suspended is not defined else 'ON' if value_json.heatercircuit[#].suspended else 'OFF' }}"), ch);
    haDisc.setValueTemplate(str);
    if (!haDisc.publish(heatingConfig[ch].enableHyst && en))
        return false;

    return true;
}

bool OTControl::sendCapDiscoveries() {
    OTValueSlaveConfigMember *vsc = static_cast<OTValueSlaveConfigMember*>(OTValue::getSlaveValue(OpenThermMessageID::SConfigSMemberIDcode));
    if ((vsc == nullptr) || !(vsc->isSet))
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

    for (int i=0; i<sizeof(heatingConfig) / sizeof(heatingConfig[0]); i++) {
        JsonObject hpObj = config[F("heating")][i];
        HeatingConfig &hc = heatingConfig[i];
        hc.chOn = hpObj[F("chOn")];
        hc.roomSet = hpObj[F("roomsetpoint")][F("temp")] | 21.0; // default room set point
        hc.flowMax = hpObj[F("flowMax")] | 40;
        hc.exponent = hpObj[F("exponent")] | 1.0;
        hc.gradient = hpObj[F("gradient")] | 1.0;
        hc.offset = hpObj[F("offset")] | 0;
        hc.flow = hpObj[F("flow")] | 35;
        JsonObject roomComp = hpObj[F("roomComp")];
        hc.roomComp.enabled = roomComp[F("enabled")] | false;
        hc.roomComp.p = roomComp[F("p")] | 0.0;
        hc.roomComp.i = roomComp[F("i")] | 0.0;
        hc.roomComp.boost = roomComp[F("boost")] | 3.0;
        hc.hysteresis = hpObj[F("hysteresis")] | 0.1;
        hc.enableHyst = hpObj[F("enableHyst")] | false;
        
        heatingCtrl[i].flowTemp = hc.flow;
        heatingCtrl[i].chOn = hc.chOn;
        heatingCtrl[i].overrideFlow = hpObj[F("overrideFlow")] | false;
        heatingCtrl[i].piCtrl.enabled = hc.roomComp.enabled;
        if (hc.roomComp.i == 0)
            heatingCtrl[i].piCtrl.integState = 0;

        if (!roomSetPoint[i]) {
            roomSetPoint[i].set(hc.roomSet, Sensor::SOURCE_NA);
            heatingCtrl[i].piCtrl.rspPrev = hc.roomSet;
        }
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

bool OTControl::slaveRequest(SlaveRequestStruct &srs) {
    SemMaster sem(2000);
    if (!sem) {
        return false;
    }

    unsigned long req = OpenTherm::buildRequest(srs.typeReq, srs.idReq, srs.dataReq);
    sendRequest('T', req);
    sem.wait();

    unsigned long resp = master.hal.getLastResponse();
    srs.typeResp = OpenTherm::getMessageType(resp);
    srs.dataResp = resp & 0xFFFF;
    
    return (master.hal.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS);
}


OTWriteRequest::OTWriteRequest(OpenThermMessageID id, uint16_t intervalS):
    interval(intervalS),
        id(id) {
}

void OTWriteRequest::send(const uint16_t data) {
    nextMillis = millis() + interval * 1000;

    unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, id, data);
    otcontrol.sendRequest('T', req);
}

void OTWriteRequest::sendFloat(const double f) {
    int16_t td;
    if (f > 100)
        td = 100 << 8;
    else if (f < -100)
        td = - (int) (100 << 8);
    else
        td = (int) (f * 256);

    send(td);
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

OTWRSetOutsideTemp::OTWRSetOutsideTemp():
        OTWriteRequest(OpenThermMessageID::Toutside, 60) {
}

OTWRSetMaxModulation::OTWRSetMaxModulation():
        OTWriteRequest(OpenThermMessageID::MaxRelModLevelSetting, 60) {
}