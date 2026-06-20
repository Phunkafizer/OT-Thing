#ifndef _portal_h
#define _portal_h

#include <Arduino.h>

class AsyncWebServerRequest;

class Portal {
private:
    String confBuf;
    String sessionToken;
    uint32_t sessionExpiryMs;
    bool configModeActive;
    bool reboot;
    bool updateEnable;
    bool hasValidSession(AsyncWebServerRequest *request);
    bool ensureAuthorized(AsyncWebServerRequest *request);
    String createSessionToken() const;
    void startSession(AsyncWebServerRequest *request);
    void clearSession(AsyncWebServerRequest *request);
public:
    Portal();
    void begin(bool configMode);
    void loop();
    void textAll(String text);
};

extern Portal portal;
#endif