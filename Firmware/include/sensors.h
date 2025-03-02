#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>

class Sensor {
public:
    enum Source: int8_t {
        SOURCE_NA = -1,
        SOURCE_MQTT = 0,
        SOURCE_OT = 1,
        SOURCE_BLE = 2,
        SOURCE_1WIRE = 3,
        SOURCE_OPENWEATHER = 4
    };
    Sensor();
    void set(const double val, const Source src);
    bool get(double &val);
    virtual void setConfig(JsonObject &obj);
    bool isMqttSource();
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

class OutsideTemp: public Sensor {
public:
    void setConfig(JsonObject &obj);
    OutsideTemp();
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

extern Sensor roomTemp[2];
extern Sensor roomSetPoint[2];
extern OutsideTemp outsideTemp;

