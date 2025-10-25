#include "wifi_manager.h"

static Preferences prefs;
static AsyncWebServer server(80);
static bool provisioning = false;

void WiFiManager::connectOrProvision(const char* apSsid, const char* apPass, const char* hostname) {
  prefs.begin("wifi", false);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  if (ssid.isEmpty() || pass.isEmpty()) {
    Serial.println("[WiFi] No saved credentials → provisioning");
    startProvisioning(apSsid, apPass);
    provisioning = true;
    return;
  }

  Serial.printf("[WiFi] Connecting to %s ...\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  if (hostname && strlen(hostname)) {
    // set hostname before begin on some cores
    WiFi.setHostname(hostname);
  }
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] ✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] ❌ Failed. Switching to provisioning mode.");
    startProvisioning(apSsid, apPass);
    provisioning = true;
  }
}

bool WiFiManager::isConnected() {
  return !provisioning && WiFi.status() == WL_CONNECTED;
}

void WiFiManager::startProvisioning(const char* apSsid, const char* apPass) {
  Serial.println("\n[Provisioning Mode]");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, apPass);

  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

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
      Serial.println("[WiFi] Credentials saved. Rebooting...");
      delay(800);
      ESP.restart();
    } else {
      req->send(400, "text/plain", "Missing SSID or password");
    }
  });

  server.begin();

  Serial.println("-------------------------------------------------------------");
  Serial.printf("🔗 Connect to Wi-Fi: %s\n", apSsid);
  Serial.printf("🔑 Password:        %s\n", apPass);
  Serial.printf("🌐 Open:            http://%s/\n", WiFi.softAPIP().toString().c_str());
  Serial.println("-------------------------------------------------------------");
}
