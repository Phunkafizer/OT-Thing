#include "mqtt.h"
//#include <ESP8266WiFi.h>
#include <WiFi.h>
#include <devstatus.h>

Mqtt mqtt;
static WiFiClient espClient;


static void mqttCb(char* topic, byte* payload, unsigned int length) {

}

Mqtt::Mqtt():
    lastConTry(0),
    lastStatus(0),
    configSet(false) {
    cli.setBufferSize(2048);
}

void Mqtt::begin() {
}

bool Mqtt::connected() {
    return cli.connected();
}

void Mqtt::setConfig(const MqttConfig conf) {
    config = conf;
    configSet = true;
    lastConTry = -10000;
    if (cli.connected())
        cli.disconnect();
}

void Mqtt::loop() {
    if (!cli.connected() && ((millis() - lastConTry) > 10000) && WiFi.isConnected() && configSet) {
        lastConTry = millis();

        cli.setClient(espClient);
        cli.setServer(config.host.c_str(), config.port);
        cli.setCallback(mqttCb);
    
        String conId = "OTThing-" + WiFi.macAddress();
        cli.connect(conId.c_str(), "", "");
        //mqtt.subscribe("home/test");
    }

    if (cli.connected() && ((millis() - lastStatus) > 5000)) {
        lastStatus = millis();
        cli.publish("home/OTThing", devstatus.getJson().c_str());
    }
    cli.loop();
}