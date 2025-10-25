#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <esp_system.h>

#include "config.h"
#include "core/mqtt_manager.h"
#include "core/io_manager.h"

// ---------- Globals ----------
Preferences prefs;
AsyncWebServer server(80);

// Wi-Fi creds
String ssid, pass;
bool needProvisioning = false;

// Managers
WiFiClient      netClient;
MqttManager     mqttMgr(netClient, DEVICE_ID, DEVICE_NAME);
IoManager       ioMgr(RELAY_PIN, BUTTON_PIN);

// ---------- Provisioning ----------
static void startProvisioning() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "text/html",
      "<h2>Synkro WiFi Setup</h2>"
      "<form action='/wifi' method='POST'>"
      "SSID: <input name='ssid'><br>"
      "Password: <input name='pass' type='password'><br><br>"
      "<button type='submit'>Save</button></form>");
  });

  server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *req){
    if (req->hasParam("ssid", true) && req->hasParam("pass", true)) {
      String newSsid = req->getParam("ssid", true)->value();
      String newPass = req->getParam("pass", true)->value();
      prefs.begin("wifi", false);
      prefs.putString("ssid", newSsid);
      prefs.putString("pass", newPass);
      prefs.end();
      req->send(200, "text/plain", "Saved! Rebooting...");
      delay(800);
      ESP.restart();
    } else {
      req->send(400, "text/plain", "Missing SSID or password");
    }
  });

  server.begin();
}

static void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_ID);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    needProvisioning = true;
  }
}

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  delay(600);

  ioMgr.begin();

  prefs.begin("wifi", false);
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_EXT) {
    prefs.clear();
    prefs.end();
    startProvisioning();
    return;
  }
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();

  if (ssid.isEmpty() || pass.isEmpty()) {
    startProvisioning();
    return;
  }

  connectToWiFi();
  if (needProvisioning || WiFi.status() != WL_CONNECTED) {
    startProvisioning();
    return;
  }

  // MQTT manager
  mqttMgr.begin(BROKER_IP, BROKER_PORT);

  // NEW: wire web control -> GPIO
  mqttMgr.setLampCommandHandler([&](bool on){
    ioMgr.setLamp(on);                 // flip relay immediately
    mqttMgr.onLampChanged(ioMgr.lampOn()); // publish new state
  });
}

void loop() {
  const unsigned long now = millis();

  // Immediate local IO responsiveness (physical button)
  if (ioMgr.tick(now)) {
    mqttMgr.onLampChanged(ioMgr.lampOn()); // publish change if connected
  }

  // Non-blocking MQTT maintenance
  mqttMgr.tick(now);
}