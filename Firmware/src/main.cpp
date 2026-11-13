#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "hwdef.h"
#include "portal.h"
#include "otcontrol.h"
#include "mqtt.h"
#include "devstatus.h"
#include "devconfig.h"
#include "command.h"
#include "sensors.h"
#include "HADiscLocal.h"
#include "main.h"
#include "util.h"
#include "esp_task_wdt.h"
#include "auxInput.h"
#include "netw.h"
#include "StatusLed.h"

#ifdef DEBUG
    #include <ArduinoOTA.h>
#endif

bool configMode = false;

#ifdef DEBUG
// Required for DEBUG build: definitions for BLE externs referenced by command.cpp.
NimBLECharacteristic *bleSerialTx = nullptr;
volatile bool bleClientConnected = false;
#endif


bool readHWVersion(uint8_t &hwMajor, uint8_t &hwMinor) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    return false;
  }

  nvs_handle_t my_handle;
  err = nvs_open("hw_info", NVS_READONLY, &my_handle);
  if (err != ESP_OK)
    return false;

  // 3. Beide Werte auslesen und prüfen, ob beide Schlüssel existieren
  esp_err_t errMajor = nvs_get_u8(my_handle, "major", &hwMajor);
  esp_err_t errMinor = nvs_get_u8(my_handle, "minor", &hwMinor);

  nvs_close(my_handle);

  return (errMajor == ESP_OK && errMinor == ESP_OK);
}

void setup() {
    statusLed.begin();
    pinMode(GPIO_CONFIG_BUTTON, INPUT);
    pinMode(GPIO_BOOT_BUTTON, INPUT_PULLUP);
    pinMode(GPIO_BYPASS_RELAY, INPUT_PULLUP);

    Serial.begin();
    Serial.setTxTimeoutMs(100);

    configTime(0, 0, PSTR("pool.ntp.org"));

    otcontrol.begin();
    devconfig.begin();

    configMode = digitalRead(GPIO_CONFIG_BUTTON) == 0;
    configMode |= !netw.hasStoredWifiCredentials();
    if (configMode)
        statusLed.set(StatusLed::LED_CONFIG);

    netw.begin(configMode);
    
    AddressableSensor::begin();
    BLESensor::begin();
    haDisc.begin();
    mqtt.begin();

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
            statusLed.end();
            setLedStatus(true);
            devconfig.remove();
            netw.end();
            WiFi.persistent(true);
            WiFi.disconnect(true, true);
            esp_wifi_restore();
            while (digitalRead(GPIO_CONFIG_BUTTON) == 0) {
                esp_task_wdt_reset();
                yield();
            }
            ESP.restart();
        }
        esp_task_wdt_reset();
        return;
    }
    else
        btnDown = now;

    static bool oldBootButtonState = false;

    if (oldBootButtonState != (digitalRead(GPIO_BOOT_BUTTON) == 0)) {
        netw.startWps();
        oldBootButtonState = digitalRead(GPIO_BOOT_BUTTON) == 0;
    }

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

    if (configMode)
        netw.loop();

    for (int i=0; i<sizeof(auxInput) / sizeof(auxInput[0]); i++)
        auxInput[i].loop();
}