#ifndef _outsidetemp_h
#define _outsidetemp_h

#include <ArduinoJson.h>
#include <WiFiClient.h>

class OutsideTemp {
private:
    double lat, lon;
    String apikey;
    WiFiClient cli;
    String replyBuf;
    unsigned long nextMillis;
    enum {
        HTTP_IDLE,
        HTTP_CONNECTING,
        HTTP_RECEIVING
    } httpState;
    double value;
    bool available;
    enum OutsideTempSource {
        OUTSIDETEMP_MQTT = 0,
        OUTSIDETEMP_OPENWEATHER = 1,
        OUTSIDETEMP_BLUETOOTH = 2
    } source;
    unsigned int interval;
public:
    OutsideTemp();
    void loop();
    void setConfig(JsonObject &obj);
    bool get(double &value);
    void setFromMqtt(const double t);
};

extern OutsideTemp outsideTemp;
#endif