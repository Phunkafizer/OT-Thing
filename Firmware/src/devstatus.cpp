#include "devstatus.h"
#include <WiFi.h>
#include <rom/rtc.h>
#include "mqtt.h"
#include "otcontrol.h"
#include "sensors.h"

DevStatus devstatus;

DevStatus::DevStatus() {
}

String DevStatus::getJson() {
    doc.clear();

    doc[F("runtime")] = millis() / 1000UL;
    doc[F("freeHeap")] = ESP.getFreeHeap();
    doc[F("resetInfo")] = rtc_get_reset_reason(0);
    doc[F("fw_version")] = BUILD_VERSION;

    JsonObject jwifi = doc[F("wifi")].to<JsonObject>();
    jwifi[F("status")] =  WiFi.status();
    jwifi[F("mode")] = WiFi.getMode();
    jwifi[F("ipsta")] = WiFi.localIP().toString();
    jwifi[F("mac")] = WiFi.macAddress();
    jwifi[F("hostname")] = WiFi.getHostname();
    jwifi[F("sta_ssid")] = WiFi.SSID();
    jwifi[F("rssi")] = WiFi.RSSI();
    
    JsonObject jmqtt = doc[F("mqtt")].to<JsonObject>();
    jmqtt[F("connected")] = mqtt.connected();
    jmqtt[F("basetopic")] = mqtt.getBaseTopic();

    JsonObject jot = doc.as<JsonObject>();
    otcontrol.getJson(jot);

    double outT;
    if (outsideTemp.get(outT))
        doc[F("outsideTemp")] = outT;

    JsonArray heatercircuit = doc[F("heatercircuit")].to<JsonArray>();
    for (int i=0; i<2; i++) {
        JsonObject hc = heatercircuit.add<JsonObject>();
        double d;
        if (roomSetPoint[i].get(d))
            hc[F("roomsetpoint")] = d;
        if (roomTemp[i].get(d))
            hc[F("roomtemp")] = d;
    }
    
    String str;
    serializeJson(doc, str);
    return str;
}

void DevStatus::getJson(AsyncResponseStream &response) {
    getJson();
    serializeJson(doc, response);
}