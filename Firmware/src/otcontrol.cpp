#include <WiFi.h>
#include "otcontrol.h"
#include "auxInput.h"
#include "otvalues.h"
#include "command.h"
#include "HADiscLocal.h"
#include "mqtt.h"
#include "hwdef.h"
#include "portal.h"
#include "sensors.h"
#include "devstatus.h"

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
struct OTTestItem loopbackTestData[51] = {
    {Status,                    0x0000}, // no flags set
    {SConfigSMemberIDcode,      nib(1<<0 | 1<<2 | 1<<5, 1)}, // DHW, cooling, CH2 present, Member ID 1
    {SConfigSMemberIDCodeVentilationHeatRecovery, nib(1<<0 | 1<<1 | 1<<2, 1)}, // heat recovery, bypass present, variable speed control present, Member ID 1
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
    {TdhwSetUBTdhwSetLB,        nib(60, 40)}, // 60 °C upper bound, 40 °C lower bound
    {MaxTSetUBMaxTSetLB,        nib(60, 25)}, // 60 °C upper bound, 20 °C lower bound
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
    const bool sent = hal.sendRequestAsync(msg);

    if (sent) {
        if (source)
            command.sendOtEvent(source, msg);
        txCount++;
        lastTx = millis();
        lastTxMsg = msg;
    }
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
        otMode(OTMODE_LOOPBACKTEST),
        slaveApp(SLAVEAPP_HEATCOOL),
        chcontrol{CHcontrol(0), CHcontrol(1)},
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

    setOTMode(OTMODE_LOOPBACKTEST);
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
    if (mode == OTMODE_LOOPBACKTEST)
        enableSlave = true; // in loopback test mode we need to enable stepup for slave to work
    digitalWrite(GPIO_STEPUP_ENABLE, enableSlave && !bypass);

    for (auto *valobj: slaveValues)
        valobj->init((mode == OTMODE_MASTER) || (mode == OTMODE_LOOPBACKTEST));

    for (auto *valobj: masterValues)
        valobj->init(false);

    for (auto *valobj: roomUnitValues)
        valobj->init(false);

    master.hal.setAlwaysReceive(mode == OTMODE_REPEATER);

    SemMaster sem(100);
    delay(200); // give some time for master to switch to new mode
    master.hal.requestLowPower();
}

void OTControl::setBypass(const bool bypass) {
    this->bypass = bypass;
    setOTMode(otMode);
}

void OTControl::setSummerMode(const bool summerMode) {
    boilerCtrl.summerMode = summerMode;
}

void OTControl::setDhwBlocking(const bool dhwBlocking) {
    boilerCtrl.dhwBlocking = dhwBlocking;
}

bool OTControl::getFlame() const {
    return OTValue::status->getFlame();
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
    if (bypass)
        return;

    hwYield();

    for (int ch=0; ch<NUM_HEATCIRCUITS; ch++) {
        if (!OTValue::slaveConfig->hasCh(ch))
            continue;
        bool force = chcontrol[ch].loop();

        if (millis() > nextPiCtrl) {    
            chcontrol[ch].loopRoomComp();
            chcontrol[ch].loopReturnLimit();
            force = true;
        }

        if (force)
            setBoilerRequest[ch].force();
    }
    if (millis() > nextPiCtrl)
        nextPiCtrl = millis() + PI_INTERVAL * 1000;

    if (OTValue::slaveConfig->hasDHW())
        dhwControl.loop();
    
    if (!discFlag)
        discFlag = sendDiscovery();

    flameStats.loop();

    SemMaster sem(10);
    if (!sem)
        return;

    switch (otMode) {
    case OTMODE_LOOPBACKTEST: {
        for (int ch=0; ch<NUM_HEATCIRCUITS; ch++) {
            if (!OTValue::slaveConfig->hasCh(ch))
                continue;

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
            setMasterConfigMember.send((0<<8) | masterMemberId);
            return;
        }

        for (int ch=0; ch<NUM_HEATCIRCUITS; ch++) {
            if (!OTValue::slaveConfig->hasCh(ch))
                continue;

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

        static int iSlaveVal = 0;
        if (slaveValues[iSlaveVal]->process())
            return;

        if (slaveApp == SLAVEAPP_HEATCOOL) {
            for (int ch=0; ch<NUM_HEATCIRCUITS; ch++) {
                if (!OTValue::slaveConfig->hasCh(ch))
                    continue;
                if (setBoilerRequest[ch]) {
                    double flow = chcontrol[ch].getChOn() ? chcontrol[ch].getFlow() : boilerConfig.chOffTemp;
                    setBoilerRequest[ch].sendFloat(flow);
                    return;
                }
            }

            if (OTValue::slaveConfig->hasDHW() && dhwControl.setDhwRequest &&!noDhwSet) {
                double tmp = dhwControl.getTemp();
                dhwControl.setDhwRequest.sendFloat(tmp);
                return;
            }

            if (OTValue::slaveConfig->hasCooling() && setCoolingCtrlSetpoint) {
                setCoolingCtrlSetpoint.sendFloat(boilerCtrl.coolingCtrl);
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
                double maxCh = chcontrol[0].getFlowMax();
                if (OTValue::slaveConfig->hasCh(1) && (chcontrol[1].getFlowMax() > maxCh))
                    maxCh = chcontrol[1].getFlowMax();
                setMaxCh.sendFloat(maxCh);
            }

            if (millis() > lastBoilerStatus + 800) {
                lastBoilerStatus = millis();
                unsigned long req = OpenTherm::buildSetBoilerStatusRequest(
                    chcontrol[0].getChOn(),
                    dhwControl.getOn(),
                    boilerCtrl.coolOn,
                    boilerConfig.otc, 
                    chcontrol[1].getChOn(),
                    boilerCtrl.summerMode,
                    boilerCtrl.dhwBlocking);
                req |= statusReqOvl;
                sendRequest('T', req);
                return;
            }  
        }

        if (slaveApp == SLAVEAPP_VENT) {
            if (ventCtrl.loop())
                return;
        }

        iSlaveVal = (iSlaveVal + 1) % ((sizeof(slaveValues) / sizeof(slaveValues[0])));
        break;

    default:
        break;
    }
}

void OTControl::sendRequest(const char source, const unsigned long msg) {
    master.sendRequest(source, msg);
    if (otMode == OTMODE_MASTER) {
        OTValue *val = OTValue::getMasterValue(OpenTherm::getDataID(msg));
        if (val) {
            const auto mt = OpenTherm::getMessageType(msg);
            val->setValue(mt, msg & 0xFFFF);
        }
        setLedOTRed(true); // when we're OTMASTER use red LED as TX LED
    }
}

void OTControl::OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status) {
    if (status == OpenThermResponseStatus::TIMEOUT) {
        master.timeoutCount++;
        portal.textAll(F("RX master timeout"));
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
        String log = F("RX master invalid: 0x");
        log += String(msg, HEX);
        portal.textAll(log);
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
            // roomunit tried to read/write dhw set temp. Catch it in order to force writing DHW setpoint by roomunit.
            newMsg = OpenTherm::buildResponse(mt, id, tmpToData(dhwControl.getSetpointRU()));
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
    master.onReceive(c, newMsg);

    if (otval) {
        otval->setValue(mt, newMsg & 0xFFFF);
        switch (mt) {
        case OpenThermMessageType::READ_ACK:
            switch (id) {
            case Toutside:
                outsideTemp.set(OpenTherm::getFloat(msg), Sensor::SOURCE_OT);
                break;
            case Tr:
                roomTemp[0].set(OpenTherm::getFloat(msg), Sensor::SOURCE_OT);
                break;
            case TrCH2:
                roomTemp[1].set(OpenTherm::getFloat(msg), Sensor::SOURCE_OT);
                break;
            case Tret:
                returnTemp[0].set(OpenTherm::getFloat(msg), Sensor::SOURCE_OT);
                break;
            default:
                break;
            }
            break;

        default:
            break;
        }
    }

    if (!otval) {
        if (mt == OpenThermMessageType::READ_ACK)
            portal.textAll(F("no slave val!"));
    }
    otval = OTValue::getMasterValue(id);
    if (otval)
        otval->setMsgType(mt);
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
        String log = F("RX slave invalid: 0x");
        log += String(msg, HEX);
        portal.textAll(log);
        return;
    }

    switch (otMode) {
    case OTMODE_MASTER: {
        unsigned long resp = OpenTherm::buildResponse(OpenThermMessageType::UNKNOWN_DATA_ID, id, 0x0000);
        slave.onReceive('S', msg);
        OTValue *otval = OTValue::getSlaveValue(id);

        switch (mt) {
        case OpenThermMessageType::READ_DATA: {
            switch (id) {
            case Toutside: {
                double t;
                if (outsideTemp.get(t))
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmpToData(t));
                break;
            }

            case TdhwSet: {
                double tmp = dhwControl.getSetpointRU();
                resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmpToData(tmp));
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

            case Status: {
                // respond with masterstatus from roomunit and slavestatus from boiler
                uint16_t data = (msg & 0xFF00) | (otval->getValue() & 0x00FF);
                resp = OpenTherm::buildResponse(otval->getLastMsgType(), id, data);
                chcontrol[0].ovrdOn.value = (msg & (1<<OTValueMasterStatus::BIT_CH_ENABLE)) != 0;
                chcontrol[1].ovrdOn.value = (msg & (1<<OTValueMasterStatus::BIT_CH2_ENABLE)) != 0;
                dhwControl.setOnRU((msg & (1<<OTValueMasterStatus::BIT_DHW_ENABLE)) != 0);
                break;
            }

            case Tret: {
                double t;
                if (returnTemp[0].get(t))
                    resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, tmpToData(t));
                break;
            }

            default: {
                if (otval != nullptr) {
                    if (otval->hasReply())
                        resp = OpenTherm::buildResponse(otval->getLastMsgType(), id, otval->getValue());
                }
                else {
                    otval = OTValue::getMasterValue(id);
                    if (otval != nullptr) {
                        if (otval->isSet())
                            resp = OpenTherm::buildResponse(OpenThermMessageType::READ_ACK, id, otval->getValue());
                    }
                }
                break;
            }
            }

            slave.sendResponse(resp, 'P');
            break;
        }
        case OpenThermMessageType::WRITE_DATA: {
            resp = OpenTherm::buildResponse(OpenThermMessageType::WRITE_ACK, id, msg & 0xFFFF);
            slave.sendResponse(resp, 'P');

            switch (id) {
            case TSet: {
                float val = OpenTherm::getFloat(msg);
                if (val < 0) val = 0;
                chcontrol[0].ovrdTemp.value = val;
                if (chcontrol[0].ovrdTemp.active)
                    setBoilerRequest[0].force();
                break;
            }
            case TsetCH2: {
                float val = OpenTherm::getFloat(msg);
                if (val < 0) val = 0;
                chcontrol[1].ovrdTemp.value = val;
                if (chcontrol[1].ovrdTemp.active)
                    setBoilerRequest[1].force();
                break;
            }

            case TrSet:
                roomSetPoint[0].set(OpenTherm::getFloat(msg), Sensor::SOURCE_OT);
                break;

            case TrSetCH2:
                roomSetPoint[1].set(OpenTherm::getFloat(msg), Sensor::SOURCE_OT);
                break;

            case TdhwSet:
                dhwControl.setSetpointRU(OpenTherm::getFloat(msg));
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
            if ( (chcontrol[0].ovrdTemp.active) && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(chcontrol[0].getFlow()));
            break;

        case TsetCH2:
            if ( (chcontrol[1].ovrdTemp.active) && (mt == OpenThermMessageType::WRITE_DATA) )
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(chcontrol[1].getFlow()));
            break;

        case TdhwSet:
            if (mt == OpenThermMessageType::WRITE_DATA) {
                dhwControl.setSetpointRU(OpenTherm::getFloat(msg));
                newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(dhwControl.getTemp()));
            }
            break;

        case Status:
            for (int i=0; i<NUM_HEATCIRCUITS; i++) {
                if (chcontrol[i].ovrdOn.active) {
                    const uint8_t bit = (i == 0) ? OTValueMasterStatus::BIT_CH_ENABLE : OTValueMasterStatus::BIT_CH2_ENABLE;
                    if (chcontrol[i].getChOn())
                        newMsg |= 1<<bit; // CHx enable
                    else
                        newMsg &= ~(1<<bit); // CHx disable
                }
            }
            
            dhwControl.setOnRU((msg & (1<<OTValueMasterStatus::BIT_DHW_ENABLE)) != 0);
            if (dhwControl.getOn())
                newMsg |= 1<<OTValueMasterStatus::BIT_DHW_ENABLE; // DHW enable
            else
                newMsg &= ~(1<<OTValueMasterStatus::BIT_DHW_ENABLE); // DHW disable
            
            newMsg = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, id, newMsg & 0xFFFF);
            break;

        default:
            break;
        }
        slave.onReceive((msg == newMsg) ? 'T' : 'R', newMsg);
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
            masterTestValues[id] = msg & 0xFFFF;
            slave.sendResponse(reply, 'P');
            break;
        }

        case OpenThermMessageType::READ_DATA: {
            uint32_t reply = OpenTherm::buildResponse(OpenThermMessageType::UNKNOWN_DATA_ID, id, msg & 0xFFFF);

            switch (id) {
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

        switch (otMode) {
        case OTMODE_MASTER:
        case OTMODE_LOOPBACKTEST: {
            OTValue *otval = OTValue::getroomUnitValue(id);
            if (otval)
                otval->setValue(OpenThermMessageType::WRITE_DATA, msg & 0xFFFF);
            break;
        }
        case OTMODE_REPEATER: {
            OTValue *otval = OTValue::getMasterValue(id);
            if (otval)
                otval->setValue(OpenThermMessageType::WRITE_DATA, msg & 0xFFFF);   
            break;
        }
        default:
            break;
        }
    }
}

void OTControl::getJson(JsonObject &obj) {
    JsonObject jSlave = obj[FPSTR(STR_STATKEY_SLAVE)].to<JsonObject>();
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
    
    if (OTValue::status->isSet())
        flameStats.writeJson(jSlave);

    JsonObject master = obj[FPSTR(STR_STATKEY_MASTER)].to<JsonObject>();
    for (auto *valobj: masterValues)
        valobj->getJson(master, true);

    if (enableSlave) {
        master[F("txCount")] = slave.txCount;
        master[F("rxCount")] = slave.rxCount;
        master[F("invalidCount")] = slave.invalidCount;

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
        master[F("smartPower")] = sp;

        JsonObject jRu = obj[FPSTR(STR_STATKEY_ROOMUNIT)].to<JsonObject>();
        for (auto *valobj: roomUnitValues)
            valobj->getJson(jRu);
    }

    JsonArray hcarr = obj[F("heatercircuit")].to<JsonArray>();
    for (int i=0; i<NUM_HEATCIRCUITS; i++) {
        if (!OTValue::slaveConfig->hasCh(i))
            continue;
        double d;
        JsonObject hc = hcarr.add<JsonObject>();

        chcontrol[i].getJson(hc);

        if (roomSetPoint[i].get(d))
            hc[FPSTR(STR_STATKEY_ROOMSETPOINT)] = d;
        
        if (roomTemp[i].get(d))
            hc[FPSTR(STR_STATKEY_ROOMTEMP)] = d;
    }

    switch (slaveApp) {
    case SLAVEAPP_HEATCOOL: {
        JsonObject jDhw = obj[FPSTR(STR_STATKEY_DHW)].to<JsonObject>();
        dhwControl.getJson(jDhw);

        JsonObject jCooling = obj[FPSTR(STR_STATKEY_COOLING)].to<JsonObject>();
        jCooling[FPSTR(STR_STATKEY_CTRLMODE)] = boilerCtrl.coolOn;
        jCooling[FPSTR(STR_STATKEY_SETPOINT)] = boilerCtrl.coolingCtrl;
        if (boilerCtrl.coolOn) {
            if (OTValue::status->getCoolingActive())
                jCooling[FPSTR(STR_STATKEY_ACTION)] = FPSTR(HA_ACTION_COOLING);
            else
                jCooling[FPSTR(STR_STATKEY_ACTION)] = FPSTR(HA_ACTION_IDLE);
        }
        else
            jCooling[FPSTR(STR_STATKEY_ACTION)] = FPSTR(HA_ACTION_OFF);
            
        break;
    }
    case SLAVEAPP_VENT: {
        JsonObject jVent = obj[FPSTR(STR_STATKEY_VENT)].to<JsonObject>();
        ventCtrl.getJson(jVent);
        break;
    }
    default:
        break;
    };

    obj[F("bypass")] = bypass;
    obj[F("summerMode")] = boilerCtrl.summerMode;
    obj[F("dhwBlocking")] = boilerCtrl.dhwBlocking;
}

bool OTControl::sendDiscovery() {
    for (auto *valobj: slaveValues)
        valobj->refreshDisc();

    for (auto *valobj: masterValues)
        valobj->refreshDisc();

    for (auto *valobj: roomUnitValues)
        valobj->refreshDisc();

    bool discFlag = true;

    haDisc.createNumber(F("outside temperature"), Mqtt::getTopicString(Mqtt::TOPIC_OUTSIDETEMP), mqtt.getCmdTopic(Mqtt::TOPIC_OUTSIDETEMP));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_ROOT, PSTR("outsideTemp")));
    haDisc.setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    haDisc.setUnit(FPSTR(HA_UNIT_CELSIUS));
    haDisc.setMinMax(-30, 45, 0.1);
    haDisc.setRetain(true);
    discFlag &= haDisc.publish(outsideTemp.isMqttSource());

    discFlag &= chcontrol[0].sendDiscoveries(slaveApp == SLAVEAPP_HEATCOOL);

    discFlag &= ventCtrl.sendDiscoveries(slaveApp == SLAVEAPP_VENT);

    haDisc.createNumber(F("Max. modulation"), Mqtt::getTopicString(Mqtt::TOPIC_MAXMODULATION), mqtt.getCmdTopic(Mqtt::TOPIC_MAXMODULATION));
    haDisc.setMinMax(0, 100, 1);
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_MASTER, PSTR("max_rel_mod")));
    haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("flame ratio"), F("flame_ratio"));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_DUTY));
    haDisc.setDeviceClass(F("power_factor"));
    haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("burner starts /h"), F("flame_freq"));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_FREQ));
    haDisc.setUnit(F("/h"));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("flametime per cycle"), F("flame_on"));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_ONTIME));
    haDisc.setUnit(FPSTR(HA_UNIT_MIN));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("pausetime per cycle"), F("flame_off"));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_OFFTIME));
    haDisc.setUnit(FPSTR(HA_UNIT_MIN));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("current on time"), F("current_on_time"));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_CURRENTONTIME));
    haDisc.setDeviceClass(PSTR(HA_DEVICE_CLASS_DURATION));
    haDisc.setUnit(FPSTR(HA_UNIT_MIN));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSensor(F("last on time"), F("last_on_time"));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_LASTONTIME));
    haDisc.setDeviceClass(PSTR(HA_DEVICE_CLASS_DURATION));
    haDisc.setUnit(FPSTR(HA_UNIT_MIN));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createSwitch(F("bypass"), Mqtt::TOPIC_BYPASS);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_ROOT, PSTR("bypass")));
    discFlag &= haDisc.publish();

    haDisc.createSwitch(F("summer mode"), Mqtt::TOPIC_SUMMERMODE);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_ROOT, STR_STATKEY_SUMMERMODE));
    discFlag &= haDisc.publish(slaveApp == SLAVEAPP_HEATCOOL);

    return discFlag;
}

bool OTControl::sendCapDiscoveries() {
    if (!dhwControl.sendDiscoveries(OTValue::slaveConfig->hasDHW()))
        return false;

    haDisc.createSwitch(F("Cooling"), Mqtt::TOPIC_COOLINGMODE);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_COOLING, STR_STATKEY_CTRLMODE));
    if (!haDisc.publish(OTValue::slaveConfig->hasCooling()))
        return false;

    haDisc.createNumber(F("cooling control signal"), Mqtt::getTopicString(Mqtt::TOPIC_COOLINGCTRL), mqtt.getCmdTopic(Mqtt::TOPIC_COOLINGCTRL));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_COOLING, STR_STATKEY_SETPOINT));
    haDisc.setMinMax(0, 100, 1);
    haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
    haDisc.setIcon(F("mdi:snowflake-thermometer"));
    haDisc.setRetain(true);
    if (!haDisc.publish(OTValue::slaveConfig->hasCooling()))
        return false;

    return chcontrol[1].sendDiscoveries(OTValue::slaveConfig->hasCh(1));
}

void OTControl::setDhwCtrlMode(const HADiscovery::ClimateMode mode) {
    dhwControl.setOn(mode != HADiscovery::MODE_OFF);
}

void OTControl::setCoolingMode(const bool on) {
    boilerCtrl.coolOn = on;
}

void OTControl::setCoolingCtrl(const int ctrl) {
    boilerCtrl.coolingCtrl = (uint8_t) constrain(ctrl, 0, 100);
    setCoolingCtrlSetpoint.force();
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

    OTMode mode = OTMODE_LOOPBACKTEST;
    if (config[F("otMode")].is<JsonInteger>())
        mode = (OTMode) (int) config[F("otMode")];
    bool ens = config[F("enableSlave")] | false;
    ens |= (mode == OTMODE_REPEATER) || (mode == OTMODE_LOOPBACKTEST);
    if (!init || (mode != otMode) || (ens != enableSlave)) {
        enableSlave = ens;
        setOTMode(mode);
        discFlag = false;
    }
    devconfig.overrideEnabled = ((otMode == OTMODE_MASTER) && enableSlave) || (otMode == OTMODE_REPEATER) || (otMode == OTMODE_LOOPBACKTEST);

    for (int i=0; i<NUM_HEATCIRCUITS; i++) {
        JsonObject obj = config[F("heating")][i];
        chcontrol[i].setConfig(obj, init);
    }

    JsonObject ventObj = config[F("vent")];
    ventCtrl.setConfig(ventObj);

    JsonObject boiler = config[F("boiler")];
    dhwControl.setConfig(boiler);
    
    boilerCtrl.maxModulation = boiler[F("maxModulation")] | 100;
    statusReqOvl = boiler[F("statusReq")] | 0x0000;
    boilerConfig.otc = boiler[F("otc")] | false;
    boilerCtrl.summerMode = boiler[F("summerMode")] | false;
    boilerCtrl.dhwBlocking = boiler[F("dhwBlocking")] | false;
    boilerCtrl.coolOn = boiler[F("coolOn")] | false;
    boilerCtrl.coolingCtrl = 0;
    boilerConfig.chOffTemp = boiler[F("chOffTemp")] | 10.0;
    OTValue::setTexhaustAsFloat(boiler[F("texhaustAsFloat")] | false);

    masterMemberId = config[F("masterMemberId")] | 22;

    slaveApp = (SlaveApplication) ((int) config[F("slaveApp")] | 0);

    for (int i=0; i<NUM_HEATCIRCUITS; i++) {
        setBoilerRequest[i].force();
        setRoomTemp[i].force();
        setRoomSetPoint[i].force();
    }
    setMasterConfigMember.force();
    setMaxModulation.force();
    setProdVersion.force();
    setOTVersion.force();
    setMaxCh.force();
    setOutsideTemp.force();
    setCoolingCtrlSetpoint.force();

    master.resetCounters();
    slave.resetCounters();
    master.hal.setRequestDelay(config[F("otDelay")] | 100);

    noDhwSet = config[F("noDhwSet")] | false;

    init = true;
}

void OTControl::setChCtrlMode(const HADiscovery::ClimateMode mode, const uint8_t channel) {
    chcontrol[channel].mode = mode;
    setBoilerRequest[channel].force();
}

void OTControl::setOverrideChOn(const bool ovrd, const uint8_t channel) {
    chcontrol[channel].ovrdOn.active = ovrd;
    setBoilerRequest[channel].force();
}

void OTControl::setOverrideChFlow(const bool ovrd, const uint8_t channel) {
    chcontrol[channel].ovrdTemp.active = ovrd;
    setBoilerRequest[channel].force();
}

void OTControl::setMaxMod(const int mm) {
    boilerCtrl.maxModulation = mm;
    setMaxModulation.force();
}

void OTControl::setRoomMode(const HADiscovery::ClimateMode mode, const uint8_t channel) {
    chcontrol[channel].setRoomComp(mode);
    setBoilerRequest[channel].force();
}

void OTControl::setChTemp(const double temp, const uint8_t channel) {
    if (temp == 0)
        chcontrol[channel].setMode(HADiscovery::MODE_AUTO);
    else
        chcontrol[channel].flowTemp = temp;

    setBoilerRequest[channel].force();
}

void OTControl::setFlowMin(const double flowMin, const uint8_t channel) {
    chcontrol[channel].flowMin = flowMin;
    setBoilerRequest[channel].force();
}

void OTControl::forceFlowCalc(const uint8_t channel) {
    setBoilerRequest[channel].force();
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