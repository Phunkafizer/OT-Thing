#include "HADiscLocal.h"
#include <WiFi.h>
#include "mqtt.h"

OTThingHADiscovery haDisc;

const char *DEVNAME PROGMEM = "OTthing";
const char *MANUFACTURER PROGMEM = "Seegel Systeme";

OTThingHADiscovery::OTThingHADiscovery() {
    manufacturer = MANUFACTURER;
}

void OTThingHADiscovery::begin() {
    // set here, not in the ctor: static init order of devName is undefined
    devName = FPSTR(DEVNAME);

    String shortMac = WiFi.macAddress();
    shortMac.remove(0, 9);
    int idx;
    while ( (idx = shortMac.indexOf(':')) >= 0)
        shortMac.remove(idx, 1);
    devPrefix = F("otthing_");
    devPrefix += shortMac;
}

void OTThingHADiscovery::createSwitch(String name, Mqtt::MqttTopic topic) {
    HADiscovery::createSwitch(name, Mqtt::getTopicString(topic), mqtt.getCmdTopic(topic));
    haDisc.setRetain(true);
}

bool OTThingHADiscovery::publish(const bool avail) {
    if (!avail)
        haDisc.clearDoc();
    else {
         // Expose direct web configuration endpoint in HA device info using IP only.
        String cfgUrl = F("http://");
        cfgUrl += WiFi.localIP().toString();
        doc[F("dev")][F("cu")] = cfgUrl;
    }

    return mqtt.publish(topic, doc, true);
}

HADiscovery::ClimateAction OTThingHADiscovery::calcAction(const bool active, const bool enabled, const HADiscovery::ClimateAction actAction) {
    if (active)
        return actAction;

    return enabled ? HADiscovery::ACTION_IDLE : HADiscovery::ACTION_OFF;
}