#include "devstatus.h"
#include <WiFi.h>
#include <rom/rtc.h>
#include "mqtt.h"
#include "otcontrol.h"
#include "sensors.h"
#include "httpUpdate.h"
#include <NimBLEDevice.h>

DevStatus devstatus;

DevStatus::DevStatus():
        numWifiDiscon(0) {
}

void DevStatus::lock() {
    mutex.lock();
}
void DevStatus::unlock() {
    doc.clear();
    mutex.unlock();
}

JsonDocument &DevStatus::buildDoc() {
    doc.clear();

    doc[F("runtime")] = millis() / 1000UL;
    doc[F("freeHeap")] = ESP.getFreeHeap();
    doc[F("resetInfo")] = rtc_get_reset_reason(0);
    doc[F("fw_version")] = F(BUILD_VERSION);
    doc[F("USB_connected")] = Serial.isConnected();
    doc[F("reset_reason0")] = rtc_get_reset_reason(0);
    doc[F("reset_reason1")] = rtc_get_reset_reason(1);
    doc[F("numWifiDisc")] = numWifiDiscon;

    String newFw;
    if (httpupdate.getNewFw(newFw))
        doc[F("new_fw")] = newFw;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M:%S", &timeinfo);
        doc[F("dateTime")] = buffer;
    }

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
    jmqtt[F("numDisc")] = mqtt.getNumDisc();

    JsonObject jot = doc.as<JsonObject>();
    otcontrol.getJson(jot);

    double outT;
    if (outsideTemp.get(outT))
        doc[F("outsideTemp")] = outT;

    if (!outsideTemp.owResult.isEmpty())
        doc[F("owResult")] = outsideTemp.owResult;

    JsonObject jo = doc[F("1wire")].to<JsonObject>();
    OneWireNode::writeJsonAll(jo);

    JsonObject ble = doc[F("BLE")].to<JsonObject>();
    BLESensor::writeJsonAll(ble);

    return doc;
}

void DevStatus::getJson(String &str) {
    buildDoc();
    serializeJson(doc, str);
}
