// ================== Synkro ESP32 Firmware (FireBeetle 2 ESP32-E) ==================
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <esp_system.h>

// ------------------ USER CONFIG ------------------
#define AP_SSID      "Synkro_Setup"
#define AP_PASS      "12345678"

#define DEVICE_ID    "synkro_res_p_beta"
#define DEVICE_NAME  "IEFP_AUDITORIUM"

// Your Pi (Mosquitto) TCP broker for the ESP32 to connect
#define BROKER_IP    "192.168.1.90"
#define BROKER_PORT  1883

// Optional: your Pi’s mDNS hostname (without .local) used only for discovery hints
#ifndef BROKER_MDNS
  #define BROKER_MDNS "synkro-discovery"
#endif

// GPIOs
#define RELAY_PIN    25
#define BUTTON_PIN   26

// ------------------ GLOBALS ------------------
Preferences prefs;
AsyncWebServer server(80);
WiFiClient espClient;
PubSubClient mqtt(espClient);

String ssid, pass;
bool needProvisioning = false;

unsigned long lastReport = 0;
const unsigned long reportInterval = 10 * 1000UL; // report every 10s

bool lampState = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 300; // ms debounce

// Helpers
String wsUrlFromIp() {
  return "ws://" + String(BROKER_IP) + ":9001";
}
String mdnsHost() {
  return String(BROKER_MDNS).length() ? String(BROKER_MDNS) + ".local" : String("");
}

// ================== Provisioning (AP + form) ==================
void startProvisioning() {
  Serial.println("\n[Provisioning Mode]");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

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
      Serial.println("Credentials received, rebooting...");
      delay(800);
      ESP.restart();
    } else {
      req->send(400, "text/plain", "Missing SSID or password");
    }
  });

  server.begin();

  Serial.println();
  Serial.println("-------------------------------------------------------------");
  Serial.println("🔗 Connect to Wi-Fi: " + String(AP_SSID));
  Serial.println("🔑 Password:        " + String(AP_PASS));
  Serial.println("🌐 Open:            http://" + WiFi.softAPIP().toString() + "/");
  Serial.println("-------------------------------------------------------------");
}

// ================== Wi-Fi ==================
void connectToWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  // (Optional) set hostname before begin on some ESP32 cores
  WiFi.setHostname(DEVICE_ID);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000UL) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Failed to connect. Switching to provisioning mode.");
    needProvisioning = true;
  }
}

// ================== MQTT: State / Discovery / Control / LWT ==================
void reportState() {
  if (!mqtt.connected()) return;

  StaticJsonDocument<256> state;
  state["deviceId"] = DEVICE_ID;
  state["name"]     = DEVICE_NAME;
  state["uptime"]   = millis() / 1000;
  state["status"]   = "online";
  state["lamp"]     = lampState ? "on" : "off";
  state["ip"]       = WiFi.localIP().toString();
  state["brokerUrl"]= wsUrlFromIp();
  if (mdnsHost().length()) state["mdns"] = mdnsHost();

  String msg;
  serializeJson(state, msg);

  // Preferred per-device topic (used by the webapp)
  String topicPerDevice = String("synkro/devices/") + DEVICE_ID + "/state";
  mqtt.publish(topicPerDevice.c_str(), msg.c_str());

  // Legacy global topic (kept for compatibility)
  mqtt.publish("synkro/devices/state", msg.c_str());

  Serial.println("[MQTT] State reported: " + msg);
}

void sendDiscovery() {
  if (!mqtt.connected()) return;

  StaticJsonDocument<256> doc;
  doc["id"]        = DEVICE_ID;
  doc["name"]      = DEVICE_NAME;
  doc["ip"]        = WiFi.localIP().toString();
  doc["status"]    = "online";
  doc["brokerUrl"] = wsUrlFromIp();
  if (mdnsHost().length()) doc["mdns"] = mdnsHost();

  String msg;
  serializeJson(doc, msg);
  mqtt.publish("synkro/discovery", msg.c_str());

  Serial.println("[MQTT] Discovery sent: " + msg);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.print("[MQTT] Message on "); Serial.print(topic); Serial.print(": "); Serial.println(msg);

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg)) {
    Serial.println("[MQTT] JSON parse failed");
    return;
  }

  if (doc.containsKey("action")) {
    String action = doc["action"].as<String>();
    if (action == "on") {
      lampState = true;
      digitalWrite(RELAY_PIN, HIGH);
    } else if (action == "off") {
      lampState = false;
      digitalWrite(RELAY_PIN, LOW);
    }
    reportState();
  }
}

void connectToMQTT() {
  mqtt.setServer(BROKER_IP, BROKER_PORT);
  mqtt.setCallback(mqttCallback);

  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting...");

    // PubSubClient LWT via connect() overload:
    // connect(clientID, willTopic, willQoS, willRetain, willMessage)
    String lwtTopic = String("synkro/devices/") + DEVICE_ID + "/lwt";
    bool ok = mqtt.connect(DEVICE_ID, lwtTopic.c_str(), 1, true, "offline");

    if (ok) {
      Serial.println("connected!");

      // Immediately publish ONLINE (retained) to the LWT topic
      mqtt.publish(lwtTopic.c_str(), "online", true);

      // Subscribe to control for this device
      String controlTopic = String("synkro/devices/") + DEVICE_ID + "/control";
      mqtt.subscribe(controlTopic.c_str());
      Serial.println("[MQTT] Subscribed to " + controlTopic);

      // Announce current state + discovery
      reportState();
      sendDiscovery();
    } else {
      Serial.print("failed, rc="); Serial.print(mqtt.state());
      Serial.println(" retrying in 5s...");
      delay(5000);
    }
  }
}

// ================== Button handler ==================
void handleButton() {
  static bool lastState = HIGH;
  bool currentState = digitalRead(BUTTON_PIN);

  if (lastState == HIGH && currentState == LOW && (millis() - lastButtonPress > debounceDelay)) {
    lastButtonPress = millis();
    lampState = !lampState;
    digitalWrite(RELAY_PIN, lampState ? HIGH : LOW);
    Serial.println(lampState ? "[BUTTON] Lamp ON" : "[BUTTON] Lamp OFF");
    reportState();
  }
  lastState = currentState;
}

// ================== Setup / Loop ==================
void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n[Booting FireBeetle 2 ESP32-E]");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  prefs.begin("wifi", false);

  // If the physical RST button was pressed, clear Wi-Fi creds and enter provisioning
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_EXT) {
    Serial.println("[RESET] Physical reset detected → clearing Wi-Fi credentials...");
    prefs.clear();
    prefs.end();
    delay(400);
    startProvisioning();
    return;
  }

  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();

  if (ssid.isEmpty() || pass.isEmpty()) {
    Serial.println("[Info] No saved Wi-Fi credentials → provisioning mode.");
    startProvisioning();
  } else {
    connectToWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Connection failed → provisioning mode.");
      startProvisioning();
    } else {
      connectToMQTT();
    }
  }
}

void loop() {
  if (!needProvisioning) {
    if (!mqtt.connected()) connectToMQTT();
    mqtt.loop();

    handleButton();

    if (millis() - lastReport > reportInterval) {
      lastReport = millis();
      reportState();
      sendDiscovery(); // keep discovery fresh for UI scanners
    }
  }
}
