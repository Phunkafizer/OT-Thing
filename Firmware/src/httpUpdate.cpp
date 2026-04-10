#include "httpUpdate.h"
#include <Update.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "esp_task_wdt.h"
#include "otcontrol.h"

HttpUpdate httpupdate;

void HttpUpdate::checkUpdate() {
    if (updating)
        return;

    if (!WiFi.isConnected())
        return;

    newFw.clear();

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.begin(client, F("https://otthing.seegel-systeme.de/version.php"));
    https.addHeader(F("Accept"), F("text/plain"));

    int code = https.GET();
    if (code != 200) {
        https.end();
        return;
    }

    newFw = https.getString();
    https.end();

    String currentFw('v');
    currentFw += F(BUILD_VERSION);
    if (newFw == currentFw)
        newFw.clear();
}

bool HttpUpdate::getNewFw(String &version) {
    if (newFw.isEmpty() || updating)
        return false;

    version = newFw;
    return true;
}

void HttpUpdate::update() {
    if (updating || newFw.isEmpty())
        return;

    updating = true;
    HTTPClient https;

    WiFiClientSecure client;
    client.setInsecure();
    https.begin(client, F("https://otthing.seegel-systeme.de/firmware.php"));
    int code = https.GET();
    if (code != HTTP_CODE_OK) {
        https.end();
        updating = false;
        return;
    }

    int len = https.getSize();
    if (!Update.begin(len)) {
        https.end();
        updating = false;
        return;
    }

    otcontrol.setBypass(true);

    auto stream = https.getStreamPtr();
    uint8_t buf[512];

    while (https.connected() && (len > 0 || len == -1)) {
        size_t available = stream->available();
        if (available) {            
            if (available > sizeof(buf))
                available = sizeof(buf);
            int read = stream->readBytes(buf, available);
            Update.write(buf, read);
            if (len > 0)
                 len -= read;
        }
        yield();
        esp_task_wdt_reset();
    }

    https.end();
    Update.end(true);
    ESP.restart();    
}

bool HttpUpdate::isUpdating() {
    return updating;
}