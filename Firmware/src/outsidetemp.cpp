#include "outsidetemp.h"

OutsideTemp outsideTemp;

static void httpcliCB(void *optParm, AsyncHTTPRequest *request, int readyState) {
    Serial.println("CB!");
    if (readyState == readyStateDone) {
        Serial.println("CB ready!");
        Serial.println(request->responseText());
        /*DynamicJsonDocument doc(2048);
        String resp = request->responseText();
        if (!deserializeJson(doc, resp.c_str())) {
            if (doc.is<JsonObject>() && doc.containsKey("main")) {
                JsonObject weatherObj = doc["main"].as<JsonObject>();
                double outsideTemp = round((weatherObj["temp"].as<double>()) * 10) / 10.0;
                //devstatus.setOutsideTemp(outsideTemp);
                Serial.println(outsideTemp);
            }
        }*/
    }
}

OutsideTemp::OutsideTemp():
        source(-1) {
    //httpcli.onReadyStateChange(httpcliCB);
}

void OutsideTemp::loop() {
    static uint32_t lastMillis = 0;

    switch (source) {
    case 0:
        // open weather
        if (millis() - lastMillis > 10000) {
            lastMillis = millis();
            Serial.println("HTTP get!");
            
            String url = "http://api.openweathermap.org/data/2.5/weather/?units=metric";
            url += "&lat=" + String(49.476);
            url += "&lon=" + String(10.989);
            url += "&appid=" + String("5a3314b35f5a86234217381962342d27");
            //httpcli.setDebug(true);
            Serial.print("Open: ");
            //bool x = httpcli.open("GET", url.c_str());
            //Serial.println(x);
            //httpcli.send();
        }
        break;
    }
}


void OutsideTemp::setConfig(JsonObject &obj) {
    source = obj[F("source")];
}