#ifndef _portal_h
#define _portal_h

#include <Arduino.h>

class Portal {
private:
    String confBuf;
    bool reboot;
    bool updateEnable;
    bool checkUpdate {false};
    bool doUpdate {false};
public:
    Portal();
    void begin(bool configMode);
    void loop();
    void textAll(String text);
};

extern Portal portal;
#endif