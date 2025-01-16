#include "outsidetemp.h"

#include <BLEDevice.h>

OutsideTemp outsideTemp;

OutsideTemp::OutsideTemp():
        source(OUTSIDETEMP_MQTT),
        nextMillis(0),
        httpState(HTTP_IDLE) {
}

void OutsideTemp::loop() {
    switch (source) {
    case OUTSIDETEMP_OPENWEATHER: // open weather
        switch (httpState) {
        case HTTP_IDLE:
            if (millis() > nextMillis) {
                Serial.println("Connecting open weather");
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
                    nextMillis = millis() + 10000;
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

                doc[F("main")][F("temp")].is<JsonFloat>();
                temp = doc[F("main")][F("temp")];

                Serial.println(temp);
                nextMillis = millis() + 10000;
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
    source = (OutsideTempSource) obj[F("source")];
    lat = obj[F("lat")];
    lon = obj[F("lon")];
    apikey = obj[F("apikey")].as<String>();
}