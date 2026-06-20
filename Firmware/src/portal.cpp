
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include "portal.h"
#include "devstatus.h"
#include "devconfig.h"
#include "html.h"
#include "otcontrol.h"
#include "otvalues.h"

static const char APP_JSON[] PROGMEM = "application/json";
static const char SESSION_COOKIE[] PROGMEM = "OTSESSID";
static const uint32_t SESSION_TTL_MS = 30UL * 60UL * 1000UL;
static const IPAddress apAddress(4, 3, 2, 1);
static const IPAddress apMask(255, 255, 255, 0);
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
    static const char hex[] = "0123456789abcdef";
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

    if (configMode) {
        WiFi.persistent(false);
        WiFi.softAPConfig(apAddress, apAddress, apMask);
        WiFi.softAP(F(AP_SSID), F(AP_PASSWORD));
        
        if (WiFi.SSID().isEmpty())
            WiFi.mode(WIFI_AP);
        else
            WiFi.mode(WIFI_AP_STA);

        WiFi.setAutoReconnect(false);
        WiFi.persistent(true);
    }

    websrv.begin();
    websrv.addHandler(&ws);

    websrv.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
        #ifdef DEBUG
        if (LittleFS.exists(F("/index.html"))) {
            request->send(LittleFS, F("/index.html"), F("text/html"));
            return;
        }
        #endif

        AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=utf-8"), html_gz, html_gz_len);
        response->addHeader(F("Content-Encoding"), F("gzip"));
        response->addHeader(F("Cache-Control"), F("no-cache"));
        response->addHeader(F("Vary"), F("Accept-Encoding"));
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
        AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON));
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
            WiFi.scanNetworks(true);
        else
            if (n >= 0) {
                JsonArray results = jobj[F("results")].to<JsonArray>();
                for (int i=0; i<n; i++) {
                    JsonObject result = results.add<JsonObject>();
                    result[F("ssid")] = WiFi.SSID(i);
                    result[F("rssi")] = WiFi.RSSI(i);
                    result[F("channel")] = WiFi.channel(i);
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

        JsonDocument doc;
        devstatus.buildDoc(doc);
        AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON));
        serializeJson(doc, *response);
        request->send(response);
    });

    websrv.on(PSTR("/otitems"), HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ensureAuthorized(request))
            return;

        JsonDocument doc;
        JsonObject jSlave = doc[FPSTR(STR_STATKEY_SLAVE)].to<JsonObject>();
            for (auto *valobj: slaveValues)
                valobj->getStatus(jSlave);

        JsonObject jMaster = doc[FPSTR(STR_STATKEY_MASTER)].to<JsonObject>();
            for (auto *valobj: masterValues)
                valobj->getStatus(jMaster);

        AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON));
        serializeJson(doc, *response);
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
            if (!mqtt.setValue(key, value)) {
                request->send(503);
                return;
            }
        }
        request->send(200);
    });
}

void Portal::loop() {
    if (reboot) {
        delay(500);
        ESP.restart();
    }

    ws.cleanupClients();
}

void Portal::textAll(String text) {
    ws.textAll(text);
}