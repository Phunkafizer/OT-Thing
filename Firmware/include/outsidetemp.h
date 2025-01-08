#ifndef _outsidetemp_h
#define _outsidetemp_h

#include <ArduinoJson.h>
#include <WiFiClient.h>

class OutsideTemp {
private:
    int source;
    double lat, lon;
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
};

extern OutsideTemp outsideTemp;
#endif