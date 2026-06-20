#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include "devconfig.h"
#include "mqtt.h"
#include "otcontrol.h"
#include "sensors.h"
#include "auxInput.h"
#include <HADiscovery.h>

const char CFG_FILENAME[] PROGMEM = "/config.json";
const char AUTH_FILENAME[] PROGMEM = "/auth.json";

const char CFGKEY_HOSTNAME[] PROGMEM = "hostname";
const char CFGKEY_HAPREFIX[] PROGMEM = "haPrefix";
const char CFGKEY_MQTT[] PROGMEM = "mqtt";
const char CFGKEY_OUTSIDETEMP[] PROGMEM = "outsideTemp";
const char CFGKEY_HEATING[] PROGMEM = "heating";
const char *CFGKEY_AUX PROGMEM = "aux";

const char AUTHKEY_SALT[] PROGMEM = "salt";
const char AUTHKEY_HASH[] PROGMEM = "hash";

static String bytesToHex(const uint8_t *data, size_t len) {
    static const char hex[] = "0123456789abcdef";
    String out;
    out.reserve(len * 2);
    for (size_t i=0; i<len; i++) {
        out += hex[(data[i] >> 4) & 0x0F];
        out += hex[data[i] & 0x0F];
    }
    return out;
}

static String sha256Hex(const String &input) {
    uint8_t digest[32] = {0};
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, reinterpret_cast<const uint8_t*>(input.c_str()), input.length());
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    return bytesToHex(digest, sizeof(digest));
}

static String randomHex(uint8_t byteLen) {
    uint8_t buf[24] = {0};
    if (byteLen > sizeof(buf))
        byteLen = sizeof(buf);

    for (uint8_t i=0; i<byteLen; i += 4) {
        uint32_t r = esp_random();
        for (uint8_t j=0; j<4 && (i + j) < byteLen; j++)
            buf[i + j] = (r >> (j * 8)) & 0xFF;
    }

    return bytesToHex(buf, byteLen);
}

DevConfig devconfig;

DevConfig::DevConfig():
    writeBufFlag(false),
    timezone(3600),
    authConfigured(false) {
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
            JsonObject hcfg = doc[FPSTR(CFGKEY_HEATING)][i]; 
            
            JsonObject obj = hcfg[F("roomtemp")];
            roomTemp[i].setConfig(obj);

            obj = hcfg[F("roomsetpoint")];
            roomSetPoint[i].setConfig(obj);

            obj = hcfg[F("returnLimit")];
            returnTemp[i].setConfig(obj);
        }

        JsonObject cfg = doc.as<JsonObject>();
        otcontrol.setConfig(cfg);

        f.close();
    }

    authConfigured = false;
    authSalt.clear();
    authHash.clear();
    File af = LittleFS.open(FPSTR(AUTH_FILENAME), "r");
    if (af) {
        JsonDocument adoc;
        if (deserializeJson(adoc, af) == DeserializationError::Ok) {
            authSalt = adoc[FPSTR(AUTHKEY_SALT)].as<String>();
            authHash = adoc[FPSTR(AUTHKEY_HASH)].as<String>();
            authConfigured = !authSalt.isEmpty() && !authHash.isEmpty();
        }
        af.close();
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

bool DevConfig::isAuthConfigured() const {
    return authConfigured;
}

bool DevConfig::verifyUiCredentials(const String &password) const {
    if (!authConfigured)
        return true;

    String candidateHash = sha256Hex(authSalt + ":" + password);
    return candidateHash == authHash;
}

bool DevConfig::setUiCredentials(const String &password) {
    if (password.isEmpty())
        return false;

    authSalt = randomHex(16);
    authHash = sha256Hex(authSalt + ":" + password);
    authConfigured = true;

    JsonDocument authDoc;
    authDoc[FPSTR(AUTHKEY_SALT)] = authSalt;
    authDoc[FPSTR(AUTHKEY_HASH)] = authHash;

    String output;
    serializeJson(authDoc, output);
    File af = LittleFS.open(FPSTR(AUTH_FILENAME), "w");
    if (!af)
        return false;
    af.write((uint8_t *) output.c_str(), output.length());
    af.close();
    return true;
}

bool DevConfig::clearUiCredentials() {
    authConfigured = false;
    authSalt.clear();
    authHash.clear();
    return LittleFS.remove(FPSTR(AUTH_FILENAME)) || !LittleFS.exists(FPSTR(AUTH_FILENAME));
}