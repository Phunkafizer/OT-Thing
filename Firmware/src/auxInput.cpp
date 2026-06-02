#include "auxInput.h"
#include <Arduino.h>
#include "otcontrol.h"
#include "HADiscLocal.h"
#include "sensors.h"
#include "hwdef.h"

AuxInput auxInput[2] = {
    AuxInput(PSTR("aux1"), GPIO_1WIRE_DIO),
    AuxInput(PSTR("aux2"), GPIO_AUX_IN)
};

AuxInput::AuxInput(const char *name, const uint8_t gpio):
    state(false),
    name(name),
    gpio(gpio),
    mode(MODE_UNUSED),
    digitalRole(DROLE_NONE) {
}

void AuxInput::setConfig(JsonObject cfg) {
    mode = (Mode) (cfg[F("mode")] | MODE_UNUSED);
    digitalRole = DROLE_NONE;

    switch (mode) {
    case MODE_BINARY:
        digitalRole = (DigitalRole) (cfg[F("digitalRole")] | DROLE_NONE);
        pinMode(gpio, INPUT);
        // Invert initial state so first loop() always syncs the role state
        state = (digitalRole != DROLE_NONE) ? !(digitalRead(gpio) == 0) : (digitalRead(gpio) == 0);
        break;

    case MODE_COUNTER:
        pinMode(gpio, INPUT);
        state = digitalRead(gpio) == 0;
        break;

    case MODE_ANALOG:
        pinMode(gpio, ANALOG);
        analogSetAttenuation(ADC_11db);
        break;

    case MODE_1WIRE:
        OneWireNode::begin(gpio);
        break;

    default:
        break;
    }
}

void AuxInput::loop() {
    if (mode != MODE_BINARY || digitalRole == DROLE_NONE)
        return;
    bool now = (digitalRead(gpio) == 0);
    if (now == state)
        return;
    state = now;
    uint8_t ch = (digitalRole == DROLE_DEMAND_CH2 || digitalRole == DROLE_ENABLE_CH2) ? 1 : 0;
    if (digitalRole == DROLE_ENABLE_CH1 || digitalRole == DROLE_ENABLE_CH2)
        otcontrol.setAuxEnable(ch, state);
    else
        otcontrol.setAuxDemand(ch, state);
}

void AuxInput::getJson(JsonDocument &doc) const {
    JsonObject obj = doc[FPSTR(name)].to<JsonObject>();
    switch (mode) {
    case MODE_BINARY:
        obj[F("state")] = digitalRead(gpio) == 0;
        break;
    case MODE_ANALOG:
        obj[F("value")] = analogRead(gpio);
        break;
    default:
        break;
    }
}

bool AuxInput::sendDiscovery() {
    switch (mode) {
    case MODE_BINARY: {
        const __FlashStringHelper *label = (digitalRole != DROLE_NONE)
            ? F("heating demand") : F("digital input");
        haDisc.createBinarySensor(label, FPSTR(name), "");
        String str = F("{% set tmp=(value_json.get('#') or {}).get('state') %}{{ none if tmp is none else 'ON' if tmp else 'OFF' }}");
        str.replace("#", FPSTR(name));
        haDisc.setValueTemplate(str);
        return haDisc.publish();
    }
    case MODE_ANALOG: {
        haDisc.createSensor(F("analog input"), FPSTR(name));
        String str = F("{% set tmp=(value_json.get('#') or {}).get('value') %}} {{ tmp | default(none) }}");
        str.replace("#", FPSTR(name));
        haDisc.setValueTemplate(str);
        return haDisc.publish();
    }
    default:
        haDisc.createBinarySensor("", FPSTR(name), "");
        haDisc.publish(false);
        haDisc.createSensor("", FPSTR(name));
        haDisc.publish(false);
        break;
    }
    return true;
}

bool AuxInput::hasEnableRole(uint8_t channel) {
    DigitalRole target = (channel == 0) ? DROLE_ENABLE_CH1 : DROLE_ENABLE_CH2;
    for (const auto& a : auxInput)
        if (a.mode == MODE_BINARY && a.digitalRole == target)
            return true;
    return false;
}
