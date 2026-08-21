#pragma once

#include <WiFi.h>
#include <esp_wps.h>
#include <ImprovWiFiBLE.h>
#include <DNSServer.h>
#include "esp_wifi.h"


class OtNetwork {
private:
    esp_wps_config_t wpscfg;
    wifi_event_id_t wifiEventId;
    bool wpsActive = false;
    DNSServer dnsServer;
    ImprovWiFiBLE improvBle;
public:
    void begin(const bool cfgMode);
    void loop();
    void end();
    bool startWps();
    void stopWps();
    void startScan();
    bool isScanning {false};
};

extern OtNetwork netw;