#include <WiFi.h>
#include <ESPmDNS.h>
#include "devconfig.h"
#include "mqtt.h"
#include "otcontrol.h"
#include "sensors.h"
#include "auxInput.h"
#include <HADiscovery.h>

const char CFG_FILENAME[] PROGMEM = "/config.json";

const char CFGKEY_HOSTNAME[] PROGMEM = "hostname";
const char CFGKEY_HAPREFIX[] PROGMEM = "haPrefix";
const char CFGKEY_MQTT[] PROGMEM = "mqtt";
const char CFGKEY_OUTSIDETEMP[] PROGMEM = "outsideTemp";
const char CFGKEY_HEATING[] PROGMEM = "heating";
const char *CFGKEY_AUX PROGMEM = "aux";

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

        if (doc[FPSTR(CFGKEY_HOSTNAME)].is<String>())
            hostname = doc[FPSTR(CFGKEY_HOSTNAME)].as<String>();

        if (doc[FPSTR(CFGKEY_HAPREFIX)].is<String>())
            HADiscovery::setHAPrefix(doc[FPSTR(CFGKEY_HAPREFIX)].as<String>());

        timezone = doc[F("timezone")] | 3600;
            
        if (hostname.isEmpty())
            hostname = F(HOSTNAME);

        if (WiFi.isConnected()) {
            WiFi.setHostname(hostname.c_str());
            MDNS.begin(hostname);
        }

        if (doc[FPSTR(CFGKEY_MQTT)].is<JsonObject>()) {
            MqttConfig mc;
            const JsonObject &jobj = doc[FPSTR(CFGKEY_MQTT)].as<JsonObject>();
            mc.host = jobj[F("host")].as<String>();
            mc.port = jobj[F("port")].as<uint16_t>();
            mc.tls = jobj[F("tls")].as<bool>();
            mc.user = jobj[F("user")].as<String>();
            mc.pass = jobj[F("pass")].as<String>();
            mc.keepAlive = jobj[F("keepAlive")] | 15;
            mqtt.setConfig(mc);
        }

        OneWireNode::clear();
        for (int i=0; i<sizeof(auxInput) / sizeof(auxInput[0]); i++) {
            JsonObject obj = doc[FPSTR(CFGKEY_AUX)][i];
            auxInput[i].setConfig(obj);
        }   

        if (doc[FPSTR(CFGKEY_OUTSIDETEMP)].is<JsonObject>()) {
            JsonObject obj = doc[FPSTR(CFGKEY_OUTSIDETEMP)];
            outsideTemp.setConfig(obj);
        }

        for (int i=0; i<2; i++) {
            JsonObject obj = doc[FPSTR(CFGKEY_HEATING)][i][F("roomtemp")];
            roomTemp[i].setConfig(obj);

            obj = doc[FPSTR(CFGKEY_HEATING)][i][F("roomsetpoint")];
            roomSetPoint[i].setConfig(obj);
        }

        JsonObject cfg = doc.as<JsonObject>();
        otcontrol.setConfig(cfg);

        f.close();
    }
}

File DevConfig::getFile() {
    return LittleFS.open(FPSTR(CFG_FILENAME), "r");
}

void DevConfig::write(String &str) {
    File f = LittleFS.open(FPSTR(CFG_FILENAME), "w");
    f.write((uint8_t *) str.c_str(), str.length());
    f.close();
    writeBufFlag = true;
}

void DevConfig::remove() {
    LittleFS.remove(FPSTR(CFG_FILENAME));
}

void DevConfig::loop() {
    if (writeBufFlag) {
        writeBufFlag = false;
        update();
    }
}

String DevConfig::getHostname() const {
    return hostname;
}

int DevConfig::getTimezone() const {
    return timezone;
}