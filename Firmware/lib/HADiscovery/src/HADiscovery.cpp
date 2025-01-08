#include "HADiscovery.h"

const char *HA_DEVICE_CLASS_RUNNING PROGMEM = "running";
const char *HA_DEVICE_CLASS_PROBLEM PROGMEM = "problem";

const char HA_AVAILABILITY[]                    PROGMEM = "availability";
const char HA_TOPIC[]                           PROGMEM = "t";
const char HA_UNIQUE_ID[]                       PROGMEM = "uniq_id";
const char HA_OBJECT_ID[]                       PROGMEM = "obj_id";
const char HA_ENTITY_CATEGORY[]                 PROGMEM = "ent_cat";
const char HA_ENTITY_CATEGORY_CONFIG[]          PROGMEM = "config";
const char HA_VALUE_TEMPLATE[]                  PROGMEM = "val_tpl";
const char HA_ENTITY_SWITCH[]                   PROGMEM = "switch";
const char HA_DEVICE[]                          PROGMEM = "dev";
const char HA_IDENTIFIERS[]                     PROGMEM = "ids";
const char HA_SW_VERSION[]                      PROGMEM = "sw";
const char HA_NAME[]                            PROGMEM = "name";
const char HA_STATE_TOPIC[]                     PROGMEM = "stat_t";
const char HA_COMMAND_TOPIC[]                   PROGMEM = "cmd_t";
const char HA_STATE_CLASS[]                     PROGMEM = "stat_cla";
const char HA_DEVICE_CLASS[]                    PROGMEM = "dev_cla";
const char HA_UNIT_OF_MEASUREMENT[]             PROGMEM = "unit_of_meas";
const char HA_STATE_CLASS_MEASUREMENT[]         PROGMEM = "measurement";
const char HA_DEVICE_CLASS_TEMPERATURE[]        PROGMEM = "temperature";
const char HA_ICON[]                            PROGMEM = "ic";
const char HA_MANUFACTURER[]                    PROGMEM = "mf";

const char *ha_prefix = "homeassistant";

HADiscovery::HADiscovery():
        devName(nullptr),
        manufacturer(nullptr) {
}

void HADiscovery::init(String &name, String &id) {
    doc.clear();
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_IDENTIFIERS)][0] = devPrefix;
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_SW_VERSION)] = "1.2";
    doc[FPSTR(HA_DEVICE)][FPSTR(HA_NAME)] = FPSTR(devName);

    doc[FPSTR(HA_NAME)] = name;
    doc[FPSTR(HA_UNIQUE_ID)] = devPrefix + "_" + id;
    doc[FPSTR(HA_OBJECT_ID)] = doc[FPSTR(HA_UNIQUE_ID)];

    doc[FPSTR(HA_STATE_TOPIC)] = stateTopic;

    topic = FPSTR(ha_prefix);
    topic += "/sensor/" + devPrefix + "/" + id + "/config";
}

void HADiscovery::setValueTemplate(String valueTemplate) {
    doc[FPSTR(HA_VALUE_TEMPLATE)] = valueTemplate;
}

void HADiscovery::createTempSensor(String name, String id) {
    init(name, id);

    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("temperature");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("°C"); 
}

void HADiscovery::createPowerFactorSensor(String name, String id) {
    init(name, id);

    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("power_factor");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("%");
}

void HADiscovery::createPressureSensor(String name, String id) {
    init(name, id);

    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("pressure");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("bar");
}

void HADiscovery::createHourDuration(String name, String id) {
    init(name, id);

    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("duration");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("h");
    //doc[FPSTR(HA_ICON)] = F("timer-sand-complete");
}

void HADiscovery::createSensor(String name, String id) {
    init(name, id);
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");
    doc[FPSTR(HA_ICON)] = F("mdi:counter");
}

void HADiscovery::createBinarySensor(String name, String id, String deviceClass) {
    init(name, id);

    doc[FPSTR(HA_DEVICE_CLASS)] = deviceClass;

    topic = FPSTR(ha_prefix);
    topic += "/binary_sensor/" + devPrefix + "/" + id + "/config";
}