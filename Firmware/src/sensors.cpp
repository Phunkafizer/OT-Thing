#include "sensors.h"
//#include <BLEDevice.h>

Sensor roomTemp[2];
Sensor roomSetPoint[2];
OutsideTemp outsideTemp;

Sensor *Sensor::lastSensor = nullptr;

Sensor::Sensor():
    src(SOURCE_NA) {
    prevSensor = lastSensor;
    lastSensor = this;
}

void Sensor::set(const double val, const Source src) {
    if (src == this->src) {
        this->value = val;
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
}

bool Sensor::isMqttSource() {
    return (src == SOURCE_MQTT);
}

void Sensor::loopAll() {
    Sensor *item = lastSensor;
    while (item) {
        item->loop();
        item = item->prevSensor;
    }
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
            Serial.println(replyBuf);
            JsonDocument doc;
            deserializeJson(doc, replyBuf);
            replyBuf.clear();

            if (doc[F("main")][F("temp")].is<JsonFloat>()) {
                value = doc[F("main")][F("temp")];
                setFlag = true;
            }
            else
                setFlag = false;

            nextMillis = millis() + interval;
            httpState = HTTP_IDLE;
        }
        break;
    }
    default:
        break;
    }
}


