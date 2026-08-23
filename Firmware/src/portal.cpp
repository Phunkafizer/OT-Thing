
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <WiFi.h>
#include "portal.h"
#include "devstatus.h"
#include "devconfig.h"
#include "html.h"
#include "otcontrol.h"
#include "otvalues.h"
#include "netw.h"

static const char APP_JSON[] PROGMEM = "application/json";
static const char SESSION_COOKIE[] PROGMEM = "OTSESSID";
static const uint32_t SESSION_TTL_MS = 30UL * 60UL * 1000UL;

Portal portal;
static AsyncWebServer websrv(80);
AsyncWebSocket ws("/ws");


Portal::Portal():
    sessionExpiryMs(0),
    configModeActive(false),
    reboot(false),
    updateEnable(true) {
}

String Portal::createSessionToken() const {
    String hex(F("0123456789abcdef"));
    char buf[33] = {0};
    for (int i=0; i<16; i++) {
        uint8_t b = (uint8_t) (esp_random() & 0xFF);
        buf[i * 2] = hex[(b >> 4) & 0x0F];
        buf[i * 2 + 1] = hex[b & 0x0F];
    }
    return String(buf);
}

static String getCookieValue(AsyncWebServerRequest *request, const char *cookieName) {
    if (!request->hasHeader(F("Cookie")))
        return String();

    String cookieHeader = request->header(F("Cookie"));
    String search = String(cookieName) + "=";
    int start = cookieHeader.indexOf(search);
    if (start < 0)
        return String();

    start += search.length();
    int end = cookieHeader.indexOf(';', start);
    if (end < 0)
        end = cookieHeader.length();

    String value = cookieHeader.substring(start, end);
    value.trim();
    return value;
}

bool Portal::hasValidSession(AsyncWebServerRequest *request) {
    if (sessionToken.isEmpty())
        return false;

    if ((int32_t) (millis() - sessionExpiryMs) >= 0)
        return false;

    String token = getCookieValue(request, SESSION_COOKIE);
    if (token.isEmpty() || token != sessionToken)
        return false;

    sessionExpiryMs = millis() + SESSION_TTL_MS;
    return true;
}

bool Portal::ensureAuthorized(AsyncWebServerRequest *request) {
    if (configModeActive)
        return true;

    if (!devconfig.isAuthConfigured())
        return true;

    if (hasValidSession(request))
        return true;

    request->send(401);
    return false;
}

void Portal::startSession(AsyncWebServerRequest *request) {
    sessionToken = createSessionToken();
    sessionExpiryMs = millis() + SESSION_TTL_MS;
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader(F("Set-Cookie"), String(SESSION_COOKIE) + "=" + sessionToken + F("; Path=/; Max-Age=1800; SameSite=Strict"));
    request->send(response);
}

void Portal::clearSession(AsyncWebServerRequest *request) {
    sessionToken.clear();
    sessionExpiryMs = 0;
    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader(F("Set-Cookie"), String(SESSION_COOKIE) + F("=; Path=/; Max-Age=0; SameSite=Strict"));
    request->send(response);
}

void Portal::begin(bool configMode) {
    configModeActive = configMode;

    websrv.begin();
    websrv.addHandler(&ws);

    websrv.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
        if (netw.isScanning) {
            request->send(503); // service unavailable
            return;
        }

        #ifdef DEBUG
        if (LittleFS.exists(F("/index.html"))) {
            request->send(LittleFS, F("/index.html"), F("text/html"));
            return;
        }
        #endif

        AsyncWebServerResponse *response = request->beginResponse(200, F("text/html; charset=utf-8"), html_gz, html_gz_len);
        response->addHeader(F("Content-Encoding"), F("gzip"));
        response->addHeader(F("Cache-Control"), F("no-cache"));
        response->addHeader(F("Vary"), F("Accept-Encoding"));
        request->send(response);
    });

    websrv.onNotFound([this](AsyncWebServerRequest *request) {
        if (!configModeActive) {
            request->send(404);
            return;
        }

        request->send(404);
        return;

        AsyncWebServerResponse *response = request->beginResponse(302);
        response->addHeader(F("Location"), String(F("http://")) + WiFi.softAPIP().toString() + F("/"));
        request->send(response);
    });

    websrv.on(PSTR("/config"), HTTP_GET, [this] (AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        if (LittleFS.exists(FPSTR(CFG_FILENAME)))
            request->send(LittleFS, FPSTR(CFG_FILENAME), FPSTR(APP_JSON));
        else
            request->send(404);
    });

    websrv.on(PSTR("/config"), HTTP_POST, 
        [this] (AsyncWebServerRequest *request) {
        },
        [] (AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        },
        [this] (AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (!ensureAuthorized(request))
                return;

            static String confBuf;
            if (!index)
                confBuf.clear();

            confBuf.concat((const char*) data, len);

            if (confBuf.length() == total) {
                JsonDocument doc;
                if (deserializeJson(doc, confBuf) != DeserializationError::Ok) {
                    confBuf.clear();
                    request->send(400);
                    return;
                }

                devconfig.write(confBuf);
                confBuf.clear();
                request->send(200);
            }
        }
    );

    websrv.on(PSTR("/auth/state"), HTTP_GET, [this] (AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonObject jobj = doc.to<JsonObject>();
        bool configured = devconfig.isAuthConfigured();
        jobj[F("configured")] = configured;
        jobj[F("loggedIn")] = (configModeActive || !configured) ? true : hasValidSession(request);
        jobj[F("bypass")] = configModeActive;
        AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON), 256);
        serializeJson(doc, *response);
        request->send(response);
    });

    websrv.on(PSTR("/auth/login"), HTTP_POST, [this] (AsyncWebServerRequest *request) {
        if (configModeActive) {
            request->send(200);
            return;
        }

        if (!devconfig.isAuthConfigured()) {
            startSession(request);
            return;
        }

        if (!request->hasArg(F("password"))) {
            request->send(400);
            return;
        }

        String password = request->arg(F("password"));
        if (!devconfig.verifyUiCredentials(password)) {
            request->send(401);
            return;
        }

        startSession(request);
    });

    websrv.on(PSTR("/auth/logout"), HTTP_POST, [this] (AsyncWebServerRequest *request) {
        clearSession(request);
    });

    websrv.on(PSTR("/auth/setup"), HTTP_POST, [this] (AsyncWebServerRequest *request) {
        if (devconfig.isAuthConfigured() && !ensureAuthorized(request))
            return;

        if (!request->hasArg(F("password"))) {
            request->send(400);
            return;
        }

        String password = request->arg(F("password"));

        if (password.isEmpty()) {
            devconfig.clearUiCredentials();
            clearSession(request);
            return;
        }

        if (!devconfig.setUiCredentials(password)) {
            request->send(400);
            return;
        }

        startSession(request);
    });

    websrv.on(PSTR("/scan"), HTTP_GET, [this] (AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        JsonDocument doc;
        JsonObject jobj = doc.to<JsonObject>();

        int n = WiFi.scanComplete();
        jobj[F("status")] = n;
        if (n == -2)
            netw.startScan();
        else
            if (n >= 0) {
                JsonArray results = jobj[F("results")].to<JsonArray>();
                for (int i=0; i<n; i++) {
                    JsonObject result = results.add<JsonObject>();
                    result[F("ssid")] = WiFi.SSID(i);
                    result[F("rssi")] = WiFi.RSSI(i);
                    result[F("channel")] = WiFi.channel(i);
                    result[F("encType")] = WiFi.encryptionType(i);
                    uint8_t bssid[6];
                    WiFi.BSSID(i, bssid);
                    String bssidStr;
                    for (int bi=0; bi<sizeof(bssid); bi++) {
                        if (!bssidStr.isEmpty())
                            bssidStr += ':';
                        if (bssid[bi] < 16)
                            bssidStr += '0';
                        bssidStr += String(bssid[bi], 16);
                    }
                    result[F("bssid")] = bssidStr;
                }
                WiFi.scanDelete();
            }

        AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON));
        serializeJson(doc, *response);
        request->send(response);
    });

    websrv.on(PSTR("/setwifi"), HTTP_POST, [this] (AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        if (request->hasArg(F("ssid")) && request->hasArg(F("pass"))) {
            request->send(200);
            delay(500);

            WiFi.disconnect();
            WiFi.persistent(true);
            WiFi.setAutoReconnect(true);
            String ssid = request->arg(F("ssid"));
            String pass = request->arg(F("pass"));
            WiFi.begin(ssid, pass);
        }
        else
            request->send(400); // bad request
    });

    websrv.on(PSTR("/status"), HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        if (netw.isScanning) {
            request->send(503); // service unavailable
            return;
        }

        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot().to<JsonObject>();

        devstatus.buildDoc(root);
        response->setLength();
        request->send(response);
    });

    websrv.on(PSTR("/otitems"), HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        AsyncJsonResponse *response = new AsyncJsonResponse();
        JsonObject root = response->getRoot().to<JsonObject>();

        JsonObject jSlave = root[FPSTR(STR_STATKEY_SLAVE)].to<JsonObject>();
            for (auto *valobj: slaveValues)
                valobj->getStatus(jSlave);

        JsonObject jMaster = root[FPSTR(STR_STATKEY_MASTER)].to<JsonObject>();
            for (auto *valobj: masterValues)
                valobj->getStatus(jMaster);

        JsonObject jRoomunit = root[FPSTR(STR_STATKEY_ROOMUNIT)].to<JsonObject>();
            for (auto *valobj: roomUnitValues)
                valobj->getStatus(jRoomunit);

        response->setLength();
        request->send(response);
    });

    websrv.on(PSTR("/reboot"), HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        request->send(200);
        this->reboot = true;
    });

    websrv.on(PSTR("/update"), HTTP_POST, 
        [this] (AsyncWebServerRequest *request) { // onRequest handler
            if (!ensureAuthorized(request))
                return;

            int httpRes;

            if (!this->updateEnable) {
                httpRes = 503; // service unavailable
            }
            else {
                this->reboot = !Update.hasError();
                if (this->reboot)
                    httpRes = 200;
                else
                httpRes = 500;
            }
            AsyncWebServerResponse *response = request->beginResponse(httpRes);
            response->addHeader(F("Connection"), F("close"));
            request->send(response);
        },
        [this] (AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) { // onUpdate handler
            if (!this->updateEnable)
                return;

            if (!index)
                Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);

            Update.write(data, len);

            if (final)
                Update.end(true);
        }
    );

    websrv.on(PSTR("/slaverequest"), HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        static const char* STR_ID PROGMEM = "id";
        static const char* STR_RW PROGMEM = "rw";
        static const char* STR_DATA PROGMEM = "data";

        if (!request->hasParam(PSTR(STR_ID))) {
            request->send(503);
            return;
        }
        if (!request->hasParam(PSTR(STR_RW))) {
            request->send(503);
            return;
        }

        SlaveRequestStruct srs;
        srs.idReq = (OpenThermMessageID) request->getParam(PSTR(STR_ID))->value().toInt();
        srs.typeReq = (request->getParam(PSTR(STR_RW))->value().toInt() != 0) ? OpenThermMessageType::READ_DATA : OpenThermMessageType::WRITE_DATA;

        if (!request->hasParam(PSTR(STR_DATA))) {
            request->send(503);
            return;
        }
        String hexData = request->getParam(PSTR(STR_DATA))->value();
        srs.dataReq = strtol(hexData.c_str(), nullptr, 16);

        if (otcontrol.slaveRequest(srs)) {    
            JsonDocument doc;
            JsonObject jobj = doc.to<JsonObject>();
            
            jobj[F("type")] = (int) srs.typeResp;
            jobj[F("id")] = (int) srs.idReq;
            jobj[F("data")] = String(OpenTherm::getUInt(srs.dataResp), 16);
            AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON));
            serializeJson(doc, *response);
            request->send(response);
        }
        else
            request->send(503);
    });

    websrv.on(PSTR("/topics"), HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        String list;
        for (uint8_t topic = Mqtt::TOPIC_OUTSIDETEMP; topic < Mqtt::TOPIC_UNKNOWN; topic++) {
            String line;
            line = mqtt.getCmdTopic((Mqtt::MqttTopic) topic);
            list += line + F("\r\n");
        }
        request->send(200, F("text/plain"), list);
    });

    websrv.on(PSTR("/set"), HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        for (int i=0; i<request->params(); i++) {
            const AsyncWebParameter* par = request->getParam(i);
            String key = par->name();
            String value = par->value();
            if (!mqtt.setValue(key, value, true)) {
                request->send(503);
                return;
            }
        }
        request->send(200);
    });

    websrv.on(PSTR("/testdata"), HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        JsonDocument doc;
        JsonObject jobj = doc.to<JsonObject>();
        for (int i=0; i<sizeof(loopbackTestData)/sizeof(loopbackTestData[0]); i++) {
            PGM_P name = getOTname(loopbackTestData[i].id);
            if (name != nullptr)
                jobj[FPSTR(name)] = OpenTherm::getUInt(loopbackTestData[i].value);
        }

        for (auto item: otcontrol.masterTestValues) {
            PGM_P name = getOTname(item.first);
            if (name != nullptr)
                jobj[FPSTR(name)] = OpenTherm::getUInt(item.second);
        }
        AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON));
        serializeJson(doc, *response);
        request->send(response);
    });

    websrv.on(PSTR("/testdata"), HTTP_POST, 
        [this] (AsyncWebServerRequest *request) {
        },
        [] (AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        },
        [this] (AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (!ensureAuthorized(request))
                return;

            static String buf;
            if (!index)
                buf.clear();

            buf.concat((const char*) data, len);
            Serial.printf("/testdata POST chunk: index=%u len=%u total=%u bufLen=%u final=%s\n",
                (unsigned) index,
                (unsigned) len,
                (unsigned) total,
                (unsigned) buf.length(),
                "n/a");

            if (buf.length() == total) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, buf);
                if (err != DeserializationError::Ok) {
                    buf.clear();
                    request->send(400);
                    return;
                }

                if (!doc.is<JsonObject>()) {
                    buf.clear();
                    request->send(400);
                    return;
                }

                bool updated = false;
                {
                    JsonObjectConst obj = doc.as<JsonObjectConst>();
                    for (JsonPairConst kv: obj) {
                        String name = kv.key().c_str();
                        JsonVariantConst jsonValue = kv.value();

                        if (jsonValue.isNull()) {
                            continue;
                        }

                        auto decodeValue = [&] (JsonVariantConst valueVariant, uint16_t &decodedValue) -> bool {
                            if (valueVariant.isNull())
                                return false;

                            if (valueVariant.is<String>()) {
                                String hexValue = valueVariant.as<String>();
                                decodedValue = (uint16_t) strtoul(hexValue.c_str(), nullptr, 16);
                                return true;
                            }

                            if (valueVariant.is<int>() || valueVariant.is<long>() || valueVariant.is<unsigned int>() || valueVariant.is<unsigned long>()) {
                                decodedValue = valueVariant.as<uint16_t>();
                                return true;
                            }

                            return false;
                        };

                        for (int i=0; i<sizeof(loopbackTestData)/sizeof(loopbackTestData[0]); i++) {
                            PGM_P itemName = getOTname(loopbackTestData[i].id);
                            if (name != FPSTR(itemName))
                                continue;

                            uint16_t value = 0;

                            if (!decodeValue(jsonValue, value))
                                break;

                            loopbackTestData[i].value = value;
                            updated = true;
                            break;
                        }
                    }
                }
                
                buf.clear();
                request->send(updated ? 200 : 400);
            }
        }
    );
}

void Portal::loop() {
    if (reboot) {
        ws.closeAll(0, "reboot");
        websrv.end();
        delay(500);
        ESP.restart();
    }
    ws.cleanupClients();
}

void Portal::textAll(String text) {
    ws.textAll(text);
}