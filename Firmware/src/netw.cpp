#include "netw.h"
#include <ESPmDNS.h>
#include <esp_task_wdt.h>
#include "devconfig.h"
#include "devstatus.h"
#include "StatusLed.h"
#include "main.h"

OtNetwork netw;

static const IPAddress apAddress(4, 3, 2, 1);
static const IPAddress apMask(255, 255, 255, 0);

static void wifiEvent(WiFiEvent_t event) {
    #ifdef DEBUG
    Serial.printf("WiFi Event: %d\n", event);
    #endif
    switch (event) {
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        netw.isScanning = false;
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
        String hn = devconfig.getHostname();
        MDNS.begin(hn);
        publishMdnsServices();
        break;
    }

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        devstatus.numWifiDiscon++;
        MDNS.end();
        if (netw.hasStoredWifiCredentials())
            WiFi.reconnect();
        break;

    case ARDUINO_EVENT_WPS_ER_SUCCESS:
        netw.stopWps();
        break;

    case ARDUINO_EVENT_WPS_ER_FAILED:
    case ARDUINO_EVENT_WPS_ER_TIMEOUT:
        netw.stopWps();
        break;

    default:
        break;
    }
}

static bool improvConnectWifi(const char *ssid, const char *password) {
    WiFi.disconnect();
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);

    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        esp_task_wdt_reset();
        yield();
        attempts++;
    }

    return WiFi.status() == WL_CONNECTED;
}


void OtNetwork::begin(const bool cfgMode) {
    wifiEventId = WiFi.onEvent(wifiEvent);
    WiFi.setSleep(false);
    String hn = devconfig.getHostname();
    WiFi.setHostname(hn.c_str());

    if (cfgMode) {
        WiFi.persistent(false);
        WiFi.mode(hasStoredWifiCredentials() ? WIFI_AP_STA : WIFI_AP);
        WiFi.softAPConfig(apAddress, apAddress, apMask);
        WiFi.softAP(F(AP_SSID), F(AP_PASSWORD));
        
        //dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        //dnsServer.start(53, "*", apAddress);

        WiFi.setAutoReconnect(false);
        WiFi.persistent(true);

        String improvUrl = F("http://");
        improvUrl += HOSTNAME;
        improvUrl += F(".local/");

        improvBle.setDeviceInfo(
            ImprovTypes::ChipFamily::CF_ESP32_C3,
            HOSTNAME,
            BUILD_VERSION,
            HOSTNAME,
            improvUrl.c_str()
        );
        improvBle.setCustomConnectWiFi(improvConnectWifi);
    }
    else
        WiFi.mode(WIFI_STA);

    WiFi.begin();
}

void OtNetwork::loop() {
    //dnsServer.processNextRequest();
}

void OtNetwork::end() {
    WiFi.removeEvent(wifiEventId);
}

bool OtNetwork::startWps() {
    if (wpsActive)
        return false;

    #ifdef DEBUG
        Serial.println(F("Starting WPS..."));
    #endif
    
    statusLed.set(StatusLed::LED_WPS);

    wpscfg.wps_type = WPS_TYPE_PBC;
    strcpy(wpscfg.factory_info.manufacturer, PSTR("Seegel Systeme"));
    strcpy(wpscfg.factory_info.model_name, PSTR("OTthing"));
    strcpy(wpscfg.factory_info.device_name, PSTR("OTthing"));

    WiFi.mode(WIFI_AP_STA);

    // Initialize and start WPS
    const esp_err_t err = esp_wifi_wps_enable(&wpscfg);

    if (err != ESP_OK)
        return false;

    wpsActive = true;
    return esp_wifi_wps_start(0) == ESP_OK;
}

void OtNetwork::stopWps() {
    esp_wifi_wps_disable();

    if (configMode)
        WiFi.mode(hasStoredWifiCredentials() ? WIFI_AP_STA : WIFI_AP);
    else
        WiFi.mode(WIFI_STA);

    statusLed.set(StatusLed::LED_NORMAL);
    wpsActive = false;
}

bool OtNetwork::startScan() {
    if (isScanning)
        return false;

    WiFi.scanDelete();
    isScanning = WiFi.scanNetworks(true, false, false, 150) == WIFI_SCAN_RUNNING;
    return isScanning;
}

// WiFi.SSID() only reflects an active connection on this core, not the NVS-persisted config, so read it directly.
bool OtNetwork::hasStoredWifiCredentials() {
    WiFi.mode(WIFI_STA); // esp_wifi_get_config requires the STA driver to be initialized
    wifi_config_t conf = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK)
        return false;
    return conf.sta.ssid[0] != 0;
}