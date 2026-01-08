#pragma once
#include <NimBLEDevice.h>

#ifdef DEBUG
    extern NimBLECharacteristic *bleSerialTx;
    extern volatile bool bleClientConnected;
#endif