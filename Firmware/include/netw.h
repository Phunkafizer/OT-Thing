#pragma once

#include <WiFi.h>
#include <esp_wps.h>
#include "esp_wifi.h"

class OtNetwork {
private:
    esp_wps_config_t wpscfg;
    wifi_event_id_t wifiEventId;
public:
    void begin();
    void end();
    bool startWps();
};

extern OtNetwork netw;