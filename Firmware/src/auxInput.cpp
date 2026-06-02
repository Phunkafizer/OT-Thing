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
    switch (mode) {
    case MODE_BINARY: {
        bool state = (digitalRead(gpio) == 0);
        break;
    }
    default:
        break;
    }
}

void AuxInput::getJson(JsonDocument &doc) const {
    JsonObject obj = doc[FPSTR(name)].to<JsonObject>();
    switch (mode) {
    case MODE_BINARY:
        obj[F("state")] = state;
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
        String label;
        switch (digitalRole) {
        case DROLE_DEMAND_CH1:
        case DROLE_DEMAND_CH2:
            label = F("heating demand");
            break;
        case DROLE_ENABLE_CH1:
        case DROLE_ENABLE_CH2:
            label = F("heating enable");
            break;
        default:
            label = F("digital input");
            break;
        }

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

bool AuxInput::hasChDemand(const uint8_t channel) {
    const DigitalRole target = (channel == 0) ? DROLE_DEMAND_CH1 : DROLE_DEMAND_CH2;
    for (const auto& aux: auxInput) {
        if ( (aux.mode == MODE_BINARY) && (aux.digitalRole == target) && aux.state )
            return true;
    }
    return false;
}

bool AuxInput::hasChDisable(const uint8_t channel) {
    const DigitalRole target = (channel == 0) ? DROLE_ENABLE_CH1 : DROLE_ENABLE_CH2;
    for (const auto& aux: auxInput) {
        if ( (aux.mode == MODE_BINARY) && (aux.digitalRole == target) && !aux.state )
            return true;
    }
    return false;
}
