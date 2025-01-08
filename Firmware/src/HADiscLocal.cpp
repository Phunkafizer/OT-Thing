#include "HADiscLocal.h"
#include <WiFi.h>

OTThingHADiscovery haDisc;

const char *DEVNAME PROGMEM = "OT Thing";
const char *MANUFACTURER PROGMEM = "Seegel Systeme";

OTThingHADiscovery::OTThingHADiscovery() {
    devName = DEVNAME;
    manufacturer = MANUFACTURER;

    String shortMac = WiFi.macAddress();
    shortMac.remove(0, 9);
    int idx;
    while ( (idx = shortMac.indexOf(':')) >= 0)
        shortMac.remove(idx, 1);
    devPrefix = F("otthing_");
    devPrefix += shortMac;
}