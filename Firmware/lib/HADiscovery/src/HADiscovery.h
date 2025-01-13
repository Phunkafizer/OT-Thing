#pragma once
#include <ArduinoJson.h>

extern const char *HA_DEVICE_CLASS_RUNNING PROGMEM;
extern const char *HA_DEVICE_CLASS_PROBLEM PROGMEM;

class HADiscovery {
private:
    void init(String &name, String &id, String component);
public:
    HADiscovery();
    JsonDocument doc;
    String topic;
    const char *devName;
    const char *manufacturer;
    String devPrefix;
    String defaultStateTopic;
    virtual void publish(String topic, JsonDocument &doc) {}
    void setValueTemplate(String valueTemplate);
    void setStateTopic(String &stateTopic);
    void setMinMax(double min, double max, double step);
    void setMinMaxTemp(double min, double max, double step);
    void setTemperatureStateTopic(String topic);
    void setTemperatureStateTemplate(String stateTemplate);
    void setCurrentTemperatureTopic(String topic);
    void setCurrentTemperatureTemplate(String templ);

    void createTempSensor(String name, String id);
    void createPowerFactorSensor(String name, String id);
    void createPressureSensor(String name, String id);
    void createHourDuration(String name, String id);
    void createSensor(String name, String id);
    void createBinarySensor(String name, String id, String deviceClass);
    void createNumber(String name, String id, String cmdTopic);
    void createClima(String name, String id, String tmpCmdTopic);
};
