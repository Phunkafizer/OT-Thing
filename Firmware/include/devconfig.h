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
public:
    DevConfig();
    void begin();
    File getFile();
    void write(String &str);
    void remove();
    void loop();
    String getHostname() const;
    int getTimezone() const;
} devconfig;

extern const char CFG_FILENAME[];

#endif