#include "outsidetemp.h"
#include <BLEDevice.h>

OutsideTemp outsideTemp;

OutsideTemp::OutsideTemp():
        source(SOURCE_MQTT),
        nextMillis(0),
        httpState(HTTP_IDLE),
        available(false) {
}

void OutsideTemp::loop() {
    switch (source) {
    case SOURCE_OPENWEATHER: // open weather
        switch (httpState) {
        case HTTP_IDLE:
            if (millis() > nextMillis) {
                cli.connect("api.openweathermap.org", 80);
                httpState = HTTP_CONNECTING;
                nextMillis = millis() + 5000;
            }
            break;
        
        case HTTP_CONNECTING:
            if (cli.connected()) {
                // send HTTP GET
                httpState = HTTP_RECEIVING;
                String cmd = "GET /data/2.5/weather/?units=metric";
                cmd += "&lat=" + String(lat);
                cmd += "&lon=" + String(lon);
                cmd += "&appid=" + apikey + "\r\n\r\n";
                replyBuf.clear();
                cli.write(cmd.c_str());
            }
            else {
                if (millis() > nextMillis) {
                    nextMillis = millis() + interval * 1000UL;
                    cli.stop();
                    httpState = HTTP_IDLE;
                }
            }
            break;

        case HTTP_RECEIVING: {
            if (cli.available()) {
                replyBuf += cli.readString();
            }
            if (!cli.connected()) {
                JsonDocument doc;
                deserializeJson(doc, replyBuf);
                replyBuf.clear();

                if (doc[F("main")][F("temp")].is<JsonFloat>()) {
                    value = doc[F("main")][F("temp")];
                    available = true;
                }
                else
                    available = false;

                nextMillis = millis() + interval * 1000UL;
                httpState = HTTP_IDLE;
            }
            break;
        }

        default:
            break;
        }
    }
}

void OutsideTemp::setConfig(JsonObject &obj) {
    source = (Source) obj[F("source")];
    lat = obj[F("lat")];
    lon = obj[F("lon")];
    apikey = obj[F("apikey")].as<String>();
    interval = obj[F("interval")];
    if (interval == 0)
        interval = 30000;
}

bool OutsideTemp::get(double &value) {
    value = this->value;
    return available;
}

void OutsideTemp::set(const double t, const Source source) {
    if (source == this->source) {
        value = t;
        available = true;
    }
}