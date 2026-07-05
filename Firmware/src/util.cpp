#include "util.h"
#include <WiFi.h>
#include <ESPmDNS.h>

SemHelper::SemHelper(SemaphoreHandle_t &mtx, const uint16_t timeout):
        mtx(mtx) {
    result = xSemaphoreTake(mtx, (TickType_t) timeout / portTICK_PERIOD_MS);
}

SemHelper::~SemHelper() {
    if (*this)
        xSemaphoreGive(mtx);
}

SemHelper::operator bool() {
    return result == pdTRUE;
}


void clip(double &d, const double min, const double max) {
    if (d < min)
        d = min;
    if (d > max)
        d = max;
}

String getShortMac() {
    String shortMac = WiFi.macAddress();
    shortMac.remove(0, 9);
    int idx;
    while ((idx = shortMac.indexOf(':')) >= 0)
        shortMac.remove(idx, 1);
    return shortMac;
}

void publishMdnsServices() {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("otthing", "tcp", 80);

    MDNS.addServiceTxt("otthing", "tcp", "product", "OTThing");
    MDNS.addServiceTxt("otthing", "tcp", "fw", BUILD_VERSION);
    MDNS.addServiceTxt("otthing", "tcp", "api", "1");
    MDNS.addServiceTxt("otthing", "tcp", "id", getShortMac());
    MDNS.addServiceTxt("otthing", "tcp", "path", "/");
}