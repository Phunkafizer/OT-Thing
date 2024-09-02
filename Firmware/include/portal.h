#ifndef _portal_h
#define _portal_h

#include <Arduino.h>

class Portal {
private:
    String confBuf;
    bool reboot;
    bool updateEnable;
public:
    Portal();
    void begin(bool configMode);
    void loop();
};

extern Portal portal;
#endif