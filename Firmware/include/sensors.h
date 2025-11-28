#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>

class OneWireNode {
private:
    String getAdr() const;
    OneWireNode *next;
    uint8_t addr[8];
    double temp;
public:
    OneWireNode(uint8_t *addr);
    static void begin();
    static void loop();
    static void writeJson(JsonObject &status);
    static OneWireNode* find(String adr);
    static bool sendDiscovery();
};

class Sensor {
public:
    enum Source: int8_t {
        SOURCE_NA = -1,
        SOURCE_MQTT = 0,
        SOURCE_OT = 1,
        SOURCE_BLE = 2,
        SOURCE_1WIRE = 3,
        SOURCE_OPENWEATHER = 4,
        SOURCE_AUTO = 5 // has to be last item in this list!
    };
    OneWireNode *own; // points to a OneWireNode if configured
    Sensor();
    virtual void set(const double val, const Source src);
    bool get(double &val);
    virtual void setConfig(JsonObject &obj);
    bool isMqttSource();
    bool isOtSource();
    static void loopAll();
protected:
    Source src;
    double value;
    bool setFlag;
    virtual void loop() {}
private:
    static Sensor *lastSensor;
    Sensor *prevSensor;
};

class AutoSensor: public Sensor {
public:
    AutoSensor();
    void set(const double val, const Source src);
private:
    double values[SOURCE_AUTO + 1];
};

class OutsideTemp: public Sensor {
public:
    void setConfig(JsonObject &obj);
    OutsideTemp();
    String owResult;
protected:
    void loop();
private:
    uint32_t nextMillis;
    uint32_t interval;
    double lat, lon;
    String apikey;
    AsyncClient acli;
    String replyBuf;
    enum {
        HTTP_IDLE,
        HTTP_CONNECTING,
        HTTP_RECEIVING
    } httpState;
};

extern OneWireNode *oneWireNode;
extern Sensor roomTemp[2];
extern AutoSensor roomSetPoint[2];
extern OutsideTemp outsideTemp;

