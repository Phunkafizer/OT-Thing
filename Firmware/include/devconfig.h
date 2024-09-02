#ifndef _devconfig_h
#define _devconfig_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

extern class DevConfig {
private:
    void update();
public:
    void begin();
    File getFile();
    void write(String str);
} devconfig;

#endif