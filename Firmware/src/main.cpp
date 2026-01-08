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

#ifdef DEBUG
    #include <ArduinoOTA.h>
#endif

Ticker statusLedTicker;
volatile uint16_t statusLedData = 0x8000;
bool configMode = false;

#ifdef DEBUG
    NimBLECharacteristic *bleSerialTx;
    volatile bool bleClientConnected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        bleClientConnected = true;
    }

    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
        bleClientConnected = false;
    }
};
#endif

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

#ifdef DEBUG
    //auto bleSrv = NimBLEDevice::createServer();
    //auto bleSrvc = bleSrv->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
    /*bleSerialTx = bleSrvc->createCharacteristic(
        "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
        NIMBLE_PROPERTY::NOTIFY
    );
    auto bleSerialRx = bleSrvc->createCharacteristic(
        "6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
        NIMBLE_PROPERTY::WRITE
    );
    bleSrvc->start();
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(bleSrvc->getUUID());
    //adv->setScanResponse(true);
    adv->start();*/
#endif
    
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