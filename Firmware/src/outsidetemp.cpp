#include "outsidetemp.h"

OutsideTemp outsideTemp;

OutsideTemp::OutsideTemp():
        source(0),
        nextMillis(0),
        httpState(HTTP_IDLE) {
}

void OutsideTemp::loop() {
    switch (source) {
    case 0: // open weather
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
                Serial.println("HTTP get!");
                
                String cmd = "GET /data/2.5/weather/?units=metric";
                cmd += "&lat=" + String(lat);
                cmd += "&lon=" + String(lon);
                cmd += "&appid=" + String("xxxxxxxxxxxxxxxxxxxxx\r\n\r\n");
                Serial.print("Open: ");
                Serial.println(cmd);

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
    source = obj[F("source")];
    lat = obj[F("lat")];
    lon = obj[F("lon")];
}