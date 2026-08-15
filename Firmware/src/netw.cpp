#include "netw.h"

#include <ESPmDNS.h>

#include "devconfig.h"
#include "devstatus.h"

OtNetwork netw;

static void wifiEvent(WiFiEvent_t event) {
    Serial.print(F("WiFi event: "));
    Serial.println(event);
    
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
        String hn = devconfig.getHostname();
        WiFi.setHostname(hn.c_str());
        MDNS.begin(hn.c_str());
        publishMdnsServices();
        break;
    }

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        devstatus.numWifiDiscon++;
        WiFi.reconnect();
        break;

    case ARDUINO_EVENT_WPS_ER_SUCCESS:
        esp_wifi_wps_disable();
        WiFi.mode(WIFI_AP_STA);
        break;

    case ARDUINO_EVENT_WPS_ER_FAILED:
    case ARDUINO_EVENT_WPS_ER_TIMEOUT:
        esp_wifi_wps_disable();    
        
        if (WiFi.SSID().isEmpty())
            WiFi.mode(WIFI_AP);
        break;

    default:
        break;
    }
}

void OtNetwork::begin() {
    wifiEventId = WiFi.onEvent(wifiEvent);
    WiFi.setSleep(false);
    WiFi.begin();
}

void OtNetwork::end() {
    WiFi.removeEvent(wifiEventId);
}

bool OtNetwork::startWps() {
    wpscfg.wps_type = WPS_TYPE_PBC;
    strcpy(wpscfg.factory_info.manufacturer, PSTR("Seegel Systeme"));
    strcpy(wpscfg.factory_info.model_name, PSTR("OTthing"));
    strcpy(wpscfg.factory_info.device_name, PSTR("OTthing"));

    WiFi.mode(WIFI_AP_STA);

    // Initialize and start WPS
    const esp_err_t err = esp_wifi_wps_enable(&wpscfg);

    if (err != ESP_OK)
        return false;

    return esp_wifi_wps_start(0) == ESP_OK;
}