#pragma once
#include "HADiscovery.h"

class OTThingHADiscovery: public HADiscovery {
public:
    OTThingHADiscovery();
    bool publish();
};

extern OTThingHADiscovery haDisc;