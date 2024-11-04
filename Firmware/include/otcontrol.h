#ifndef _otcontrol_h_
#define _otcontrol_h_

#include <OpenTherm.h>
#include "ArduinoJson.h"
//#include <InfluxDbClient.h>
//#include <InfluxDbCloud.h>

class OTValue {
private:
    const OpenThermMessageID id;
    unsigned long lastTransfer;
    const unsigned int interval;
    virtual void getValue(JsonObject &stat) const = 0;
protected:
    const char *name;
    uint16_t value;
    OpenThermResponseStatus lastStatus;
public:
    OTValue(const char *name, const OpenThermMessageID id, const unsigned int interval);
    bool process();
    OpenThermMessageID getId() const;
    void setValue(uint16_t val);
    const char *getName() const;
    void setStatus(OpenThermResponseStatus status);
    void getJson(JsonObject &obj) const;
};

class OTValueu16: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:
    OTValueu16(const char *name, const OpenThermMessageID id, const unsigned int interval);
    uint16_t getValue() const;
};

class OTValuei16: public OTValue {
private:
    void getValue(JsonObject &stat) const;
public:
    OTValuei16(const char *name, const OpenThermMessageID id, const unsigned int interval);
    int16_t getValue() const;
    
};

class OTValueFloat: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:
    OTValueFloat(const char *name, const OpenThermMessageID id, const unsigned int interval);
    double getValue() const;
};

class OTValueStatus: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:    
    OTValueStatus();
};

class OTValueSlaveConfigMember: public OTValue {
private:
    void getValue(JsonObject &obj) const;
public:    
    OTValueSlaveConfigMember();
};

struct ChControlConfig {
    double flowMax; // max. flow temperature
    double exponent; // heating exponent
    double gradient; // gradient
    double offset; // shift
};

class OTControl {
    friend void otCbSlave(unsigned long response, OpenThermResponseStatus status);
    friend void otCbMaster(unsigned long response, OpenThermResponseStatus status);
private:
    void OnRxMaster(const unsigned long msg, const OpenThermResponseStatus status);
    void OnRxSlave(const unsigned long msg, const OpenThermResponseStatus status);
    unsigned long lastMillis;
    enum OTMode {
        OTMODE_MASTER,
        OTMODE_GATEWAY,
        OTMODE_LOOPBACK
    } otMode;
    uint32_t loopbackData; // testdata for loopback test
public:
    OTControl();
    void begin();
    void loop();
    void getJson(JsonObject &obj) const;
    void setChCtrlConfig(ChControlConfig &config);
    
    
};

extern OTControl otcontrol;
#endif