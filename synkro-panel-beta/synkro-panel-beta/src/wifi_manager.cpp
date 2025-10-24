#include "wifi_manager.h"
#include "config.h"

Preferences prefs;
bool needProvisioning = false;
String ssid, pass;

void connectToWiFi() {
  Serial.printf("[WiFi] Connecting to %s\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_ID);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Failed → provisioning mode");
    needProvisioning = true;
  }
}
