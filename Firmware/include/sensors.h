#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <NimBLEDevice.h>
#include "util.h"

class AddressableSensor {
friend class Sensor;
friend class SensorLock;
private:
    uint8_t adrLen;
    static SemaphoreHandle_t mutex;
protected:
    AddressableSensor(const uint8_t *adr, const uint8_t adrLen, AddressableSensor **prev);
    String getAdr() const;
    static AddressableSensor* find(String adr, AddressableSensor *last);
    static void writeJsonAll(JsonObject &status, AddressableSensor *last);
    virtual void writeJson(JsonVariant val) = 0;
    static bool sendDiscoveryAll(AddressableSensor *last);
    virtual bool sendDiscovery() = 0;
    uint8_t adr[8];
    AddressableSensor *next;
    double temp;
public:
    static void begin();
};

class BLESensor: public AddressableSensor {
private:
    void parse(std::string &data);
    static BLESensor *last;
    uint8_t rh;
    uint8_t bat;
    int8_t rssi;
protected:
    void writeJson(JsonVariant val) override;
    bool sendDiscovery() override;
public:
    BLESensor(const uint8_t *adr);
    static void begin();
    static void onDiscovery(const NimBLEAdvertisedDevice* dev);
    static void writeJsonAll(JsonObject &status);
    static BLESensor* find(const uint8_t *adr);
    static bool sendDiscoveryAll();

};

class OneWireNode: public AddressableSensor {
private:
    static OneWireNode *last;
protected:
    void writeJson(JsonVariant val) override;
    bool sendDiscovery() override;
public:
    OneWireNode(uint8_t *addr);
    static void begin();
    static OneWireNode* find(String adr);
    static void loop();
    static void writeJsonAll(JsonObject &status);
    static bool sendDiscoveryAll();
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
    explicit operator bool() const;
protected:
    Source src;
    double value;
    bool setFlag;
    virtual void loop() {}
private:
    static Sensor *lastSensor;
    Sensor *prevSensor;
    uint8_t adr[6];
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

extern Sensor roomTemp[2];
extern AutoSensor roomSetPoint[2];
extern OutsideTemp outsideTemp;

