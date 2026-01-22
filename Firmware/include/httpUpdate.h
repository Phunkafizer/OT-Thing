#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class HttpUpdate {
private:
    WiFiClientSecure client;
    bool checked {false};
    String newFw;
    String fwUrl;
    bool updating {false};
public:
    void checkUpdate();
    void update();
    bool getNewFw(String &version);
    bool isUpdating();
};

extern HttpUpdate httpupdate;