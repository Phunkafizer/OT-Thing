#include <WiFi.h>
#include <ESPmDNS.h>
#include "devconfig.h"
#include "mqtt.h"
#include "otcontrol.h"
#include "sensors.h"
#include <HADiscovery.h>

const char CFG_FILENAME[] PROGMEM = "/config.json";

DevConfig devconfig;

DevConfig::DevConfig():
        writeBufFlag(false) {
}

void DevConfig::begin() {
    LittleFS.begin(true);
    update();
}

void DevConfig::update() {
    File f = getFile();
    if (f) {
        JsonDocument doc;
        deserializeJson(doc, f);

        if (doc[F("hostname")].is<String>())
            hostname = doc[F("hostname")].as<String>();

        if (doc[F("haPrefix")].is<String>())
            HADiscovery::setHAPrefix(doc[F("haPrefix")].as<String>());
            
        if (hostname.isEmpty())
            hostname = F(HOSTNAME);

        if (WiFi.isConnected()) {
            WiFi.setHostname(hostname.c_str());
            MDNS.begin(hostname);
        }

        if (doc[F("mqtt")].is<JsonObject>()) {
            MqttConfig mc;
            const JsonObject &jobj = doc["mqtt"].as<JsonObject>();
            mc.host = jobj["host"].as<String>();
            mc.port = jobj["port"].as<uint16_t>();
            mc.tls = jobj["tls"].as<bool>();
            mc.user = jobj["user"].as<String>();
            mc.pass = jobj["pass"].as<String>();
            mc.keepAlive = jobj["keepAlive"] | 15;
            mqtt.setConfig(mc);
        }

        JsonObject cfg = doc.as<JsonObject>();
        otcontrol.setConfig(cfg);

        if (doc[F("outsideTemp")].is<JsonObject>()) {
            JsonObject obj = doc[F("outsideTemp")];
            outsideTemp.setConfig(obj);
        }

        for (int i=0; i<2; i++) {
            JsonObject obj = doc[F("heating")][i][F("roomtemp")];
            roomTemp[i].setConfig(obj);

            JsonObject obj2 = doc[F("heating")][i][F("roomsetpoint")];
            roomSetPoint[i].setConfig(obj2);
        }

        f.close();
    }
}

File DevConfig::getFile() {
    return LittleFS.open(FPSTR(CFG_FILENAME), "r");
}

void DevConfig::write(String &str) {
    writeBuf = str;
    writeBufFlag = true;
}

void DevConfig::remove() {
    LittleFS.remove(FPSTR(CFG_FILENAME));
}

void DevConfig::loop() {
    if (writeBufFlag) {
        writeBufFlag = false;
        File f = LittleFS.open(FPSTR(CFG_FILENAME), "w");
        f.write((uint8_t *) writeBuf.c_str(), writeBuf.length());
        f.close();
        update();
    }
}

String DevConfig::getHostname() const {
    return hostname;
}