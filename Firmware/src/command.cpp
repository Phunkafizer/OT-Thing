#include "command.h"
#include "portal.h"

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

    // socket://192.168.178.35:25238
    String st(s);

    if (st.substring(0, 4) == "PS=0") {
        sendAll("PS:0");
        enableOtEvents = true;
    }

    if (st.substring(0, 4) == "PS=1") {
        enableOtEvents = false;
        sendAll("PS: 1");
        String rep;
        rep += "11111111/11111111, "; // Status (MsgID=0) — Printed as two 8-bit bit fields
        rep += "43.2, "; // Control setpoint (MsgID=1) — Printed as a floating point value
        rep += "00000000/00000000, "; // Remote parameter flags (MsgID=6) — Printed as two 8-bit bit fields
        rep += "0.0, "; // Cooling control signal (MsgID=7) — Printed as a floating point value
        rep += "0.0, "; // Control setpoint 2 (MsgID=8) — Printed as a floating point value
        rep += "0.0, "; // Maximum relative modulation level (MsgID=14) — Printed as a floating point value
        rep += "20/40, "; // Boiler capacity and modulation limits (MsgID=15) — Printed as two bytes
        rep += "22.2, "; // Room setpoint (MsgID=16) — Printed as a floating point value
        rep += "11.1, "; // Relative modulation level (MsgID=17) — Printed as a floating point value
        rep += "1.21, "; // CH water pressure (MsgID=18) — Printed as a floating point value
        rep += "0.75, "; // DHW flow rate (MsgID=19) — Printed as floating point value
        rep += "23.4, "; // CH2 room setpoint (MsgID=23) — Printed as a floating point value
        rep += "21.0, "; // Room temperature (MsgID=24) — Printed as a floating point value
        rep += "53.2, "; // Boiler water temperature (MsgID=25) — Printed as a floating point value
        rep += "44.4, "; // DHW temperature (MsgID=26) — Printed as a floating point value
        rep += "1.2, "; // Outside temperature (MsgID=27) — Printed as a floating point value
        rep += "35.5, "; // Return water temperature (MsgID=28) — Printed as a floating point value
        rep += "58.8, "; // CH2 flow temperature (MsgID=31) — Printed as a floating point value
        rep += "98, "; // Boiler exhaust temperature (MsgID=33) — Printed as a signed decimal value
        rep += "30/60, "; // DHW setpoint boundaries (MsgID=48) — Printed as two bytes
        rep += "30/65, "; // Max CH setpoint boundaries (MsgID=49) — Printed as two bytes
        rep += "50.5, "; // DHW setpoint (MsgID=56) — Printed as a floating point value
        rep += "68.0, "; // Max CH water setpoint (MsgID=57) — Printed as a floating point value
        rep += "00000000/00000000, "; //V/H master status (MsgID=70) — Printed as two 8-bit bit fields
        rep += "0, "; // V/H control setpoint (MsgID=71) — Printed as a byte
        rep += "15, "; // Relative ventilation (MsgID=77) — Printed as a byte
        rep += "12345, "; //Burner starts (MsgID=116) — Printed as a decimal value
        rep += "23456, "; // CH pump starts (MsgID=117) — Printed as a decimal value
        rep += "11223, "; // DHW pump/valve starts (MsgID=118) — Printed as a decimal value
        rep += "22114, "; // DHW burner starts (MsgID=119) — Printed as a decimal value
        rep += "33411, "; // Burner operation hours (MsgID=120) — Printed as a decimal value
        rep += "12121, "; // CH pump operation hours (MsgID=121) — Printed as a decimal value
        rep += "44321, "; // DHW pump/valve operation hours (MsgID=122) — Printed as a decimal value
        rep += "54321"; // DHW burner operation hours (MsgID=123) — Printed as a decimal value 
        sendAll(rep);
    }

    if (st.substring(0, 4) == "PR=A") {
        sendAll("PR:A=OpenTherm Gateway 1.37");
    }

    if (st.substring(0, 4) == "PR=B") {
        sendAll("PR:B=25.7.2015");
    }

    if (st.substring(0, 4) == "PR=C") {
        sendAll("PR:C=4");
    }

    if (st.substring(0, 4) == "PR=W") {
        sendAll("PR:W=40.8");
    }

    if (st.substring(0, 4) == "PR=G") {
        sendAll("PR:G=00");
    }

    if (st.substring(0, 4) == "PR=I") {
        sendAll("PR:I=00");
    }

    if (st.substring(0, 4) == "PR=T") {
        sendAll("PR:T=00");
    }

    if (st.substring(0, 4) == "PR=L") {
        sendAll("PR:L=000000");
    }

    if (st.substring(0, 4) == "PR=M") {
        sendAll("PR:M=G");
    }

    if (st.substring(0, 4) == "PR=Q") {
        sendAll("PR:Q=P");
    }

    if (st.substring(0, 4) == "PR=S") {
        sendAll("PR:S=30.5");
    }

    if (st.substring(0, 4) == "PR=O") {
        sendAll("PR:O=32.5");
    }

    if (st.substring(0, 4) == "PR=P") {
        sendAll("PR:P=low");
    }

    if (st.substring(0, 4) == "PR=D") {
        sendAll("PR:D=R");
    }

    if (st.substring(0, 4) == "PR=R") {
        sendAll("PR:R=0");
    }

    if (st.substring(0, 4) == "PR=V") {
        sendAll("PR:V=20");
    }
}

void OtGwCommand::onClientDisconnect(void* arg, AsyncClient* client) {
}

void OtGwCommand::begin() {
    server.begin();
}

void OtGwCommand::sendAll(String s) {
    for (auto client: clients) {
        client->write((s + "\r\n").c_str());
    }
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
    
    portal.textAll(line);
}

void OtGwCommand::loop() {
}




