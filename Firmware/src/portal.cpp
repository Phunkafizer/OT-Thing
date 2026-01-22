
#include <Update.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include "portal.h"
#include "devstatus.h"
#include "devconfig.h"
#include "html.h"
#include "otcontrol.h"
#include "httpUpdate.h"

static const char APP_JSON[] PROGMEM = "application/json";
static const IPAddress apAddress(4, 3, 2, 1);
static const IPAddress apMask(255, 255, 255, 0);
Portal portal;
static AsyncWebServer websrv(80);
AsyncWebSocket ws("/ws");


Portal::Portal():
    reboot(false),
    updateEnable(true) {
}

void Portal::begin(bool configMode) {
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
        request->send(200, F("text/html"), (uint8_t*) html, strlen_P(html));
    });

    websrv.on("/config", HTTP_GET, [this] (AsyncWebServerRequest *request) {
        if (LittleFS.exists(FPSTR(CFG_FILENAME)))
            request->send(LittleFS, FPSTR(CFG_FILENAME), FPSTR(APP_JSON));
        else
            request->send(404);
    });

    websrv.on("/config", HTTP_POST, 
        [this] (AsyncWebServerRequest *request) {
        },
        [] (AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
        },
        [this] (AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            static String confBuf;
            if (!index)
                confBuf.clear();

            confBuf.concat((const char*) data, len);

            if (confBuf.length() == total) {
                devconfig.write(confBuf);
                confBuf.clear();
                request->send(200);
            }
        }
    );

    websrv.on("/scan", HTTP_GET, [this] (AsyncWebServerRequest *request) {
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

    websrv.on("/setwifi", HTTP_POST, [this] (AsyncWebServerRequest *request) {
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

    websrv.on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (httpupdate.isUpdating()) {
            request->send(503);
            return;
        }
        JsonDocument doc;
        devstatus.buildDoc(doc);
        AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON));
        serializeJson(doc, *response);
        request->send(response);
    });

    websrv.on("/reboot", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200);
        this->reboot = true;
    });

    websrv.on("/update", HTTP_POST, 
        [this] (AsyncWebServerRequest *request) { // onRequest handler
            int httpRes;

            if (!this->updateEnable || httpupdate.isUpdating()) {
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
            if (!this->updateEnable || httpupdate.isUpdating())
                return;

            if (!index)
                Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);

            Update.write(data, len);

            if (final)
                Update.end(true);
        }
    );

    websrv.on("/slaverequest", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!request->hasParam("id")) {
            request->send(503);
            return;
        }
        if (!request->hasParam("rw")) {
            request->send(503);
            return;
        }

        SlaveRequestStruct srs;
        srs.idReq = (OpenThermMessageID) request->getParam("id")->value().toInt();
        srs.typeReq = (request->getParam("rw")->value().toInt() != 0) ? OpenThermMessageType::READ_DATA : OpenThermMessageType::WRITE_DATA;

        if (!request->hasParam("data")) {
            request->send(503);
            return;
        }
        String hexData = request->getParam("data")->value();
        srs.dataReq = strtol(hexData.c_str(), nullptr, 16);

        if (otcontrol.slaveRequest(srs)) {    
            JsonDocument doc;
            JsonObject jobj = doc.to<JsonObject>();
            
            jobj["type"] = (int) srs.typeResp;
            jobj["id"] = (int) srs.idReq;
            jobj["data"] = String(OpenTherm::getUInt(srs.dataResp), 16);
            AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON));
            serializeJson(doc, *response);
            request->send(response);
        }
        else
            request->send(503);
    });

    websrv.on("/checkupdate", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->checkUpdate = true;
        request->send(200);
    });

    websrv.on("/install", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->doUpdate = true;
        request->send(200);
    });
}

void Portal::loop() {
    if (reboot) {
        delay(500);
        ESP.restart();
    }

    if (checkUpdate) {
        checkUpdate = false;
        httpupdate.checkUpdate();
    }

    if (doUpdate) {
        doUpdate = false;
        httpupdate.update();
    }

    ws.cleanupClients();
}

void Portal::textAll(String text) {
    ws.textAll(text);
}