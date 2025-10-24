#include "provisioning.h"
#include "wifi_manager.h"
#include "config.h"
#include <Preferences.h>

AsyncWebServer server(80);

void startProvisioning() {
  Serial.println("\n[Provisioning Mode]");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html",
      "<h2>Synkro WiFi Setup</h2>"
      "<form action='/wifi' method='POST'>"
      "SSID: <input name='ssid'><br>"
      "Password: <input name='pass' type='password'><br><br>"
      "<button type='submit'>Save</button></form>");
  });

  server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (req->hasParam("ssid", true) && req->hasParam("pass", true)) {
      String newSsid = req->getParam("ssid", true)->value();
      String newPass = req->getParam("pass", true)->value();
      Preferences prefs;
      prefs.begin("wifi", false);
      prefs.putString("ssid", newSsid);
      prefs.putString("pass", newPass);
      prefs.end();
      req->send(200, "text/plain", "Saved! Rebooting...");
      delay(500);
      ESP.restart();
    } else {
      req->send(400, "text/plain", "Missing SSID or password");
    }
  });

  server.begin();
}
