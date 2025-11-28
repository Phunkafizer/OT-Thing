#include "sensors.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "HADiscLocal.h"
//#include <BLEDevice.h>

Sensor roomTemp[2];
AutoSensor roomSetPoint[2];
OutsideTemp outsideTemp;

Sensor *Sensor::lastSensor = nullptr;

static OneWire oneWire(4);
OneWireNode *oneWireNode = nullptr;

Sensor::Sensor():
    src(SOURCE_NA) {
    prevSensor = lastSensor;
    lastSensor = this;
}

void Sensor::set(const double val, const Source src) {
    if (src == this->src) {
        this->value = round(val * 10) / 10;
        setFlag = true;
    }
}

bool Sensor::get(double &val) {
    if (setFlag) {
        val = this->value;
        return true;
    }
    return false;
}

void Sensor::setConfig(JsonObject &obj) {
    src = (Source) (obj["source"] | (int) SOURCE_NA);
    setFlag = false;
    own = nullptr;
    if (src == SOURCE_1WIRE) {
        own = OneWireNode::find(String(obj["adr"]));
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
    if (this->src == SOURCE_AUTO) {
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


OneWireNode::OneWireNode(uint8_t *addr):
        next(oneWireNode),
        temp(DEVICE_DISCONNECTED_C) {
    oneWireNode = this;
    memcpy(this->addr, addr, sizeof(this->addr));
}

void OneWireNode::begin() {
    oneWire.reset_search();
    uint8_t addr[8];
    while (oneWire.search(addr)) {
        new OneWireNode(addr);
    }
}

void OneWireNode::loop() {
    static uint32_t last = millis();
    if (millis() - last > 5000) {
        OneWireNode *node = oneWireNode;
        DallasTemperature ds(&oneWire);
        ds.requestTemperatures();
        while (node) {
            node->temp = round(ds.getTempC(node->addr) * 10) / 10;
            if (node->temp != DEVICE_DISCONNECTED_C) {
                for (int i=0; i<sizeof(roomTemp) / sizeof(roomTemp[0]); i++) {
                    if (roomTemp[i].own == node)
                        roomTemp[i].set(node->temp, Sensor::SOURCE_1WIRE);
                }
                if (outsideTemp.own == node)
                    outsideTemp.set(node->temp, Sensor::SOURCE_1WIRE);
            }
            node = node->next;
        }
        last = millis();
    }
}

void OneWireNode::writeJson(JsonObject &status) {
    OneWireNode *node = oneWireNode;

    while (node) {
        String adrStr = node->getAdr();

        if (node->temp != DEVICE_DISCONNECTED_C)
            status[adrStr] = node->temp;
        else
            status[adrStr] = nullptr;
            
        node = node->next;
    }
}

String OneWireNode::getAdr() const {
    String result;
    for (uint8_t i=0; i<sizeof(addr); i++) {
        if (addr[i] < 0x10)
            result += '0';
        result += String(addr[i], 16);
    }
    return result;
}

OneWireNode *OneWireNode::find(String adr) {
    OneWireNode *node = oneWireNode;
    while (node) {
        if (node->getAdr() == adr)
            return node;
        node = node->next;
    }
    return nullptr;
}

bool OneWireNode::sendDiscovery() {
    bool discFlag = true;

    OneWireNode *node = oneWireNode;
    while (node) {
        haDisc.createTempSensor("1wire", node->getAdr());
        String path = F("{{ value_json['1wire']['");
        path += node->getAdr();
        path += F("'] }}");
        haDisc.setValueTemplate(path);
        discFlag &= haDisc.publish();
        node = node->next;
    }
    return discFlag;
}
