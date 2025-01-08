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
#include "outsidetemp.h"

#ifdef DEBUG
#include <ArduinoOTA.h>
#endif

Ticker statusLedTicker;
volatile uint16_t statusLedData = 0x8000;
bool configMode = false;

void statusLedLoop() {
    static uint16_t mask = 0x8000;

    digitalWrite(GPIO_STATUS_LED, (statusLedData & mask) == 0);
    mask >>= 1;
    if (!mask)
        mask = 0x8000;
}

void wifiEvent(WiFiEvent_t event) {
    Serial.print("WiFi Event ");
    Serial.println(event);

    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        WiFi.setHostname("otthing");
        MDNS.addService("http", "tcp", 80);
        break;

    default:
        break;
    }
}

void setup() {
    pinMode(20, OUTPUT);
    pinMode(21, OUTPUT);
    pinMode(2, OUTPUT);
    pinMode(GPIO_STATUS_LED, OUTPUT);
    pinMode(GPIO_CONFIG_BUTTON, INPUT);
    digitalWrite(20, LOW);
    digitalWrite(21, LOW);
    digitalWrite(2, HIGH); // active low
    digitalWrite(GPIO_STATUS_LED, HIGH); // active low

    statusLedTicker.attach(0.2, statusLedLoop);

    unsigned long ts = millis();
    while (millis() - ts < 1000)
        if (digitalRead(GPIO_CONFIG_BUTTON) != 0)
            break;

    configMode |= millis() - ts >= 1000;
    if (configMode)
        statusLedData = 0xA000;

    Serial.begin();
    WiFi.onEvent(wifiEvent);
    WiFi.begin();

    //MDNS.begin("otthing");
    devconfig.begin();
    portal.begin(configMode);
    command.begin();
    otcontrol.begin();

#ifdef DEBUG
    ArduinoOTA.begin();
#endif
}

void loop() {
#ifdef DEBUG
    ArduinoOTA.handle();
#endif
    portal.loop();
    mqtt.loop();
    otcontrol.loop();
    outsideTemp.loop();
}