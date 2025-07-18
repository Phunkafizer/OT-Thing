#include "HADiscLocal.h"
#include <WiFi.h>
#include "mqtt.h"

OTThingHADiscovery haDisc;

const char *DEVNAME PROGMEM = "OT Thing";
const char *MANUFACTURER PROGMEM = "Seegel Systeme";

OTThingHADiscovery::OTThingHADiscovery() {
    devName = DEVNAME;
    manufacturer = MANUFACTURER;
}

void OTThingHADiscovery::begin() {
    String shortMac = WiFi.macAddress();
    shortMac.remove(0, 9);
    int idx;
    while ( (idx = shortMac.indexOf(':')) >= 0)
        shortMac.remove(idx, 1);
    devPrefix = F("otthing_");
    devPrefix += shortMac;
}

bool OTThingHADiscovery::publish() {
    return mqtt.publish(topic, doc, true);
}