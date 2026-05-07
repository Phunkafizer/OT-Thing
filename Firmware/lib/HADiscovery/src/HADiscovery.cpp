#include "HADiscovery.h"

PGM_P HA_DEVICE_CLASS_RUNNING PROGMEM = "running";
PGM_P HA_DEVICE_CLASS_PROBLEM PROGMEM = "problem";
PGM_P HA_DEVICE_CLASS_OPENING PROGMEM = "opening";
PGM_P HA_DEVICE_CLASS_TEMPERATURE PROGMEM = "temperature";
PGM_P HA_DEVICE_CLASS_CARBON_DIOXIDE PROGMEM = "carbon_dioxide";
PGM_P HA_DEVICE_CLASS_HUMIDITY PROGMEM = "humidity";
PGM_P HA_DEVICE_CLASS_VOLUME_FLOW_RATE PROGMEM = "volume_flow_rate";
PGM_P HA_DEVICE_CLASS_CURRENT PROGMEM = "current";

PGM_P HA_UNIT_PPM PROGMEM = "ppm";
PGM_P HA_UNIT_RPM PROGMEM = "rpm";
PGM_P HA_UNIT_HZ PROGMEM = "Hz";
PGM_P HA_UNIT_PERCENT PROGMEM = "%";
PGM_P HA_UNIT_CELSIUS PROGMEM = "°C";
PGM_P HA_UNIT_KELVIN PROGMEM = "K";
PGM_P HA_UNIT_L_MIN PROGMEM = "L/min";

PGM_P HA_ENTITY_CATEGORY_CONFIG = "config";
PGM_P HA_ENTITY_CATEGORY_DIAGNOSTIC = "diagnostic";

PGM_P HA_CLIMATE_MODE_OFF PROGMEM = "off";
PGM_P HA_CLIMATE_MODE_HEAT PROGMEM = "heat";
PGM_P HA_CLIMATE_MODE_AUTO PROGMEM = "auto";
PGM_P HA_CLIMATE_MODE_COOL PROGMEM = "cool";

PGM_P HA_ACTION_OFF PROGMEM = "off";
PGM_P HA_ACTION_HEATING PROGMEM = "heating";
PGM_P HA_ACTION_COOLING PROGMEM = "cooling";
PGM_P HA_ACTION_IDLE PROGMEM = "idle";

PGM_P HA_AVAILABILITY                   PROGMEM = "avty";
PGM_P HA_TOPIC                          PROGMEM = "t";
PGM_P HA_UNIQUE_ID                      PROGMEM = "uniq_id";
PGM_P HA_OBJECT_ID                      PROGMEM = "obj_id";
PGM_P HA_ENTITY_CATEGORY                PROGMEM = "ent_cat";
PGM_P HA_VALUE_TEMPLATE                 PROGMEM = "val_tpl";
PGM_P HA_ENTITY_SWITCH                  PROGMEM = "switch";
PGM_P HA_DEVICE                         PROGMEM = "dev";
PGM_P HA_IDENTIFIERS                    PROGMEM = "ids";
PGM_P HA_SW_VERSION                     PROGMEM = "sw";
PGM_P HA_NAME                           PROGMEM = "name";
PGM_P HA_STATE_TOPIC                    PROGMEM = "stat_t";
PGM_P HA_COMMAND_TOPIC                  PROGMEM = "cmd_t";
PGM_P HA_STATE_CLASS                    PROGMEM = "stat_cla";
PGM_P HA_DEVICE_CLASS                   PROGMEM = "dev_cla";
PGM_P HA_UNIT_OF_MEASUREMENT            PROGMEM = "unit_of_meas";
PGM_P HA_STATE_CLASS_MEASUREMENT        PROGMEM = "measurement";
PGM_P HA_ICON                           PROGMEM = "ic";
PGM_P HA_MANUFACTURER                   PROGMEM = "mf";
PGM_P HA_PLATFORM                       PROGMEM = "p";
PGM_P HA_MIN                            PROGMEM = "min";
PGM_P HA_MAX                            PROGMEM = "max";
PGM_P HA_STEP                           PROGMEM = "step";
PGM_P HA_TEMPERATURE_COMMAND_TOPIC      PROGMEM = "temp_cmd_t";
PGM_P HA_MODES                          PROGMEM = "modes";
PGM_P HA_MAX_TEMP                       PROGMEM = "max_temp";
PGM_P HA_MIN_TEMP                       PROGMEM = "min_temp";
PGM_P HA_TEMP_STEP                      PROGMEM = "temp_step";
PGM_P HA_TEMPERATURE_STATE_TOPIC        PROGMEM = "temp_stat_t";
PGM_P HA_TEMPERATURE_STATE_TEMPLATE     PROGMEM = "temp_stat_tpl";
PGM_P HA_CURRENT_TEMPERATURE_TEMPLATE   PROGMEM = "curr_temp_tpl";
PGM_P HA_CURRENT_TEMPERATURE_TOPIC      PROGMEM = "curr_temp_t";
PGM_P HA_INITIAL                        PROGMEM = "init";
PGM_P HA_MODE_COMMAND_TOPIC             PROGMEM = "mode_cmd_t";
PGM_P HA_OPTIMISTIC                     PROGMEM = "opt";
PGM_P HA_RETAIN                         PROGMEM = "ret";
PGM_P HA_MODE_STATE_TOPIC               PROGMEM = "mode_stat_t";
PGM_P HA_MODE_STATE_TEMPLATE            PROGMEM = "mode_stat_tpl";
PGM_P HA_ACTION_TOPIC                   PROGMEM = "act_t";
PGM_P HA_ACTION_TEMPLATE                PROGMEM = "act_tpl";    

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
    dev[FPSTR(HA_MANUFACTURER)] = FPSTR(manufacturer);

    doc[FPSTR(HA_NAME)] = name;
    doc[FPSTR(HA_UNIQUE_ID)] = devPrefix + "_" + id;

    topic = ha_prefix;
    topic += '/' + component + '/' + devPrefix + '/' + id + F("/config");

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

    if (!doc[FPSTR(HA_TEMPERATURE_STATE_TOPIC)].is<JsonString>())
        doc[FPSTR(HA_TEMPERATURE_STATE_TOPIC)] = defaultStateTopic;
}

void HADiscovery::setCurrentTemperatureTopic(String topic) {
    doc[FPSTR(HA_CURRENT_TEMPERATURE_TOPIC)] = topic;
}

void HADiscovery::setCurrentTemperatureTemplate(String templ) {
    doc[FPSTR(HA_CURRENT_TEMPERATURE_TEMPLATE)] = templ;

    if (!doc[FPSTR(HA_CURRENT_TEMPERATURE_TOPIC)].is<JsonString>())
        doc[FPSTR(HA_CURRENT_TEMPERATURE_TOPIC)] = defaultStateTopic;
}

void HADiscovery::setStateTopic(String stateTopic) {
    if (stateTopic.isEmpty())
        doc.remove(FPSTR(HA_STATE_TOPIC));
    else
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

void HADiscovery::setModeStateTopic(String topic) {
    doc[FPSTR(HA_MODE_STATE_TOPIC)] = topic;
}

void HADiscovery::setModeStateTemplate(String templ) {
    doc[FPSTR(HA_MODE_STATE_TEMPLATE)] = templ;

    if (!doc[FPSTR(HA_MODE_STATE_TOPIC)].is<JsonString>())
        doc[FPSTR(HA_MODE_STATE_TOPIC)] = defaultStateTopic;
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
    if (modes == 0)
        return;
        
    JsonArray jModes = doc[F("modes")].to<JsonArray>();
    jModes.clear();

    if ( (modes & (1<<0)) != 0)
        jModes.add(FPSTR(HA_CLIMATE_MODE_OFF));
    if ( (modes & (1<<1)) != 0)
        jModes.add(FPSTR(HA_CLIMATE_MODE_HEAT));
    if ( (modes & (1<<2)) != 0)
        jModes.add(FPSTR(HA_CLIMATE_MODE_AUTO));
    if ( (modes & (1<<3)) != 0)
        jModes.add(FPSTR(HA_CLIMATE_MODE_COOL));
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

void HADiscovery::setEntityCategory(PGM_P cat) {
    if (cat != nullptr)
        doc[FPSTR(HA_ENTITY_CATEGORY)] = FPSTR(cat);
    else
        doc.remove(FPSTR(HA_ENTITY_CATEGORY));
}

void HADiscovery::setActionTopic(const String topic) {
    doc[FPSTR(HA_ACTION_TOPIC)] = topic;
}

void HADiscovery::setActionTemplate(const String templ) {
    doc[FPSTR(HA_ACTION_TEMPLATE)] = templ;
    if (!doc[FPSTR(HA_ACTION_TOPIC)].is<JsonString>())
        doc[FPSTR(HA_ACTION_TOPIC)] = defaultStateTopic;
}

void HADiscovery::createSensor(String name, String id) {
    init(name, id, F("sensor"));
    doc[FPSTR(HA_STATE_CLASS)] = FPSTR(HA_STATE_CLASS_MEASUREMENT);
}

void HADiscovery::createTempSensor(String name, String id) {
    createSensor(name, id);
    setDeviceClass(FPSTR(HA_DEVICE_CLASS_TEMPERATURE));
    setUnit(PSTR(HA_UNIT_CELSIUS));
}

void HADiscovery::createPowerFactorSensor(String name, String id) {
    createSensor(name, id);
    doc[FPSTR(HA_DEVICE_CLASS)] = F("power_factor");
    doc[FPSTR(HA_UNIT_OF_MEASUREMENT)] = PSTR(HA_UNIT_PERCENT);
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

PGM_P HADiscovery::getClimateModeStr(const ClimateMode mode) {
    switch (mode) {
    case MODE_OFF:
        return HA_CLIMATE_MODE_OFF;
    case MODE_HEAT:
        return HA_CLIMATE_MODE_HEAT;
    case MODE_AUTO:
        return HA_CLIMATE_MODE_AUTO;
    case MODE_COOL:
        return HA_CLIMATE_MODE_COOL;
    default:
        return nullptr;
    }
}

PGM_P HADiscovery::getClimateActionStr(const ClimateAction action) {
    switch (action) {
    case ACTION_OFF:
        return HA_ACTION_OFF;
    case ACTION_HEATING:
        return HA_ACTION_HEATING;
    case ACTION_COOLING:
        return HA_ACTION_COOLING;
    case ACTION_IDLE:
        return HA_ACTION_IDLE;
    default:
        return nullptr;
    }
}

HADiscovery::ClimateMode HADiscovery::strToClimateMode(const String &str) {
    if (str.compareTo(FPSTR(HA_CLIMATE_MODE_COOL)) == 0)
        return MODE_COOL;
    if (str.compareTo(FPSTR(HA_CLIMATE_MODE_HEAT)) == 0)
        return MODE_HEAT;
    if (str.compareTo(FPSTR(HA_CLIMATE_MODE_AUTO)) == 0)
        return MODE_AUTO;
    if (str.compareTo(FPSTR(HA_CLIMATE_MODE_OFF)) == 0)
        return MODE_OFF;
    return MODE_UNKNOWN;
}