#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "io_manager.h"
#include "provisioning.h"

unsigned long lastReport = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[Boot] Synkro FireBeetle starting...");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, LOW);

  Preferences prefs;
  prefs.begin("wifi", false);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();

  if (ssid.isEmpty() || pass.isEmpty()) {
    startProvisioning();
    return;
  }

  connectToWiFi();
  if (WiFi.status() != WL_CONNECTED) startProvisioning();
  else connectToMQTT();
}

void loop() {
  if (!needProvisioning) {
    mqttLoop();
    handleButton();
    if (millis() - lastReport > REPORT_INTERVAL) {
      lastReport = millis();
      reportState();
      sendDiscovery();
    }
  }
}
