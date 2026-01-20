#include "sensors.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "HADiscLocal.h"

Sensor roomTemp[2];
AutoSensor roomSetPoint[2];
OutsideTemp outsideTemp;

SemaphoreHandle_t AddressableSensor::mutex;
Sensor* Sensor::lastSensor = nullptr;
BLESensor* BLESensor::last = nullptr;
OneWireNode *OneWireNode::last = nullptr;
static OneWire oneWire(4);

class SensorLock: public SemHelper {
public:
    SensorLock(): SemHelper(AddressableSensor::mutex, 100) {    
    }
};

Sensor::Sensor():
    src(SOURCE_NA),
    setFlag(false) {
    prevSensor = lastSensor;
    lastSensor = this;
}

void Sensor::set(const double val, const Source src) {
    if ((src == this->src) || (src == SOURCE_NA)) {
        this->value = round(val * 10) / 10;
        setFlag = true;
    }
}

bool Sensor::get(double &val) {
    if (src == SOURCE_BLE) {
        SensorLock lock;
        if (!lock)
            return false;

        auto *sensor = BLESensor::find(adr);
        if (sensor != nullptr)
            val = sensor->temp;
        return true;
    }

    if (setFlag)
        val = this->value;
    
    return setFlag;
}

Sensor::operator bool() const {
    return setFlag;
}

void Sensor::setConfig(JsonObject &obj) {
    src = (Source) (obj["source"] | (int) SOURCE_NA);
    setFlag = false;
    own = nullptr;
    if (src == SOURCE_1WIRE) {
        own = OneWireNode::find(String(obj["adr"]));
    }
    else if (src == SOURCE_BLE) {
        for (int i=0; i<6; i++)
            adr[i] = strtol(String(obj[F("adr")]).substring(i * 2, i * 2 + 2).c_str(), 0, 16);
    }
}

bool Sensor::isMqttSource() {
    return (src == SOURCE_MQTT) || (src == SOURCE_AUTO);
}

bool Sensor::isOtSource() {
    return (src == SOURCE_OT);
}

void Sensor::loopAll() {
    Sensor *item = lastSensor;
    while (item) {
        item->loop();
        item = item->prevSensor;
    }
}

AutoSensor::AutoSensor() {
    memset(values, 0, sizeof(values));
}

void AutoSensor::set(const double val, const Source src) {
    if ((this->src == SOURCE_AUTO) && (src != SOURCE_NA)) {
        if (val != values[src]) {
            Sensor::set(val, this->src);
            values[src] = val;
        }
    }
    else
        Sensor::set(val, src);
}

OutsideTemp::OutsideTemp():
        nextMillis(0),
        httpState(HTTP_IDLE) {

    acli.onData([this](void *arg, AsyncClient *client, void *data, size_t len) {
        replyBuf += String((char*) data, len);
    });

    acli.onDisconnect([](void *arg, AsyncClient *client) {
        client->close(true);
    });

    acli.onError([](void *arg, AsyncClient *client, int8_t error) {
        client->close(true);
    });
}

void OutsideTemp::setConfig(JsonObject &obj) {
    Sensor::setConfig(obj);
    lat = obj[F("lat")];
    lon = obj[F("lon")];
    apikey = obj[F("apikey")].as<String>();
    interval = (uint16_t) obj[F("interval")] * 1000;
    if (interval == 0)
        interval = 30000;
    owResult.clear();
    nextMillis = 0;
}

void OutsideTemp::loop() {
    if (src != SOURCE_OPENWEATHER)
        return;

    switch (httpState) {
    case HTTP_IDLE:
        if (millis() > nextMillis) {
            acli.connect("api.openweathermap.org", 80);
            httpState = HTTP_CONNECTING;
            nextMillis = millis() + 5000;
        }
        break;
    
    case HTTP_CONNECTING:
        if (acli.connected()) {
            // send HTTP GET
            httpState = HTTP_RECEIVING;
            String cmd = "GET /data/2.5/weather/?units=metric";
            cmd += "&lat=" + String(lat);
            cmd += "&lon=" + String(lon);
            cmd += "&appid=" + apikey + "\r\n\r\n";
            replyBuf.clear();
            acli.write(cmd.c_str());
            nextMillis = millis() + 5000;
        }
        else {
            if (millis() > nextMillis) {
                nextMillis = millis() + interval;
                acli.close(true);
                httpState = HTTP_IDLE;
            }
        }
        break;

    case HTTP_RECEIVING: {
        if (millis() > nextMillis)
            acli.close();

        if (!acli.connected()) {
            JsonDocument doc;
            owResult.clear();
            
            if (deserializeJson(doc, replyBuf) == DeserializationError::Ok) {
                if (doc[F("main")][F("temp")].is<JsonFloat>()) {
                    value = doc[F("main")][F("temp")];
                    setFlag = true;
                    owResult = F("Ok");
                }
                else
                    setFlag = false;
            }
            if (owResult.isEmpty())
                owResult = replyBuf;
            replyBuf.clear();

            nextMillis = millis() + interval;
            httpState = HTTP_IDLE;
        }
        break;
    }
    default:
        break;
    }
}


AddressableSensor::AddressableSensor(const uint8_t *adr, const uint8_t adrLen, AddressableSensor **prev):
        next(*prev) {

    *prev = this;
    this->adrLen = adrLen;
    memcpy(this->adr, adr, adrLen);
}

void AddressableSensor::begin() {
    mutex = xSemaphoreCreateMutex();
}

String AddressableSensor::getAdr() const {
    String result;
    for (uint8_t i=0; i<adrLen; i++) {
        if (adr[i] < 0x10)
            result += '0';
        result += String(adr[i], 16);
    }
    return result;
}

AddressableSensor* AddressableSensor::find(String adr, AddressableSensor *last) {
    AddressableSensor *node = last;
    while (node) {
        if (node->getAdr() == adr)
            break;
        node = node->next;
    }
    return node;
}

void AddressableSensor::writeJsonAll(JsonObject &status, AddressableSensor *last) {
    SensorLock lock;
    if (!lock)
        return;

    AddressableSensor *node = last;
    while (node) {
        String adrStr = node->getAdr();
        JsonVariant var = status[adrStr].to<JsonVariant>();
        node->writeJson(var);            
        node = node->next;
    }
}

bool AddressableSensor::sendDiscoveryAll(AddressableSensor *last) {
    SensorLock lock;
    if (!lock)
        return false;

    bool result = true;
    
    AddressableSensor *node = last;
    while (node) {
        result &= node->sendDiscovery();  
        node = node->next;
    }
    return result;
}


OneWireNode::OneWireNode(uint8_t *addr):
        AddressableSensor(addr, 8, (AddressableSensor**) &last) {
    temp = DEVICE_DISCONNECTED_C;
}

void OneWireNode::begin() {
    oneWire.reset_search();
    uint8_t addr[8];
    while (oneWire.search(addr))
        new OneWireNode(addr);
    loop();
}

void OneWireNode::loop() {
    static uint32_t next = 0;
    if (millis() > next) {
        OneWireNode *node = last;
        DallasTemperature ds(&oneWire);
        ds.requestTemperatures();
        while (node) {
            node->temp = round(ds.getTempC(node->adr) * 10) / 10;
            if (node->temp != DEVICE_DISCONNECTED_C) {
                for (int i=0; i<sizeof(roomTemp) / sizeof(roomTemp[0]); i++) {
                    if (roomTemp[i].own == node)
                        roomTemp[i].set(node->temp, Sensor::SOURCE_1WIRE);
                }
                if (outsideTemp.own == node)
                    outsideTemp.set(node->temp, Sensor::SOURCE_1WIRE);
            }
            node = static_cast<OneWireNode*>(node->next);
        }
        next = millis() + 5000;
    }
}

void OneWireNode::writeJson(JsonVariant val) {
    if (this->temp != DEVICE_DISCONNECTED_C)
        val.set(this->temp);
    else
        val.set(nullptr);
}

OneWireNode *OneWireNode::find(String adr) {
    return static_cast<OneWireNode*>(AddressableSensor::find(adr, last));
}

bool OneWireNode::sendDiscoveryAll() {
    return AddressableSensor::sendDiscoveryAll(last);
}

bool OneWireNode::sendDiscovery() {
    String id = F("1w_");
    id += getAdr();
    haDisc.createTempSensor("1wire", id);
    String path = F("{{ value_json['1wire']['");
    path += getAdr();
    path += F("'] }}");
    haDisc.setValueTemplate(path);
    return haDisc.publish();
}

void OneWireNode::writeJsonAll(JsonObject &status) {
    AddressableSensor::writeJsonAll(status, last);
}


class scanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        BLESensor::onDiscovery(dev);
    }
} scanCallbacks;

BLESensor::BLESensor(const uint8_t *adr):
        AddressableSensor(adr, 6, (AddressableSensor**) &last) {
}

void BLESensor::begin() {
    BLEDevice::init("");
    BLEDevice::setPower(9);
    NimBLEScan* pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setScanCallbacks(&scanCallbacks, true);
    pBLEScan->setActiveScan(false);
    pBLEScan->setMaxResults(0);
    pBLEScan->setInterval(160);
    pBLEScan->setWindow(80);
    pBLEScan->start(0, false, true);
}

void BLESensor::onDiscovery(const NimBLEAdvertisedDevice* dev) {
    auto srvdata = dev->getServiceData(NimBLEUUID("fcd2"));
    if (srvdata.length() == 0)
        return;

    if (srvdata[0] != 0x40)
        return;

    SensorLock lock;

    if (!lock)
        return;

    BLESensor *sensor = find(dev->getAddress().getVal());
    if (sensor == nullptr) {
        sensor = new BLESensor(dev->getAddress().getVal());
        sensor->sendDiscovery();
    }
    
    sensor->parse(srvdata);
    sensor->rssi = dev->getRSSI();
}

void BLESensor::parse(std::string &data) {
    int i = 1;
    while (i < data.length()) {
        switch (data[i++]) {
        case 0x00:
            // packet ID
            i++;
            break;

        case 0x01:
            this->bat = data[i++];
            break;

        case 0x02: {
            int16_t temp = ((data[i] | (data[i+1] << 8)) + 5) / 10;
            this->temp = temp / 10.0;
            i += 2;
            break;
        }

        case 0x03: {
            this->rh = (data[i] | (data[i+1] << 8)) / 100;
            i += 2;
            break;
        }

        case 0x0C: {
            //uint16_t v = data[i] | (data[i+1] << 8);
            i += 2;
            break;
        }

        default:
            return;
        }
    }
}

BLESensor* BLESensor::find(const uint8_t *adr) {
    BLESensor *node = last;
    while (node) {
        if (memcmp(adr, node->adr, 6) == 0)
            return node;
        node = static_cast<BLESensor*>(node->next);
    }
    return nullptr;
}

void BLESensor::writeJsonAll(JsonObject &status) {
    AddressableSensor::writeJsonAll(status, last);
}

void BLESensor::writeJson(JsonVariant val) {
    JsonObject obj = val.to<JsonObject>();
    obj[F("temp")] = this->temp;
    obj[F("rh")] = this->rh;
    obj[F("bat")] = this->bat;
    obj[F("rssi")] = this->rssi;
}

bool BLESensor::sendDiscoveryAll() {
    return AddressableSensor::sendDiscoveryAll(last);
}

bool BLESensor::sendDiscovery() {
    bool result = true;

    String path1 = F("{{ value_json['BLE']['");
    path1 += getAdr();
    path1 += F("']['?'] }}");

    String id = F("ble_t_");
    id += getAdr();
    haDisc.createTempSensor(F("BLE temp"), id);
    String path = path1;
    path.replace("?", F("temp"));
    haDisc.setValueTemplate(path);
    result &= haDisc.publish();

    id = F("ble_rh_");
    id += getAdr();
    haDisc.createSensor(F("BLE RH"), id);
    haDisc.setDeviceClass(F("humidity"));
    haDisc.setUnit(F("%"));
    path = path1;
    path.replace("?", F("rh"));
    haDisc.setValueTemplate(path);
    result &= haDisc.publish();

    id = F("ble_bat_");
    id += getAdr();
    haDisc.createSensor(F("BLE bat."), id);
    haDisc.setDeviceClass(F("battery"));
    haDisc.setUnit(F("%"));
    path = path1;
    path.replace("?", F("bat"));
    haDisc.setValueTemplate(path);
    result &= haDisc.publish();

    id = F("ble_rssi_");
    id += getAdr();
    haDisc.createSensor(F("BLE RSSI"), id);
    haDisc.setDeviceClass(F("signal_strength"));
    haDisc.setUnit(F("dBm"));
    path = path1;
    path.replace("?", F("rssi"));
    haDisc.setValueTemplate(path);
    result &= haDisc.publish();

    return result;
}