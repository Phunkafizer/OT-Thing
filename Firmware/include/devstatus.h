#ifndef _devstatus_h
#define _devstatus_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "util.h"

extern PGM_P STR_STATKEY_MASTER PROGMEM;
extern PGM_P STR_STATKEY_SLAVE PROGMEM;
extern PGM_P STR_STATKEY_ROOMCOMPINTEGRATOR PROGMEM;
extern PGM_P STR_STATKEY_ROOMTEMP PROGMEM;
extern PGM_P STR_STATKEY_ROOMSETPOINT PROGMEM;
extern PGM_P STR_STATKEY_CTRLMODE PROGMEM;
extern PGM_P STR_STATKEY_FLOWMIN PROGMEM;
extern PGM_P STR_STATKEY_OVERRIDE_TEMP PROGMEM;
extern PGM_P STR_STATKEY_OVERRIDE_ON PROGMEM;
extern PGM_P STR_STATKEY_OVERRIDE PROGMEM;
extern PGM_P STR_STATKEY_ACTION PROGMEM;
extern PGM_P STR_STATKEY_DHW PROGMEM;
extern PGM_P STR_STATKEY_RETURNLIMITINTEGRATOR PROGMEM;
extern PGM_P STR_STATKEY_ROOMACTION PROGMEM;
extern PGM_P STR_STATKEY_ROOMMODE PROGMEM;

extern class DevStatus {
friend class DevStatusLock;
private:
    SemaphoreHandle_t mutex;
public:
    DevStatus();
    bool lock();
    void unlock();
    void buildDoc(JsonDocument &doc);
    uint32_t numWifiDiscon;
} devstatus;

#endif