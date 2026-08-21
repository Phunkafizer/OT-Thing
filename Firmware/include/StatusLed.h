#pragma once

#include <Ticker.h>

class StatusLed {
private:
    Ticker ticker;
    uint16_t pattern;
    uint16_t mask;
public:
    enum Pattern {
        LED_NORMAL = 0b1000000000000000,
        LED_CONFIG = 0b1010000000000000,
        LED_WPS = 0b1010100000000000,
    };

    void begin();
    void end();
    void set(const Pattern pattern);
};

extern StatusLed statusLed;