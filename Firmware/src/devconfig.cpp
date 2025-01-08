#include "devconfig.h"
#include "mqtt.h"
#include "otcontrol.h"
#include "outsidetemp.h"

const char CFG_FILENAME[] PROGMEM = "/config.json";

DevConfig devconfig;

void DevConfig::begin() {
    LittleFS.begin();
    update();
}

void DevConfig::update() {
    File f = getFile();
    if (f) {
        JsonDocument doc;
        deserializeJson(doc, f);

        if (doc[F("mqtt")].is<JsonObject>()) {
            MqttConfig mc;
            const JsonObject &jobj = doc["mqtt"].as<JsonObject>();
            mc.host = jobj["host"].as<String>();
            mc.port = jobj["port"].as<uint16_t>();
            mc.tls = jobj["tls"].as<bool>();
            mqtt.setConfig(mc);
        }

        if (doc[F("chControl")].is<JsonObject>()) {
            ChControlConfig chcfg;
            const JsonObject &obj = doc[F("chControl")].as<JsonObject>();
            chcfg.flowMax = obj[F("flowMax")];
            chcfg.exponent = obj[F("exponent")];
            chcfg.gradient = obj[F("gradient")];
            chcfg.offset = obj[F("offset")];

            //otcontrol.setChCtrlConfig(chcfg);
        }

        if (doc[F("otSource")].is<JsonObject>()) {
            JsonObject obj = doc[F("otSource")];
            outsideTemp.setConfig(obj);
        }

        f.close();
    }
}

File DevConfig::getFile() {
    return LittleFS.open(FPSTR(CFG_FILENAME), "r");
}

void DevConfig::write(String str) {
    File f = LittleFS.open(FPSTR(CFG_FILENAME), "w");
    f.write((uint8_t *) str.c_str(), str.length());
    f.close();
    update();
}