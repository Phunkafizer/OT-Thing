#pragma once
#include <ArduinoJson.h>

inline constexpr PGM_P HA_DEVICE_CLASS_RUNNING PROGMEM = "running";
inline constexpr PGM_P HA_DEVICE_CLASS_PROBLEM PROGMEM = "problem";
inline constexpr PGM_P HA_DEVICE_CLASS_HEAT PROGMEM = "heat";
inline constexpr PGM_P HA_DEVICE_CLASS_OPENING PROGMEM = "opening";
inline constexpr PGM_P HA_DEVICE_CLASS_TEMPERATURE PROGMEM = "temperature";
inline constexpr PGM_P HA_DEVICE_CLASS_CARBON_DIOXIDE PROGMEM = "carbon_dioxide";
inline constexpr PGM_P HA_DEVICE_CLASS_HUMIDITY PROGMEM = "humidity";
inline constexpr PGM_P HA_DEVICE_CLASS_VOLUME_FLOW_RATE PROGMEM = "volume_flow_rate";
inline constexpr PGM_P HA_DEVICE_CLASS_CURRENT PROGMEM = "current";
inline constexpr PGM_P HA_DEVICE_CLASS_DURATION PROGMEM = "duration";
inline constexpr PGM_P HA_DEVICE_CLASS_CONNECTIVITY PROGMEM = "connectivity";

inline constexpr PGM_P HA_UNIT_PPM PROGMEM = "ppm";
inline constexpr PGM_P HA_UNIT_RPM PROGMEM = "rpm";
inline constexpr PGM_P HA_UNIT_HZ PROGMEM = "Hz";
inline constexpr PGM_P HA_UNIT_PERCENT PROGMEM = "%";
inline constexpr PGM_P HA_UNIT_CELSIUS PROGMEM = "°C";
inline constexpr PGM_P HA_UNIT_KELVIN PROGMEM = "K";
inline constexpr PGM_P HA_UNIT_L_MIN PROGMEM = "L/min";
inline constexpr PGM_P HA_UNIT_MIN PROGMEM = "min";

inline constexpr PGM_P HA_ENTITY_CATEGORY_CONFIG PROGMEM = "config";
inline constexpr PGM_P HA_ENTITY_CATEGORY_DIAGNOSTIC PROGMEM = "diagnostic";

inline constexpr PGM_P HA_CLIMATE_MODE_OFF PROGMEM = "off";
inline constexpr PGM_P HA_CLIMATE_MODE_HEAT PROGMEM = "heat";
inline constexpr PGM_P HA_CLIMATE_MODE_AUTO PROGMEM = "auto";
inline constexpr PGM_P HA_CLIMATE_MODE_COOL PROGMEM = "cool";

inline constexpr PGM_P HA_ACTION_OFF PROGMEM = "off";
inline constexpr PGM_P HA_ACTION_HEATING PROGMEM = "heating";
inline constexpr PGM_P HA_ACTION_COOLING PROGMEM = "cooling";
inline constexpr PGM_P HA_ACTION_IDLE PROGMEM = "idle";


class HADiscovery {
private:
    void init(String &name, String &id, String component);
    static String ha_prefix;
protected:
    JsonDocument doc;
    String topic;
public:
    enum ClimateMode {
        MODE_UNKNOWN = -1,
        MODE_OFF,
        MODE_HEAT,
        MODE_AUTO,
        MODE_COOL
    };
    enum ClimateAction {
        ACTION_OFF,
        ACTION_HEATING,
        ACTION_COOLING,
        ACTION_IDLE
    };
    HADiscovery();
    static String devName;
    PGM_P manufacturer;
    String devPrefix;
    String defaultStateTopic;
    static void setHAPrefix(String prefix);
    virtual bool publish();
    void clearDoc();
    void setValueTemplate(String valueTemplate);
    void setStateTopic(String stateTopic);
    void setMinMax(double min, double max, double step);
    void setMinMaxTemp(double min, double max, double step = 0);
    void setTemperatureStateTopic(String topic);
    void setTemperatureStateTemplate(String stateTemplate);
    void setCurrentTemperatureTopic(String topic);
    void setCurrentTemperatureTemplate(String templ);
    void setInitial(double initial);
    void setModeCommandTopic(String topic);
    void setModeStateTopic(String topic);
    void setModeStateTemplate(String templ);
    void setOptimistic(const bool opt);
    void setRetain(const bool retain);
    void setIcon(String icon);
    void setModes(const uint8_t modes);
    void setUnit(const String unit);
    void setDeviceClass(const String dc);
    void setStateClass(const String sc);
    void setEntityCategory(PGM_P cat);
    void setActionTopic(const String topic);
    void setActionTemplate(const String templ);
    void setAvailability(const String &topic, const String &templ);

    void createTempSensor(String name, String id);
    void createPowerFactorSensor(String name, String id);
    void createPressureSensor(String name, String id);
    void createHourDuration(String name, String id);
    void createSensor(String name, String id);
    void createBinarySensor(String name, String id, String deviceClass = "");
    void createNumber(String name, String id, String cmdTopic);
    void createClima(String name, String id, String tmpCmdTopic);
    void createSwitch(String name, String id, String cmdTopic);
    void createrWaterHeater(String name, String id, String tmpCmdTopic);

    PGM_P getClimateModeStr(const ClimateMode mode);
    PGM_P getClimateActionStr(const ClimateAction action);
    ClimateMode strToClimateMode(const String &str);
};
