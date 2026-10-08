#include <WiFi.h>
#include "otcontrol.h"
#include "auxInput.h"
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

void OTControl::OTInterface::sendResponse(const char source, const unsigned long msg) {
    uint32_t temp = millis();

    while (true) {
        if (hal.sendResponse(msg)) {
            if (source)
                command.sendOtEvent(source, msg);
        
            txCount++;
            lastTx = millis();
            break;
        }
        if (millis() - temp > 300)
            break;

        hal.process();
        yield();
    }
}

bool OTControl::OTInterface::isConnected() const {
    return millis() - lastRx < 2000; // consider connected if received a message within the last 2 seconds
}

void OTControl::OTInterface::writeJson(JsonObject &obj) const {
    obj["connected"] = isConnected();
    obj[FPSTR(STR_STATKEY_TXCOUNT)] = txCount;
    obj[FPSTR(STR_STATKEY_RXCOUNT)] = rxCount;
}

OTControl::OTControl():
        lastBoilerStatus(0),
        otMode(OTMODE_MASTER_SLAVE),
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

    setOTMode(OTMODE_MASTER_SLAVE);
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
    digitalWrite(GPIO_BYPASS_RELAY, !bypass);

    // set +24V stepup up
    bool slaveEn = (mode == OTMODE_MASTER_SLAVE) || (mode == OTMODE_REPEATER);
    digitalWrite(GPIO_STEPUP_ENABLE, slaveEn && !bypass);

    for (auto *valobj: slaveValues)
        valobj->init(isMaster());

    for (auto *valobj: masterValues)
        valobj->init(false);

    for (auto *valobj: roomUnitValues)
        valobj->init(false);

    master.hal.setAlwaysReceive(mode == OTMODE_REPEATER);

    SemMaster sem(100);
    delay(200); // give some time for master to switch to new mode
    master.hal.requestLowPower();
}

bool OTControl::getOverrideEnabled() const {
    return (otMode == OTMODE_MASTER_SLAVE) || (otMode == OTMODE_REPEATER);
}

void OTControl::setBypass(const bool bypass) {
    if (this->bypass != bypass) {
        this->bypass = bypass;
        setOTMode(otMode);
    }    
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

// return weather a slave (e. g. boiler) is connected to OTthing's master interface
bool OTControl::masterConnected() const {
    return master.isConnected();
}

// return weather a master (e. g. roomunit) is connected to OTthing's slave interface
bool OTControl::slaveConnected() const {
    return slave.isConnected();
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

    const bool newFlame = getFlame();
    if (newFlame != FlameStats::currentFlame)
        flameStats.flameChange(newFlame);

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

        if (newFlame != FlameStats::currentFlame)
            chcontrol[ch].flameChange(newFlame);
    }
    FlameStats::currentFlame = newFlame;

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

    if (isMaster()) {
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
                return;
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
    }
}

bool OTControl::isMaster() const {
    return (otMode == OTMODE_MASTER) || (otMode == OTMODE_MASTER_SLAVE);
}

bool OTControl::hasSlave() const {
    return (otMode == OTMODE_REPEATER) || (otMode == OTMODE_MASTER_SLAVE);
}

void OTControl::sendRequest(const char source, const unsigned long msg) {
    master.sendRequest(source, msg);
    if (isMaster()) {
        OTValue *val = OTValue::getMasterValue(OpenTherm::getDataID(msg));
        if (val) {
            const auto mt = OpenTherm::getMessageType(msg);
            val->setValue(mt, msg & 0xFFFF);
        }
        setLedOTRed(true); // when we're OTMASTER use red LED as TX LED
    }
}

void OTControl::sendResponse(const unsigned long msg, const char source) {
    slave.sendResponse(source, msg);
    OTValue *val = OTValue::getroomUnitValue(OpenTherm::getDataID(msg));
    if (val) {
        const auto mt = OpenTherm::getMessageType(msg);
        val->setMsgResult(mt);
    }
}

void OTControl::setOtValue(OTValue *otval, const OpenThermMessageType mt, const uint16_t data) {
    if (otval) {
        otval->setValue(mt, data);
        const OpenThermMessageID id = otval->getId();
        if (otval->isDataMessage(mt) || (id == TrSet)) { // roomunit "RAM 786" sends TrSet as READ command (out of spec!)
            const double d = OpenTherm::getFloat(data);
            switch (id) {
            case Toutside:
                outsideTemp.set(d, Sensor::SOURCE_OT);
                break;
            case Tret:
                returnTemp[0].set(d, Sensor::SOURCE_OT);
                break;
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
        }
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

        sendResponse(newMsg);
    }

    char c;
    if ((status == OpenThermResponseStatus::INVALID) && (otMode != OTMODE_REPEATER))
        c = 'E';
    else
        c = (newMsg == msg) ? 'B' : 'A';
    master.onReceive(c, newMsg);

    if (otval)
        setOtValue(otval, mt, newMsg & 0xFFFF);
    else {
        if (mt == OpenThermMessageType::READ_ACK)
            portal.textAll(F("no slave val!"));
    }

    otval = OTValue::getMasterValue(id);
    if (otval)
        otval->setMsgResult(mt);
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
    case OTMODE_MASTER:
    case OTMODE_MASTER_SLAVE: {
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
                resp = OpenTherm::buildResponse(otval->getLastMsgResult(), id, data);
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
                        resp = OpenTherm::buildResponse(otval->getLastMsgResult(), id, otval->getValue());
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

            sendResponse(resp, 'P');
            break;
        }

        case OpenThermMessageType::WRITE_DATA: {
            resp = OpenTherm::buildResponse(OpenThermMessageType::WRITE_ACK, id, msg & 0xFFFF);
            sendResponse(resp, 'P');

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
        // data to boiler can be modified here
        bool noForward = false;
        if (OTValue::isDataMessage(id, mt)) {
            switch (id) {
            case TSet:
                if (chcontrol[0].ovrdTemp.active)
                    newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(chcontrol[0].getFlow()));
                break;

            case TsetCH2:
                if (chcontrol[1].ovrdTemp.active)
                    newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(chcontrol[1].getFlow()));
                break;

            case TdhwSet:
                dhwControl.setSetpointRU(OpenTherm::getFloat(msg));
                if (noDhwSet) {
                    noForward = true;
                    sendResponse(OpenTherm::buildResponse(OpenThermMessageType::WRITE_ACK, id, msg & 0xFFFF), 'P');
                }
                else
                    newMsg = OpenTherm::buildRequest(mt, id, OpenTherm::temperatureToData(dhwControl.getTemp()));
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
        }

        if (!noForward) {
            slave.onReceive((msg == newMsg) ? 'T' : 'R', newMsg);
            SemMaster sem(500);
            if (sem)
                sendRequest(0, newMsg);
        }
        break;
    }

    default:
        break;
    }

    OTValue *otval = nullptr;
    uint16_t data = msg & 0xFFFF;

    switch (otMode) {
    case OTMODE_MASTER:
    case OTMODE_MASTER_SLAVE: {
        otval = OTValue::getroomUnitValue(id);
        break;
    }
    case OTMODE_REPEATER: {
        otval = OTValue::getroomUnitValue(id); // update room unit value with original message from roomunit
        if (otval)
            otval->setValue(mt, msg & 0xFFFF);

        otval = OTValue::getMasterValue(id);
        data = newMsg & 0xFFFF;
        break;
    }
    default:
        break;
    }

    setOtValue(otval, mt, data);
}

void OTControl::getJson(JsonObject &obj) {
    JsonObject jSlave = obj[FPSTR(STR_STATKEY_SLAVE)].to<JsonObject>();
    for (auto *valobj: slaveValues)
        valobj->getJson(jSlave);
    
    if (OTValue::status->isSet())
        flameStats.writeJson(jSlave);

    JsonObject jMaster = obj[FPSTR(STR_STATKEY_MASTER)].to<JsonObject>();
    master.writeJson(jMaster);
    if (isMaster())
        jMaster[F("timeouts")] = master.timeoutCount;
    for (auto *valobj: masterValues)
        valobj->getJson(jMaster, true);

    if (hasSlave()) {
        JsonObject jRu = obj[FPSTR(STR_STATKEY_ROOMUNIT)].to<JsonObject>();
        slave.writeJson(jRu);
        jRu[F("invalidCount")] = slave.invalidCount;

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
        jRu[F("smartPower")] = sp;

        for (auto *valobj: roomUnitValues)
            valobj->getJson(jRu, true);
    }

    JsonArray hcarr = obj[F("heatercircuit")].to<JsonArray>();
    for (int i=0; i<NUM_HEATCIRCUITS; i++) {
        if (!OTValue::slaveConfig->hasCh(i))
            continue;

        JsonObject hc = hcarr.add<JsonObject>();
        chcontrol[i].getJson(hc);
    }

    if (OTValue::slaveConfig->hasDHW()) {
        JsonObject jDhw = obj[FPSTR(STR_STATKEY_DHW)].to<JsonObject>();
        dhwControl.getJson(jDhw);
        obj[FPSTR(STR_STATKEY_DHWBLOCKING)] = boilerCtrl.dhwBlocking;
    }

    if (OTValue::slaveConfig->hasCooling()) {
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
    }

    if (OTValue::isSet(StatusVentilationHeatRecovery)) {
        JsonObject jVent = obj[FPSTR(STR_STATKEY_VENT)].to<JsonObject>();
        ventCtrl.getJson(jVent);
    }

    obj[FPSTR(STR_STATKEY_BYPASS)] = bypass;
    obj[FPSTR(STR_STATKEY_SUMMERMODE)] = boilerCtrl.summerMode;
}

bool OTControl::sendDiscovery() {
    for (auto *valobj: slaveValues)
        valobj->refreshDisc();

    for (auto *valobj: masterValues)
        valobj->refreshDisc();

    for (auto *valobj: roomUnitValues)
        valobj->refreshDisc();

    bool discFlag = true;
    const bool isHeatCool = (slaveApp == SLAVEAPP_HEATCOOL);

    haDisc.createNumber(F("outside temperature"), Mqtt::TOPIC_OUTSIDETEMP);
    haDisc.setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    haDisc.setUnit(FPSTR(HA_UNIT_CELSIUS));
    haDisc.setMinMax(-30, 45, 0.1);
    discFlag &= haDisc.publish(outsideTemp.isMqttSource(), Mqtt::VALTMPL_ROOT, PSTR("outsideTemp.current"));

    discFlag &= chcontrol[0].sendDiscoveries(isHeatCool);

    discFlag &= ventCtrl.sendDiscoveries(slaveApp == SLAVEAPP_VENT);

    haDisc.createNumber(F("Max. modulation"), Mqtt::TOPIC_MAXMODULATION);
    haDisc.setMinMax(0, 100, 1);
    haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
    discFlag &= haDisc.publish(isHeatCool, Mqtt::VALTMPL_MASTER, getOTname(OpenThermMessageID::MaxRelModLevelSetting));

    haDisc.createSensor(F("flame ratio"), F("flame_ratio"));
    haDisc.setDeviceClass(F("power_factor"));
    haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
    discFlag &= haDisc.publish(isHeatCool, Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_DUTY);

    haDisc.createSensor(F("burner starts /h"), F("flame_freq"));
    haDisc.setUnit(F("/h"));
    discFlag &= haDisc.publish(isHeatCool, Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_FREQ);

    haDisc.createSensor(F("flametime per cycle"), F("flame_on"));
    haDisc.setUnit(FPSTR(HA_UNIT_MIN));
    discFlag &= haDisc.publish(isHeatCool, Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_ONTIME);

    haDisc.createSensor(F("pausetime per cycle"), F("flame_off"));
    haDisc.setValueTemplate(mqtt.getValueTemplate(Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_OFFTIME));
    haDisc.setUnit(FPSTR(HA_UNIT_MIN));
    discFlag &= haDisc.publish(isHeatCool, Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_OFFTIME);

    haDisc.createSensor(F("current on time"), F("current_on_time"));
    haDisc.setDeviceClass(PSTR(HA_DEVICE_CLASS_DURATION));
    haDisc.setUnit(FPSTR(HA_UNIT_MIN));
    discFlag &= haDisc.publish(isHeatCool, Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_CURRENTONTIME);

    haDisc.createSensor(F("last on time"), F("last_on_time"));
    haDisc.setDeviceClass(PSTR(HA_DEVICE_CLASS_DURATION));
    haDisc.setUnit(FPSTR(HA_UNIT_MIN));
    discFlag &= haDisc.publish(isHeatCool, Mqtt::VALTMPL_FLAMESTATS, STR_STATKEY_FLAMESTATS_LASTONTIME);

    haDisc.createSwitch(F("bypass"), Mqtt::TOPIC_BYPASS);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_ROOT, STR_STATKEY_BYPASS));
    discFlag &= haDisc.publish(true);

    haDisc.createSwitch(F("summer mode"), Mqtt::TOPIC_SUMMERMODE);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_ROOT, STR_STATKEY_SUMMERMODE));
    discFlag &= haDisc.publish(isHeatCool);

    haDisc.createBinarySensor(F("master connection"), F("master_connection"), HA_DEVICE_CLASS_CONNECTIVITY);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_MASTER_ROOT, STR_STATKEY_CONNECTED));
    discFlag &= haDisc.publish(true);

    haDisc.createBinarySensor(F("slave connection"), F("slave_connection"), HA_DEVICE_CLASS_CONNECTIVITY);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_ROOMUNIT_ROOT, STR_STATKEY_CONNECTED));
    discFlag &= haDisc.publish(hasSlave());

    return discFlag;
}

bool OTControl::sendCapDiscoveries() {
    if (!dhwControl.sendDiscoveries(OTValue::slaveConfig->hasDHW()))
        return false;

    haDisc.createSwitch(F("Cooling"), Mqtt::TOPIC_COOLINGMODE);
    haDisc.setValueTemplate(mqtt.getValueTemplateBool(Mqtt::VALTMPL_COOLING, STR_STATKEY_CTRLMODE));
    if (!haDisc.publish(OTValue::slaveConfig->hasCooling()))
        return false;

    haDisc.createNumber(F("cooling control signal"), Mqtt::TOPIC_COOLINGCTRL);
    haDisc.setMinMax(0, 100, 1);
    haDisc.setUnit(FPSTR(HA_UNIT_PERCENT));
    haDisc.setIcon(F("mdi:snowflake-thermometer"));
    haDisc.setRetain(true);
    if (!haDisc.publish(OTValue::slaveConfig->hasCooling(), Mqtt::VALTMPL_COOLING, STR_STATKEY_SETPOINT))
        return false;

    return chcontrol[1].sendDiscoveries(OTValue::slaveConfig->hasCh(1));
}

void OTControl::setDhwCtrlMode(const HADiscovery::ClimateMode mode) {
    dhwControl.setOn(mode != HADiscovery::MODE_OFF);
}

void OTControl::setCoolingMode(const bool on) {
    boilerCtrl.coolOn = on;
}

void OTControl::setTurboShift(const double shift, const uint8_t channel) {
    chcontrol[channel].turbo.shift = shift;
}

void OTControl::setTurboDuration(const uint32_t duration, const uint8_t channel) {
    chcontrol[channel].turbo.endTime = time(nullptr) + duration * 60;
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

    OTMode mode = OTMODE_MASTER_SLAVE;
    if (config[F("otMode")].is<JsonInteger>())
        mode = (OTMode) (int) config[F("otMode")];
    bool bp = config[F("bypass")] | false;

    if (!init || (mode != otMode) || (bp != bypass)) {
        bypass = bp;
        setOTMode(mode);
        discFlag = false;
    }
    devconfig.masterOvrdEnabled = (otMode == OTMODE_MASTER_SLAVE);

    for (int i=0; i<NUM_HEATCIRCUITS; i++) {
        JsonObject obj = config[FPSTR(STR_CONFKEY_HEATING)][i];
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

void OTControl::setChTemp(const double temp, const uint8_t channel, const Sensor::Source src) {
    if (temp == 0)
        chcontrol[channel].setMode(HADiscovery::MODE_AUTO);
    else
        chcontrol[channel].setFlowTemp(temp, src);

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