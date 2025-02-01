#ifndef _outsidetemp_h
#define _outsidetemp_h

#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>

class OutsideTemp {
public:
    enum Source {
        SOURCE_MQTT = 0,
        SOURCE_OPENWEATHER = 1,
        SOURCE_BLUETOOTH = 2,
        SOURCE_OPENTHERM = 3
    };
private:
    Source source;
    double lat, lon;
    String apikey;
    WiFiClient cli;
    AsyncClient acli;
    String replyBuf;
    unsigned long nextMillis;
    enum {
        HTTP_IDLE,
        HTTP_CONNECTING,
        HTTP_RECEIVING
    } httpState;
    double value;
    bool available;
    unsigned int interval;
public:
    OutsideTemp();
    void loop();
    void setConfig(JsonObject &obj);
    bool get(double &value);
    void set(const double t, const Source source);
};

extern OutsideTemp outsideTemp;
#endif