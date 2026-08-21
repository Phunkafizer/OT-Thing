#include <Arduino.h>
#include "StatusLed.h"
#include "hwdef.h"

StatusLed statusLed;

void StatusLed::begin() {
    pinMode(GPIO_STATUS_LED, OUTPUT);
    setLedStatus(false);
    set(Pattern::LED_NORMAL);

    ticker.attach_ms(200, [this]() {
        setLedStatus((pattern & mask) != 0);
        mask >>= 1;
        if (!mask)
            mask = 0x8000;
    });
}

void StatusLed::end() {
    ticker.detach();
    setLedStatus(false);
}

void StatusLed::set(const Pattern pattern) {
    this->pattern = pattern;
    mask = 0x0001;
}