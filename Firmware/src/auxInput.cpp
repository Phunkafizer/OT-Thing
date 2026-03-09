#include "auxInput.h"
#include <Arduino.h>
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
    mode(MODE_UNUSED) {
}

void AuxInput::setConfig(JsonObject cfg) {
    mode = (Mode) (cfg[F("mode")] | MODE_UNUSED);

    switch (mode) {
    case MODE_BINARY:
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
        haDisc.createBinarySensor(F("digital input"), FPSTR(name), "");
        String str = F("{{ None if value_json.#.state is not defined else 'ON' if value_json.#.state else 'OFF' }}");
        str.replace("#", FPSTR(name));
        haDisc.setValueTemplate(str);
        return haDisc.publish();
    }
    case MODE_ANALOG: {
        haDisc.createSensor(F("analog input"), FPSTR(name));
        String str = F("{{ value_json.#.value | default(None) }}");
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