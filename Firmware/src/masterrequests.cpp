#include "masterrequests.h"
#include "otcontrol.h"

OTWriteRequest::OTWriteRequest(OpenThermMessageID id, uint16_t intervalS):
    interval(intervalS),
        id(id) {
}

void OTWriteRequest::send(const uint16_t data) {
    nextMillis = millis() + interval * 1000;

    unsigned long req = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, id, data);
    otcontrol.sendRequest('T', req);
}

void OTWriteRequest::sendFloat(const double f) {
    int16_t td;
    if (f > 100)
        td = 100 << 8;
    else if (f < -100)
        td = - (int) (100 << 8);
    else
        td = (int) (f * 256);

    send(td);
}

void OTWriteRequest::force() {
    nextMillis = 0;
}

OTWriteRequest::operator bool() {
    if (interval == 0)
        return (nextMillis == 0);
        
    return (millis() > nextMillis);
}


OTWRSetDhw::OTWRSetDhw():
        OTWriteRequest(OpenThermMessageID::TdhwSet, 30) {
}

OTWRSetBoilerTemp::OTWRSetBoilerTemp(const uint8_t ch):
        OTWriteRequest(OpenThermMessageID::TSet, 10) {
    if (ch == 1)
        id = OpenThermMessageID::TsetCH2;
}

OTWRMasterConfigMember::OTWRMasterConfigMember():
        OTWriteRequest(OpenThermMessageID::MConfigMMemberIDcode, 60) {
}

OTWRSetVentSetpoint::OTWRSetVentSetpoint():
        OTWriteRequest(OpenThermMessageID::Vset, 60) {
}

OTWRSetRoomTemp::OTWRSetRoomTemp(const uint8_t ch):
        OTWriteRequest((ch == 0) ? OpenThermMessageID::Tr : OpenThermMessageID::TrCH2, 60) {
}

OTWRSetRoomSetPoint::OTWRSetRoomSetPoint(const uint8_t ch):
        OTWriteRequest((ch == 0) ? OpenThermMessageID::TrSet : OpenThermMessageID::TrSetCH2, 60) {
}

OTWRSetOutsideTemp::OTWRSetOutsideTemp():
        OTWriteRequest(OpenThermMessageID::Toutside, 60) {
}

OTWRSetMaxModulation::OTWRSetMaxModulation():
        OTWriteRequest(OpenThermMessageID::MaxRelModLevelSetting, 60) {
}


OTWRProdVersion::OTWRProdVersion():
        OTWriteRequest(OpenThermMessageID::MasterVersion, 0) {
}

OTWRSetOTVersion::OTWRSetOTVersion():
        OTWriteRequest(OpenThermMessageID::OpenThermVersionMaster, 0) {
}

