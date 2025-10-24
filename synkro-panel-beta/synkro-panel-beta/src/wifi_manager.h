#pragma once
#include <WiFi.h>
#include <Preferences.h>

extern String ssid;
extern String pass;
extern bool needProvisioning;

void connectToWiFi();
