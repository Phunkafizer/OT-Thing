#pragma once
#include "HADiscovery.h"

class OTThingHADiscovery: public HADiscovery {
public:
    OTThingHADiscovery();
    void begin();
    bool publish();
};

extern OTThingHADiscovery haDisc;