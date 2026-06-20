#ifndef _devconfig_h
#define _devconfig_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

extern class DevConfig {
private:
    void update();
    bool writeBufFlag;
    String hostname;
    int timezone;
    bool authConfigured;
    String authSalt;
    String authHash;
public:
    DevConfig();
    void begin();
    File getFile();
    void write(String &str);
    void remove();
    void loop();
    bool isAuthConfigured() const;
    bool verifyUiCredentials(const String &password) const;
    bool setUiCredentials(const String &password);
    bool clearUiCredentials();
    String getHostname() const;
    int getTimezone() const;
} devconfig;

extern const char CFG_FILENAME[] PROGMEM;

#endif