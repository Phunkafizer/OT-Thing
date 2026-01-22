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
#include "time.h"
#include "main.h"
#include "esp_task_wdt.h"

#ifdef DEBUG
    #include <ArduinoOTA.h>
#endif

Ticker statusLedTicker;
volatile uint16_t statusLedData = 0x8000;
bool configMode = false;


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
        String hn = devconfig.getHostname();
        WiFi.setHostname(hn.c_str());
        MDNS.begin(hn.c_str());
        MDNS.addService("http", "tcp", 80);
        break;
    }

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        devstatus.numWifiDiscon++;
        WiFi.reconnect();
        break;

    default:
        break;
    }
}

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
    WiFi.setSleep(false);
    WiFi.begin();
    
    AddressableSensor::begin();
    OneWireNode::begin();
    BLESensor::begin();
    haDisc.begin();
    mqtt.begin();
    devconfig.begin();
    configTime(devconfig.getTimezone(), 3600, PSTR("pool.ntp.org"));

    portal.begin(configMode);
    command.begin();

    esp_task_wdt_add(NULL);

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
    esp_task_wdt_reset();
    portal.loop();
    mqtt.loop();
    otcontrol.loop();
    Sensor::loopAll();
    devconfig.loop();
    OneWireNode::loop();
}