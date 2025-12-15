#include "httpUpdate.h"
#include <Update.h>
#include <ArduinoJSON.h>

HttpUpdate httpupdate;

void HttpUpdate::checkUpdate() {
    if (updating)
        return;

    fwUrl.clear();
    newFw.clear();

    client.setInsecure();
    HTTPClient https;
    https.begin(client, PSTR(RELEASE_REPO));
    https.addHeader("User-Agent", "ESP32");
    https.addHeader("Accept", "application/vnd.github+json");

    int code = https.GET();
    if (code != 200) {
        https.end();
        return;
    }

    String json = https.getString();
    https.end();

    JsonDocument doc;
    if (deserializeJson(doc, json)) return;

    newFw = doc["tag_name"].as<String>().substring(1);
    Serial.println(newFw);
    if (newFw != F(BUILD_VERSION)) {
        fwUrl = doc["assets"][0]["browser_download_url"].as<String>();
        Serial.println(fwUrl);
    }
}

bool HttpUpdate::getNewFw(String &version) {
    if (newFw.isEmpty() || updating)
        return false;

    version = newFw;
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
        return;
    }

    int len = https.getSize();
    if (!Update.begin(len)) {
        https.end();
        return;
    }

    auto stream = https.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (written != len || !Update.end(true)) {
        Update.abort();
        https.end();
        return;
    }

    https.end();
    ESP.restart();
}