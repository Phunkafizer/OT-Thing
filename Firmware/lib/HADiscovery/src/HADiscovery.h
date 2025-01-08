#pragma once
#include <ArduinoJson.h>

extern const char *HA_DEVICE_CLASS_RUNNING PROGMEM;
extern const char *HA_DEVICE_CLASS_PROBLEM PROGMEM;

class HADiscovery {
private:
    void init(String &name, String &id);
public:
    HADiscovery();
    JsonDocument doc;
    String topic;
    const char *devName;
    const char *manufacturer;
    String devPrefix;
    String stateTopic;
    virtual void publish(String topic, JsonDocument &doc) {}
    void setValueTemplate(String valueTemplate);

    void createTempSensor(String name, String id);
    void createPowerFactorSensor(String name, String id);
    void createPressureSensor(String name, String id);
    void createHourDuration(String name, String id);
    void createSensor(String name, String id);
    void createBinarySensor(String name, String id, String deviceClass);
};
