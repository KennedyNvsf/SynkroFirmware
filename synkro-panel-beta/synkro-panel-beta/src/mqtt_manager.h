#pragma once
#include <PubSubClient.h>

extern PubSubClient mqtt;

void mqttLoop();
void connectToMQTT();
void reportState();
void sendDiscovery();
void mqttCallback(char* topic, byte* payload, unsigned int length);
