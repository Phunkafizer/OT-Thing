#include "auxInput.h"
#include <Arduino.h>
#include "HADiscLocal.h"

const uint8_t PIN_AUX = 5;

AuxInput auxInput;

void AuxInput::setConfig(JsonObject cfg) {
    mode = (Mode) (cfg[F("mode")] | MODE_UNUSED);

    switch (mode) {
    case MODE_BINARY:
    case MODE_COUNTER:
        pinMode(PIN_AUX, INPUT);
        state = digitalRead(PIN_AUX) == 0;
        break;

    case MODE_ANALOG:
        pinMode(5, ANALOG);
        break;
    }
}

void AuxInput::loop() {
}

void AuxInput::getJson(JsonDocument &doc) const {
    JsonObject obj = doc[F("auxInput")].to<JsonObject>();
    switch (mode) {
    case MODE_BINARY:
        obj[F("state")] = digitalRead(PIN_AUX) == 0;
        break;
    case MODE_ANALOG:
        obj[F("value")] = analogRead(PIN_AUX);
        break;
    }
}

bool AuxInput::sendDiscovery() {
    switch (mode) {
    case MODE_BINARY: {
        haDisc.createBinarySensor(F("digital input"), F("aux_in"), "");
        String str = F("{{ None if value_json.auxInput.state is not defined else 'ON' if value_json.auxInput.state else 'OFF' }}");
        haDisc.setValueTemplate(str);
        break;
    }
    case MODE_ANALOG: {
        haDisc.createSensor(F("analog input"), F("aux_in"));
        String str = F("{{ value_json.auxInput.value | default(None) }}");
        haDisc.setValueTemplate(str);
        break;
    }
    default:
        break;
    }

    return haDisc.publish(mode != MODE_UNUSED);
}