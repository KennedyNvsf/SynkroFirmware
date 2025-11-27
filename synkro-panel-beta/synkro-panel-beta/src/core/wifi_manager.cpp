// src/core/wifi_manager.cpp
#include "wifi_manager.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <esp_system.h>

// --- statics (module-private) ---
static AsyncWebServer sServer(80);
static Preferences sPrefs;
static bool sProvisioning = false;
static String sSsid;
static String sPass;
static String sApSsid;
static String sApPass;
static String sDeviceId;

// ---------- internal helpers ----------

static void startProvisioningInternal() {
    Serial.println("\n[Provisioning Mode]");
    sProvisioning = true;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(sApSsid.c_str(), sApPass.c_str());
    IPAddress apIp = WiFi.softAPIP();
    Serial.print("AP IP: ");
    Serial.println(apIp);

    sServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(
            200,
            "text/html",
            "<h2>Synkro WiFi Setup</h2>"
            "<form action='/wifi' method='POST'>"
            "SSID: <input name='ssid'><br>"
            "Password (leave empty if open network): "
            "<input name='pass' type='password'><br><br>"
            "<button type='submit'>Save</button></form>"
        );
    });

    sServer.on("/wifi", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (req->hasParam("ssid", true) && req->hasParam("pass", true)) {
            String newSsid = req->getParam("ssid", true)->value();
            String newPass = req->getParam("pass", true)->value();

            // Optional: trim spaces
            newSsid.trim();
            newPass.trim();

            sPrefs.begin("wifi", false);
            sPrefs.putString("ssid", newSsid);
            sPrefs.putString("pass", newPass);
            sPrefs.end();

            req->send(200, "text/plain", "Saved! Rebooting...");
            Serial.println("[WiFi] Credentials received, rebooting...");
            Serial.print("  SSID: '"); Serial.print(newSsid); Serial.println("'");
            Serial.print("  PASS: '"); Serial.print(newPass); Serial.println("'");
            delay(800);
            ESP.restart();
        } else {
            req->send(400, "text/plain", "Missing SSID or password");
        }
    });

    sServer.begin();

    Serial.println();
    Serial.println("-------------------------------------------------------------");
    Serial.println("🔗 Connect to Wi-Fi: " + sApSsid);
    Serial.println("🔑 Password: " + sApPass);
    Serial.println("🌐 Open: http://" + apIp.toString() + "/");
    Serial.println("-------------------------------------------------------------");
}

static void connectStaInternal() {
    Serial.print("[WiFi] Connecting to ");
    Serial.println(sSsid);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(sDeviceId.c_str());

    // 🔹 OPEN NETWORK SUPPORT
    if (sPass.isEmpty()) {
        Serial.println("[WiFi] Open network detected → connecting without password");
        WiFi.begin(sSsid.c_str());
    } else {
        WiFi.begin(sSsid.c_str(), sPass.c_str());
    }

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000UL) {
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        sProvisioning = false;
    } else {
        Serial.println("\n[WiFi] Failed to connect. Switching to provisioning mode.");
        startProvisioningInternal();
    }
}

// ---------- public API ----------

void wifi_portal::begin(const char* deviceId, const char* apSsid, const char* apPass) {
    sDeviceId = deviceId;
    sApSsid = apSsid;
    sApPass = apPass;

    // Check reset reason — clear creds on physical reset
    esp_reset_reason_t reason = esp_reset_reason();

    sPrefs.begin("wifi", false);
    if (reason == ESP_RST_EXT) {
        Serial.println("[RESET] Physical reset detected → clearing Wi-Fi credentials...");
        sPrefs.clear();
        sPrefs.end();
        delay(400);
        startProvisioningInternal();
        return;
    }

    sSsid = sPrefs.getString("ssid", "");
    sPass = sPrefs.getString("pass", "");
    sPrefs.end();

    Serial.print("[WiFi] Loaded SSID: '"); Serial.print(sSsid); Serial.println("'");
    Serial.print("[WiFi] Loaded PASS length: "); Serial.println(sPass.length());

    // 🔹 ONLY treat missing SSID as "no credentials"
    if (sSsid.isEmpty()) {
        Serial.println("[WiFi] No saved Wi-Fi SSID → provisioning mode.");
        startProvisioningInternal();
    } else {
        connectStaInternal();
    }
}

bool wifi_portal::isProvisioning() {
    return sProvisioning;
}

bool wifi_portal::isConnected() {
    return (!sProvisioning) && (WiFi.status() == WL_CONNECTED);
}

void wifi_portal::loop() {
    // Currently nothing to do; AsyncWebServer is event-based.
    // Kept for symmetry / future reconnect logic.
}