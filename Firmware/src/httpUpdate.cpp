#include "httpUpdate.h"
#include <Update.h>
#include <ArduinoJSON.h>
#include "esp_task_wdt.h"
#include "otcontrol.h"

HttpUpdate httpupdate;

void HttpUpdate::checkUpdate() {
    if (updating)
        return;

    fwUrl.clear();
    newFw.clear();

    client.setInsecure();
    HTTPClient https;
    https.begin(client, PSTR(RELEASE_REPO));
    https.addHeader(F("User-Agent"), F("ESP32"));
    https.addHeader(F("Accept"), F("application/vnd.github+json"));

    int code = https.GET();
    if (code != 200) {
        https.end();
        return;
    }

    String json = https.getString();
    https.end();

    JsonDocument doc;
    if (deserializeJson(doc, json)) return;

    newFw = doc["tag_name"].as<String>();
    String currentFw('v');
    currentFw += F(BUILD_VERSION);
    if (newFw != currentFw)
        fwUrl = doc["assets"][0]["browser_download_url"].as<String>();
}

bool HttpUpdate::getNewFw(String &version) {
    if (newFw.isEmpty() || updating)
        return false;

    version = fwUrl.isEmpty() ? "" : newFw;
    return true;
}

void HttpUpdate::update() {
    if (updating)
        return;

    updating = true;
    HTTPClient https;

    if (fwUrl.isEmpty())
        return;

    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    https.begin(client, fwUrl);
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

    otcontrol.bypass();

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