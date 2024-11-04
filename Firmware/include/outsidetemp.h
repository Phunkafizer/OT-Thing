#ifndef _outsidetemp_h
#define _outsidetemp_h

#include <ArduinoJson.h>
#include <AsyncHTTPRequest_Generic.hpp>

class OutsideTemp {
private:
    int source;
    //AsyncHTTPRequest httpcli;
public:
    OutsideTemp();
    void loop();
    void setConfig(JsonObject &obj);
};

extern OutsideTemp outsideTemp;
#endif