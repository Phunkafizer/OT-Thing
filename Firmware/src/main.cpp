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
//#include <OneWire.h>
//#include <DallasTemperature.h>

#ifdef DEBUG
#include <ArduinoOTA.h>
#endif

Ticker statusLedTicker;
volatile uint16_t statusLedData = 0x8000;
bool configMode = false;
const char hostname[] PROGMEM = HOSTNAME;

//OneWire oneWire(4);

void statusLedLoop() {
    static uint16_t mask = 0x8000;

    setLedStatus((statusLedData & mask) != 0);
    mask >>= 1;
    if (!mask)
        mask = 0x8000;
}

void wifiEvent(WiFiEvent_t event) {
    Serial.print("WiFi Event ");
    Serial.println(event);

    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
        String hn(FPSTR(hostname));
        WiFi.setHostname(hn.c_str());
        MDNS.addService("http", "tcp", 80);
        Serial.println(WiFi.localIP().toString());
        break;
    }

    default:
        break;
    }
}

void setup() {
    pinMode(GPIO_BYPASS_RELAY, OUTPUT);
    pinMode(GPIO_OTRED_LED, OUTPUT);
    pinMode(GPIO_OTGREEN_LED, OUTPUT);
    pinMode(GPIO_STATUS_LED, OUTPUT);
    pinMode(GPIO_CONFIG_BUTTON, INPUT);
    digitalWrite(GPIO_BYPASS_RELAY, LOW);
    
    setLedOTGreen(false);
    setLedOTRed(false);
    setLedStatus(false);

    statusLedTicker.attach(0.2, statusLedLoop);

    unsigned long ts = millis();
    while (millis() - ts < 1000)
        if (digitalRead(GPIO_CONFIG_BUTTON) != 0)
            break;

    configMode |= millis() - ts >= 1000;
    if (configMode)
        statusLedData = 0xA000;

    Serial.begin();
    //Serial0.begin(460800);
    WiFi.onEvent(wifiEvent);
    WiFi.begin();

    mqtt.begin();
    String hn(FPSTR(hostname));
    MDNS.begin(hn.c_str());
    devconfig.begin();
    portal.begin(configMode);
    command.begin();
    otcontrol.begin();

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
    
    static uint32_t lastSensors = 0;
    if (now - lastSensors > 10000) {
      /*  uint8_t addr[8];

    if (now - lastSensors > 1000) {
        return;
        uint8_t addr[8];
        if (!oneWire.search(addr)) {
            oneWire.reset_search();
            lastSensors = now;
        }
        else {
            String adr;
            for (int i=0; i<8; i++) {
                if (addr[i] < 0x10)
                    adr += '0';
                adr += String(addr[i], HEX);
            }

            DallasTemperature ds(&oneWire);
            ds.requestTemperatures();
            double t = ds.getTempC(addr);
            JsonDocument doc;
            doc["temp"] = t;
            String topic = mqtt.getBaseTopic() + F("/1wire/") + adr;
            mqtt.publish(topic, doc, false);
        }*/
    }
}