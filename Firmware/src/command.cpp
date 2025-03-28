#include "command.h"
#include "portal.h"
#include "otvalues.h"

OtGwCommand command;

void handleNewClient(void* arg, AsyncClient* client) {
    command.onNewClient(arg, client);
}

void handleClientData(void* arg, AsyncClient* client, void *data, size_t len) {
    command.onClientData(arg, client, data, len);
}

void handleClientDisconnect(void* arg, AsyncClient* client) {
    command.onClientDisconnect(arg, client);
}

OtGwCommand::OtGwCommand():
        enableOtEvents(true),
        server(25238) {
    server.onClient(&handleNewClient, &server);
}

void OtGwCommand::onNewClient(void* arg, AsyncClient* client) {
    clients.push_back(client);
    client->onData(&handleClientData, NULL);
    client->onDisconnect(&handleClientDisconnect, NULL);
}

void OtGwCommand::onClientData(void* arg, AsyncClient* client, void *data, size_t len) {
    char *s = (char*) data;
    s[len - 1] = 0;
    Serial.println(s);
}

void OtGwCommand::onClientDisconnect(void* arg, AsyncClient* client) {
}

void OtGwCommand::begin() {
    server.begin();
}

void OtGwCommand::sendAll(String s) {
    for (auto client: clients)
        client->write((s + "\r\n").c_str());
    Serial.println(s);
}

void OtGwCommand::sendOtEvent(const char source, const uint32_t data) {
    String line(source);
    int pos = 28;
    while (pos > 0) {
        pos -= 4;
        if (((data>>pos) & 0xF0) != 0)
            break;

        line += '0';
    }
    line += String(data, HEX);

    if (enableOtEvents)
        sendAll(line);

    auto mt = OpenTherm::getMessageType(data);
    auto id = OpenTherm::getDataID(data);

    line += ' ';
    switch (mt) {
    case OpenThermMessageType::READ_DATA:
        line += F("READ");
        break;
    case OpenThermMessageType::WRITE_DATA:
        line += F("WRITE");
        break;
    case OpenThermMessageType::INVALID_DATA:
        line += F("INVALID_DATA");
        break;
    case OpenThermMessageType::READ_ACK:
        line += F("READ_ACK");
        break;
    case OpenThermMessageType::WRITE_ACK:
        line += F("WRITE_ACK");
        break;
    case OpenThermMessageType::DATA_INVALID:
        line += F("DATA_INVALID");
        break;
    case OpenThermMessageType::UNKNOWN_DATA_ID:
        line += F("UKNOWN ID");
        break;
    default:
        break;
    }

    const char *name = OTItem::getName(id);
    line += ' ';
    if (name != nullptr)
        line += FPSTR(name);
    else {
        line += F("ID ");
        line += String((int) id);
    }
    line += F(" data 0x");
    line += String(data & 0xFFFF, HEX);
    portal.textAll(line);
}

void OtGwCommand::loop() {
}