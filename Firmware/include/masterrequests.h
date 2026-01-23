#pragma once

#include <Arduino.h>
#include <OpenTherm.h>

class OTWriteRequest {
private:
    uint32_t nextMillis {0};
    uint16_t interval;
protected:
    OpenThermMessageID id;
public:
    OTWriteRequest(OpenThermMessageID id, uint16_t intervalS);
    void send(const uint16_t data);
    void sendFloat(const double f);
    void force();
    operator bool();
};

class OTWRSetDhw: public OTWriteRequest {
public:
    OTWRSetDhw();
};

class OTWRSetBoilerTemp: public OTWriteRequest {
public:
    OTWRSetBoilerTemp(const uint8_t ch);
};

class OTWRMasterConfigMember: public OTWriteRequest {
public:
    OTWRMasterConfigMember();
};

class OTWRSetVentSetpoint: public OTWriteRequest {
public:
    OTWRSetVentSetpoint();
};

class OTWRSetRoomTemp: public OTWriteRequest {
public:
    OTWRSetRoomTemp(const uint8_t ch);
};

class OTWRSetRoomSetPoint: public OTWriteRequest {
public:
    OTWRSetRoomSetPoint(const uint8_t ch);
};

class OTWRSetOutsideTemp: public OTWriteRequest {
public:
    OTWRSetOutsideTemp();
};

class OTWRSetMaxModulation: public OTWriteRequest {
public:
    OTWRSetMaxModulation();
};

class OTWRProdVersion: public OTWriteRequest {
public:
    OTWRProdVersion();
};

class OTWRSetOTVersion: public OTWriteRequest {
public:
    OTWRSetOTVersion();
};