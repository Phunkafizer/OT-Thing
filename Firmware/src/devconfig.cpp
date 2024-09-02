#include "devconfig.h"
#include "mqtt.h"
#include "otcontrol.h"
#include "outsidetemp.h"

const char CFG_FILENAME[] PROGMEM = "config.json";

DevConfig devconfig;

void DevConfig::begin() {
    LittleFS.begin();
    update();
}

void DevConfig::update() {
    File f = getFile();
    if (f) {
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, f);

        if (doc.containsKey(F("mqtt"))) {
            MqttConfig mc;
            const JsonObject &jobj = doc["mqtt"].as<JsonObject>();
            mc.host = jobj["host"].as<String>();
            mc.port = jobj["port"].as<uint16_t>();
            mc.tls = jobj["tls"].as<bool>();
            mqtt.setConfig(mc);
        }

        if (doc.containsKey(F("chControl"))) {
            ChControlConfig chcfg;
            const JsonObject &obj = doc[F("chControl")].as<JsonObject>();
            chcfg.flowMax = obj[F("flowMax")];
            chcfg.exponent = obj[F("exponent")];
            chcfg.gradient = obj[F("gradient")];
            chcfg.offset = obj[F("offset")];

            //otcontrol.setChCtrlConfig(chcfg);
        }

        if (doc.containsKey(F("otSource"))) {
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