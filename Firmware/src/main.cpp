#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Ticker.h>
#include "hwdef.h"
#include "portal.h"
#include "otcontrol.h"
#include "mqtt.h"
#include "devstatus.h"
#include "devconfig.h"
#include "command.h"
#include "sensors.h"
#include "HADiscLocal.h"
#include <time.h>
//#include <NimBLEDevice.h>

#ifdef DEBUG
#include <ArduinoOTA.h>
#endif

Ticker statusLedTicker;
volatile uint16_t statusLedData = 0x8000;
bool configMode = false;
const char hostname[] PROGMEM = HOSTNAME;

void statusLedLoop() {
    static uint16_t mask = 0x8000;

    setLedStatus((statusLedData & mask) != 0);
    mask >>= 1;
    if (!mask)
        mask = 0x8000;
}

void wifiEvent(WiFiEvent_t event) {
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
        String hn(FPSTR(hostname));
        WiFi.setHostname(hn.c_str());
        MDNS.addService("http", "tcp", 80);

        const char* tz = "CET-1CEST,M3.5.0,M10.5.0/3";
        configTzTime(tz, "pool.ntp.org");
        break;
    }

    default:
        break;
    }
}
/*
class scanCallbacks : public NimBLEScanCallbacks {
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {
        Serial.println("Discovered Device: %s\n", advertisedDevice->toString().c_str());
    }

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        Serial.println("Device result: %s\n", advertisedDevice->toString().c_str());
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        Serial.println("Scan ended reason = %d; restarting scan\n", reason);
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
} scanCallbacks;
*/
void setup() {
    pinMode(GPIO_STATUS_LED, OUTPUT);
    pinMode(GPIO_CONFIG_BUTTON, INPUT);
    
    setLedStatus(false);

    otcontrol.begin();

    statusLedTicker.attach(0.2, statusLedLoop);

    configMode = digitalRead(GPIO_CONFIG_BUTTON) == 0;
    if (configMode)
        statusLedData = 0xA000;

    Serial.begin();
    Serial.setTxTimeoutMs(100);
    
    WiFi.onEvent(wifiEvent);
    WiFi.begin();
    OneWireNode::begin();
    haDisc.begin();
    mqtt.begin();
    String hn(FPSTR(hostname));
    MDNS.begin(hn.c_str());
    devconfig.begin();
    portal.begin(configMode);
    command.begin();


/*
    NimBLEDevice::init("");                         // Initialize the device, you can specify a device name if you want.
    NimBLEScan* pBLEScan = NimBLEDevice::getScan(); // Create the scan object.
    pBLEScan->setScanCallbacks(&scanCallbacks, false); // Set the callback for when devices are discovered, no duplicates.
    pBLEScan->setActiveScan(true);          // Set active scanning, this will get more data from the advertiser.
    pBLEScan->setMaxResults(0);             // Do not store the scan results, use callback only.
    pBLEScan->start(30000UL, false, true); // duration, not a continuation of last scan, restart to get all devices again.
  */  
#ifdef DEBUG
    ArduinoOTA.begin();
#endif
}

void loop() {
    unsigned long now = millis();

    static unsigned long btnDown = 0;
    if (digitalRead(GPIO_CONFIG_BUTTON) == 0) {
        if ((now - btnDown) > 10000) {
            statusLedTicker.detach();
            setLedStatus(true);
            devconfig.remove();
            WiFi.persistent(true);
            WiFi.disconnect(true, true);
            while (digitalRead(GPIO_CONFIG_BUTTON) == 0)
                yield();
            ESP.restart();
        }
        return;
    }
    else
        btnDown = now;

#ifdef DEBUG
    ArduinoOTA.handle();
#endif
    portal.loop();
    mqtt.loop();
    otcontrol.loop();
    Sensor::loopAll();
    devconfig.loop();
    OneWireNode::loop();
}