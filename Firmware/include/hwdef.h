#pragma once

#define GPIO_CONFIG_BUTTON 0
#define GPIO_STATUS_LED 8
#define GPIO_OTRED_LED 2
#define GPIO_OTGREEN_LED 21
#define GPIO_BYPASS_RELAY 20
#define GPIO_STEPUP_ENABLE 10
#define GPIO_1WIRE_DIO 4
#define GPIO_OTSLAVE_IN 6
#define GPIO_OTSLAVE_OUT 7
#define GPIO_OTMASTER_IN 3
#define GPIO_OTMASTER_OUT 1

inline void setLedOTRed(const bool on) {
    digitalWrite(GPIO_OTRED_LED, !on);
}

inline void setLedOTGreen(const bool on) {
    digitalWrite(GPIO_OTGREEN_LED, on);
}

inline void setLedStatus(const bool on) {
    digitalWrite(GPIO_STATUS_LED, !on);
}