#include "devstatus.h"
#include <WiFi.h>
#include <rom/rtc.h>
#include "mqtt.h"
#include "otcontrol.h"


DevStatus devstatus;

DevStatus::DevStatus():
        doc(4096) {
}

void DevStatus::setOutsideTemp(double t) {
    doc[F("outsideTemp")] = t;
}

String DevStatus::getJson() {
    doc.clear();

    doc[F("millis")] = millis();
    doc[F("freeHeap")] = ESP.getFreeHeap();
    doc[F("resetInfo")] = rtc_get_reset_reason(0);
    doc[F("fw_version")] = FW_VER;

    JsonObject jwifi = doc.createNestedObject(F("wifi"));
    jwifi[F("status")] =  WiFi.status();
    jwifi[F("mode")] = WiFi.getMode();
    jwifi[F("ipsta")] = WiFi.localIP().toString();
    jwifi[F("mac")] = WiFi.macAddress();
    jwifi[F("hostname")] = WiFi.getHostname();
    jwifi[F("sta_ssid")] = WiFi.SSID();
    jwifi[F("rssi")] = WiFi.RSSI();
    
    JsonObject jmqtt = doc.createNestedObject(F("mqtt"));
    jmqtt[F("connected")] = mqtt.connected();

    JsonObject jot = doc.createNestedObject(F("ot"));
    //otcontrol.getJson(jot);

    String str;
    serializeJson(doc, str);
    return str;
}

void DevStatus::getJson(AsyncResponseStream &response) {
    getJson();
    serializeJson(doc, response);
}