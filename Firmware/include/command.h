#ifndef _command_h
#define _command_h

#include <Arduino.h>
#ifdef ESP32
#include <AsyncTCP.h>
#endif
#ifdef ESP8266
#include <ESPAsyncTCP.h>
#endif
#include <vector>



extern class OtGwCommand {
private:
    bool enableOtEvents;
    AsyncServer server;
    std::vector<AsyncClient*> clients;
    friend void handleNewClient(void* arg, AsyncClient* client);
    friend void handleClientData(void* arg, AsyncClient* client, void *data, size_t len);
    friend void handleClientDisconnect(void* arg, AsyncClient* client);
    void onNewClient(void* arg, AsyncClient* client);
    void onClientData(void* arg, AsyncClient* client, void *data, size_t len);
    void onClientDisconnect(void* arg, AsyncClient* client);
public:
    OtGwCommand();
    void begin();
    void loop();
    void sendAll(String s);
    void sendOtEvent(const char source, const uint32_t data);
    
} command;



#endif