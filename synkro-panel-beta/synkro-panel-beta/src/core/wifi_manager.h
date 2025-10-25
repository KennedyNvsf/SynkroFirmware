#pragma once
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

class WiFiManager {
public:
  static void connectOrProvision(const char* apSsid, const char* apPass, const char* hostname);
  static bool isConnected();

private:
  static void startProvisioning(const char* apSsid, const char* apPass);
};
