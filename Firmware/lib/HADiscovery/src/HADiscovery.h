#pragma once
#include <ArduinoJson.h>

extern PGM_P HA_DEVICE_CLASS_RUNNING PROGMEM;
extern PGM_P HA_DEVICE_CLASS_PROBLEM PROGMEM;
extern PGM_P HA_DEVICE_CLASS_HEAT PROGMEM;
extern PGM_P HA_DEVICE_CLASS_OPENING PROGMEM;
extern PGM_P HA_DEVICE_CLASS_TEMPERATURE PROGMEM;
extern PGM_P HA_DEVICE_CLASS_CARBON_DIOXIDE PROGMEM;
extern PGM_P HA_DEVICE_CLASS_HUMIDITY PROGMEM;
extern PGM_P HA_DEVICE_CLASS_VOLUME_FLOW_RATE PROGMEM;
extern PGM_P HA_DEVICE_CLASS_CURRENT PROGMEM;

extern PGM_P HA_UNIT_PPM PROGMEM;
extern PGM_P HA_UNIT_RPM PROGMEM;
extern PGM_P HA_UNIT_HZ PROGMEM;
extern PGM_P HA_UNIT_PERCENT PROGMEM;
extern PGM_P HA_UNIT_CELSIUS PROGMEM;
extern PGM_P HA_UNIT_KELVIN PROGMEM;
extern PGM_P HA_UNIT_L_MIN PROGMEM;

extern PGM_P HA_ENTITY_CATEGORY_CONFIG PROGMEM;
extern PGM_P HA_ENTITY_CATEGORY_DIAGNOSTIC PROGMEM;


class HADiscovery {
private:
    void init(String &name, String &id, String component);
    static String ha_prefix;
protected:
    JsonDocument doc;
    String topic;
public:
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
    void setTemperatureStateTopic();
    void setTemperatureStateTemplate(String stateTemplate);
    void setCurrentTemperatureTopic(String topic);
    void setCurrentTemperatureTopic();
    void setCurrentTemperatureTemplate(String templ);
    void setInitial(double initial);
    void setModeCommandTopic(String topic);
    void setModeStateTopic(String topic);
    void setModeStateTopic();
    void setModeStateTemplate(String templ);
    void setOptimistic(const bool opt);
    void setRetain(const bool retain);
    void setIcon(String icon);
    void setModes(const uint8_t modes);
    void setUnit(const String unit);
    void setDeviceClass(const String dc);
    void setStateClass(const String sc);
    void setEntityCategory(PGM_P cat);

    void createTempSensor(String name, String id);
    void createPowerFactorSensor(String name, String id);
    void createPressureSensor(String name, String id);
    void createHourDuration(String name, String id);
    void createSensor(String name, String id);
    void createBinarySensor(String name, String id, String deviceClass);
    void createNumber(String name, String id, String cmdTopic);
    void createClima(String name, String id, String tmpCmdTopic);
    void createSwitch(String name, String id, String cmdTopic);
    void createrWaterHeater(String name, String id, String tmpCmdTopic);
};
