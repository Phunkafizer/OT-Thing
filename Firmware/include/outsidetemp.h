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
public:
    OutsideTemp();
    void loop();
    void setConfig(JsonObject &obj);
    double temp;
    enum OutsideTempSource {
        OUTSIDETEMP_OPENWEATHER,
        OUTSIDETEMP_MQTT
    } source;
};

extern OutsideTemp outsideTemp;
#endif