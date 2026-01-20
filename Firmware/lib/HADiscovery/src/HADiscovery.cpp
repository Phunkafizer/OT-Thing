#include "HADiscovery.h"

const char *HA_DEVICE_CLASS_RUNNING PROGMEM = "running";
const char *HA_DEVICE_CLASS_PROBLEM PROGMEM = "problem";
const char *HA_DEVICE_CLASS_OPEN PROGMEM = "open";

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
const char HA_PLATFORM[]                        PROGMEM = "p";
const char HA_MIN[]                             PROGMEM = "min";
const char HA_MAX[]                             PROGMEM = "max";
const char HA_STEP[]                            PROGMEM = "step";
const char HA_TEMPERATURE_COMMAND_TOPIC[]       PROGMEM = "temp_cmd_t";
const char HA_MODES[]                           PROGMEM = "modes";
const char HA_MAX_TEMP[]                        PROGMEM = "max_temp";
const char HA_MIN_TEMP[]                        PROGMEM = "min_temp";
const char HA_TEMP_STEP[]                       PROGMEM = "temp_step";
const char HA_TEMPERATURE_STATE_TOPIC[]         PROGMEM = "temp_stat_t";
const char HA_TEMPERATURE_STATE_TEMPLATE[]      PROGMEM = "temp_stat_tpl";
const char HA_CURRENT_TEMPERATURE_TEMPLATE[]    PROGMEM = "curr_temp_tpl";
const char HA_CURRENT_TEMPERATURE_TOPIC[]       PROGMEM = "curr_temp_t";
const char HA_INITIAL[]                         PROGMEM = "initial";
const char HA_MODE_COMMAND_TOPIC[]              PROGMEM = "mode_cmd_t";
const char HA_OPTIMISTIC[]                      PROGMEM = "optimistic";
const char HA_RETAIN[]                          PROGMEM = "retain";

String HADiscovery::ha_prefix = F("homeassistant");
String HADiscovery::devName;

HADiscovery::HADiscovery():
        manufacturer(nullptr) {
}

void HADiscovery::setHAPrefix(String prefix) {
    ha_prefix = prefix;
}

void HADiscovery::init(String &name, String &id, String component) {
    doc.clear();
    JsonObject dev = doc[FPSTR(HA_DEVICE)].to<JsonObject>();
    dev[FPSTR(HA_IDENTIFIERS)][0] = devPrefix;
    dev[FPSTR(HA_SW_VERSION)] = BUILD_VERSION;
    dev[FPSTR(HA_NAME)] = devName;
    dev[FPSTR(HA_MANUFACTURER)] = manufacturer;

    doc[FPSTR(HA_NAME)] = name;
    doc[FPSTR(HA_UNIQUE_ID)] = devPrefix + "_" + id;

    topic = ha_prefix;
    topic += '/' + component + '/' + devPrefix + '/' + id + "/config";

    setStateTopic(defaultStateTopic);
}

void HADiscovery::clearDoc() {
    doc.clear();
}

bool HADiscovery::publish() {
    return false;
}

void HADiscovery::setValueTemplate(String valueTemplate) {
    doc[FPSTR(HA_VALUE_TEMPLATE)] = valueTemplate;
}

void HADiscovery::setTemperatureStateTopic(String topic) {
    doc[FPSTR(HA_TEMPERATURE_STATE_TOPIC)] = topic;
}

void HADiscovery::setTemperatureStateTemplate(String stateTemplate) {
    doc[FPSTR(HA_TEMPERATURE_STATE_TEMPLATE)] = stateTemplate;
}

void HADiscovery::setCurrentTemperatureTopic(String topic) {
    doc[FPSTR(HA_CURRENT_TEMPERATURE_TOPIC)] = topic;
}

void HADiscovery::setCurrentTemperatureTemplate(String templ) {
    doc[FPSTR(HA_CURRENT_TEMPERATURE_TEMPLATE)] = templ;
}

void HADiscovery::setStateTopic(String stateTopic) {
    doc[FPSTR(HA_STATE_TOPIC)] = stateTopic;
}

void HADiscovery::setMinMax(double min, double max, double step) {
    doc[FPSTR(HA_MIN)] = min;
    doc[FPSTR(HA_MAX)] = max;
    doc[FPSTR(HA_STEP)] = step;
}

void HADiscovery::setMinMaxTemp(double min, double max, double step) {
    doc[FPSTR(HA_MIN_TEMP)] = min;
    doc[FPSTR(HA_MAX_TEMP)] = max;
    if (step > 0)
        doc[FPSTR(HA_TEMP_STEP)] = step;
}

void HADiscovery::setInitial(double initial) {
    doc[FPSTR(HA_INITIAL)] = initial;
}

void HADiscovery::setModeCommandTopic(String topic) {
    doc[FPSTR(HA_MODE_COMMAND_TOPIC)] = topic;
}

void HADiscovery::setOptimistic(const bool opt) {
    doc[FPSTR(HA_OPTIMISTIC)] = opt;
}

void HADiscovery::setRetain(const bool retain) {
    doc[FPSTR(HA_RETAIN)] = retain;
}

void HADiscovery::setIcon(String icon) {
    doc[FPSTR(HA_ICON)] = icon;
}

void HADiscovery::setModes(const uint8_t modes) {
    JsonArray jModes = doc[F("modes")].to<JsonArray>();
    jModes.clear();

    if ( (modes & (1<<0)) != 0)
        jModes.add(F("off"));
    if ( (modes & (1<<1)) != 0)
        jModes.add(F("heat"));
    if ( (modes & (1<<2)) != 0)
        jModes.add(F("auto"));
}

void HADiscovery::setUnit(const String unit) {
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = unit;
}

void HADiscovery::setDeviceClass(const String dc) {
    doc[FPSTR(HA_DEVICE_CLASS)] = dc;
}

void HADiscovery::setStateClass(const String sc) {
    if (sc.isEmpty())
        doc.remove(FPSTR(HA_STATE_CLASS));
    else
        doc[FPSTR(HA_STATE_CLASS)] = sc;
}

void HADiscovery::createSensor(String name, String id) {
    init(name, id, F("sensor"));
    doc[FPSTR(HA_STATE_CLASS)] = F("measurement");
}

void HADiscovery::createTempSensor(String name, String id) {
    createSensor(name, id);
    setDeviceClass(F("temperature"));
    setUnit(F("°C"));
}

void HADiscovery::createPowerFactorSensor(String name, String id) {
    createSensor(name, id);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("power_factor");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("%");
}

void HADiscovery::createPressureSensor(String name, String id) {
    createSensor(name, id);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("pressure");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("bar");
}

void HADiscovery::createHourDuration(String name, String id) {
    createSensor(name, id);
    doc[FPSTR(HA_STATE_CLASS)] = F("total_increasing");
    doc[FPSTR(HA_DEVICE_CLASS)] = F("duration");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = F("h");
    doc[FPSTR(HA_ICON)] = F("mdi:timer-sand-complete");
}

void HADiscovery::createBinarySensor(String name, String id, String deviceClass) {
    init(name, id, F("binary_sensor"));
    if (!deviceClass.isEmpty())
        doc[FPSTR(HA_DEVICE_CLASS)] = deviceClass;
}

void HADiscovery::createNumber(String name, String id, String cmdTopic) {
    init(name, id, F("number"));
    doc[FPSTR(HA_PLATFORM)] = F("number");
    doc[FPSTR(HA_COMMAND_TOPIC)] = cmdTopic;
}

void HADiscovery::createClima(String name, String id, String tmpCmdTopic) {
    init(name, id, F("climate"));
    doc[FPSTR(HA_TEMPERATURE_COMMAND_TOPIC)] = tmpCmdTopic;
    setModes(0x07); // off, heat, auto
}

void HADiscovery::createrWaterHeater(String name, String id, String tmpCmdTopic) {
    init(name, id, F("water_heater"));

    JsonArray jModes = doc[F("modes")].to<JsonArray>();
    jModes.clear();
    jModes.add(F("off"));
    jModes.add(F("gas"));

    doc[FPSTR(HA_TEMPERATURE_COMMAND_TOPIC)] = tmpCmdTopic;
}

void HADiscovery::createSwitch(String name, String id, String cmdTopic) {
    init(name, id, F("switch"));
    doc[FPSTR(HA_COMMAND_TOPIC)] = cmdTopic;
}