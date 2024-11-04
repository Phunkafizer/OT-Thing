#include "portal.h"
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "devstatus.h"
#include "devconfig.h"

static const char APP_JSON[] PROGMEM = "application/json";
static const IPAddress apAddress(4, 3, 2, 1);
Portal portal;
static AsyncWebServer websrv(80);

Portal::Portal():
    reboot(false),
    updateEnable(true) {
}

void Portal::begin(bool configMode) {
     if (configMode) {
        WiFi.persistent(false);
        WiFi.softAPConfig(apAddress, apAddress, IPAddress(255, 255, 255, 0));
        WiFi.softAP("OTThing", F("12345678"));
        WiFi.mode(WIFI_AP_STA);
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
    }

    websrv.begin();

    websrv.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
        //#ifdef DEBUG
        if (LittleFS.exists(F("/index.html"))) {
            request->send(LittleFS, F("/index.html"), F("text/html"));
            return;
        }
        //#endif
        //request->send_P(200, F("text/html"), html);
    });

    websrv.on("/config", HTTP_GET, [this] (AsyncWebServerRequest *request) {
        File f = devconfig.getFile();
        if (f) {
            request->send(f, FPSTR(APP_JSON), f.size());
            f.close();
        }
        else
            request->send(200);
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
                Serial.println("Write DevConfig");
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
                JsonArray results = jobj.createNestedArray(F("results"));
                for (int i=0; i<n; i++) {
                    JsonObject result = results.createNestedObject();
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
            String ssid = request->arg(F("ssid"));
            String pass = request->arg(F("pass"));
            Serial.println(ssid);
            Serial.println(pass);
            WiFi.disconnect();
            WiFi.begin(ssid, pass);
            //WiFi.setAutoConnect(false); // do not connect automatically on power on
            WiFi.persistent(true);
        }
        request->send(200);
    });

    websrv.on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream(FPSTR(APP_JSON));
        devstatus.getJson(*response);
        request->send(response);
    });

    websrv.on("/reboot", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200);
        this->reboot = true;
    });

    websrv.on("/update", HTTP_POST, 
        [this] (AsyncWebServerRequest *request) { // onRequest handler
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
            if (!this->updateEnable) {
                //request->send(410);
                return;
            }

            if (!index) {
                Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
            }
            Update.write(data, len);
            if (final) {
                Update.end(true);
            }
        }
    );
}

void Portal::loop() {
    if (reboot) {
        delay(500);
        ESP.restart();
    }
}