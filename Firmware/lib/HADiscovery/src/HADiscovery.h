#pragma once
#include <ArduinoJson.h>

extern const char *HA_DEVICE_CLASS_RUNNING PROGMEM;
extern const char *HA_DEVICE_CLASS_PROBLEM PROGMEM;
extern const char *HA_DEVICE_CLASS_HEAT PROGMEM;
extern const char *HA_DEVICE_CLASS_OPENING PROGMEM;

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
    const char *manufacturer;
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
    void setOptimistic(const bool opt);
    void setRetain(const bool retain);
    void setIcon(String icon);
    void setModes(const uint8_t modes);
    void setUnit(const String unit);
    void setDeviceClass(const String dc);
    void setStateClass(const String sc);

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
