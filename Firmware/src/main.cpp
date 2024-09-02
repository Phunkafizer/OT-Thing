#include <Arduino.h>
#include <WiFi.h>
#include <StreamUtils.h>
#include <ESPmDNS.h>
#include "portal.h"
#include "otcontrol.h"
#include "mqtt.h"
#include "devstatus.h"
#include "devconfig.h"
#include "command.h"
#include "outsidetemp.h"

void wifiEvent(WiFiEvent_t event) {
    Serial.print("ev ");
    Serial.println(event);
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        WiFi.setHostname("otthing");
        //MDNS.addService("http", "tcp", 80);
        break;

    default:
        break;
    }
}

void setup() {
    Serial.begin(9600);
    WiFi.onEvent(wifiEvent);
    WiFi.begin("Senf", "60370682792176389231");
    MDNS.begin("otthing");
    devconfig.begin();
    portal.begin(true); // TODO set configmode according to button
    command.begin();
    //otcontrol.begin();
}

void loop() {
    portal.loop();

    mqtt.loop();
    //otcontrol.loop();
    outsideTemp.loop();
}