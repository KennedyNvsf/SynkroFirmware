// src/core/mqtt_manager.cpp
#include "mqtt_manager.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "devices/DeviceBase.h"
#include "devices/LightingDevice.h"

// -------- statics --------
static const char* sDeviceId    = nullptr;
static const char* sDeviceName  = nullptr;
static const char* sBrokerIp    = nullptr;
static uint16_t    sBrokerPort  = 1883;
static const char* sBrokerMdns  = nullptr;

static LightingDevice* sMainLight = nullptr;

static WiFiClient     sEspClient;
static PubSubClient   sMqtt(sEspClient);

static unsigned long  sLastReport         = 0;
static const unsigned long REPORT_MS      = 10000UL;   // 10 s

// Reconnect throttling (non-blocking)
static unsigned long  sLastConnectAttempt = 0;
static const unsigned long RECONNECT_MS   = 5000UL;    // min 5 s between attempts

// Fail-safe: after too many failures, stop trying so local IO is 100% free
static uint8_t        sFailCount          = 0;
static const uint8_t  MAX_FAILS           = 5;

// forward declarations
static void ensureMqttConnectedNonBlocking();
static void reportState();
static void sendDiscovery();
static void mqttCallback(char* topic, byte* payload, unsigned int length);

static String wsUrlFromIp() {
  // WebSocket URL for your Pi's broker (used by the web app)
  return "ws://" + String(sBrokerIp ? sBrokerIp : "") + ":9001";
}

static String mdnsHost() {
  if (!sBrokerMdns || !strlen(sBrokerMdns)) return String("");
  return String(sBrokerMdns) + ".local";
}

// -------- public API --------
void mqtt_runtime::begin(
  const char* deviceId,
  const char* deviceName,
  const char* brokerIp,
  uint16_t    brokerPort,
  const char* brokerMdns,
  LightingDevice* mainLight
) {
  sDeviceId   = deviceId;
  sDeviceName = deviceName;
  sBrokerIp   = brokerIp;
  sBrokerPort = brokerPort;
  sBrokerMdns = brokerMdns;
  sMainLight  = mainLight;

  // 🛡 Make TCP operations as “cheap” as possible
  // Prevent long blocking during connect() when broker is down
  sEspClient.setTimeout(200);      // 200 ms socket timeout
  sEspClient.setNoDelay(true);     // send small packets promptly

  // Let devices publish per-device state via Device::mqtt()
  Device::setMqttClient(&sMqtt);
  sMqtt.setServer(sBrokerIp, sBrokerPort);
  sMqtt.setCallback(mqttCallback);

  sLastConnectAttempt = 0;
  sFailCount          = 0;

  // First attempt – still non-blocking-ish (capped by 200 ms)
  ensureMqttConnectedNonBlocking();
}

void mqtt_runtime::loop() {
  // 🔁 MUST NOT BLOCK – physical IO (button / relay) depends on this.

  // If not connected, try a lightweight reconnect every RECONNECT_MS.
  ensureMqttConnectedNonBlocking();

  if (sMqtt.connected()) {
    sMqtt.loop();

    unsigned long now = millis();
    if (now - sLastReport > REPORT_MS) {
      sLastReport = now;
      reportState();
      sendDiscovery(); // keep discovery fresh for the scanner
    }
  }
}

void mqtt_runtime::notifyStateChanged() {
  // If MQTT is up, this republishes; if it's down, this is a cheap no-op.
  reportState();
}

// -------- internal helpers --------
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    msg += static_cast<char>(payload[i]);
  }

  Serial.print("[MQTT] Message on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(msg);

  if (!sDeviceId || !sMainLight) return;

  // Global control topic: synkro/devices/<DEVICE_ID>/control
  const String baseControl = String("synkro/devices/") + sDeviceId + "/control";
  if (String(topic) == baseControl) {
    // For now we just have one controlled device
    sMainLight->onMqttControl(msg);
    // Immediately push aggregate state for the web UI
    reportState();
  }
}

static void ensureMqttConnectedNonBlocking() {
  // Already connected → nothing to do.
  if (sMqtt.connected()) return;

  // No Wi-Fi → don't even try.
  if (WiFi.status() != WL_CONNECTED) return;

  // If we've failed too many times, stop trying entirely until reboot.
  // 👉 This guarantees the firmware never gets “stuck” hammering a dead broker.
  if (sFailCount >= MAX_FAILS) {
    return;
  }

  unsigned long now = millis();
  // Throttle reconnection attempts (avoid hammering broker).
  if (now - sLastConnectAttempt < RECONNECT_MS) {
    return;
  }
  sLastConnectAttempt = now;

  if (!sDeviceId || !sBrokerIp) return;

  Serial.print("[MQTT] Connecting to broker ");
  Serial.print(sBrokerIp);
  Serial.print(":");
  Serial.print(sBrokerPort);
  Serial.print(" ... ");

  // LWT: synkro/devices/<ID>/lwt retained "offline"
  String lwtTopic = String("synkro/devices/") + sDeviceId + "/lwt";

  bool ok = sMqtt.connect(
    sDeviceId,
    lwtTopic.c_str(), // will topic
    1,                // QoS
    true,             // retained
    "offline"         // will payload
  );

  if (!ok) {
    sFailCount++;
    Serial.print("failed, rc=");
    Serial.println(sMqtt.state());
    return;   // ❗ important: just return, don't block any longer
  }

  // Success: reset fail counter
  sFailCount = 0;
  Serial.println("connected!");

  // Immediately publish ONLINE (retained) to the LWT topic
  sMqtt.publish(lwtTopic.c_str(), "online", true);

  // Subscribe to global control
  String controlTopic = String("synkro/devices/") + sDeviceId + "/control";
  sMqtt.subscribe(controlTopic.c_str());
  Serial.println("[MQTT] Subscribed to " + controlTopic);

  // Announce current state + discovery right away
  reportState();
  sendDiscovery();
}

static void reportState() {
  if (!sMqtt.connected() || !sDeviceId || !sDeviceName) return;

  StaticJsonDocument<256> state;
  state["deviceId"] = sDeviceId;
  state["name"]     = sDeviceName;
  state["uptime"]   = millis() / 1000;
  state["status"]   = "online";
  state["ip"]       = WiFi.localIP().toString();
  state["brokerUrl"] = wsUrlFromIp();
  if (mdnsHost().length()) state["mdns"] = mdnsHost();

  // This matches your web liveKey = "light"
  if (sMainLight) {
    state["light"] = sMainLight->isOn() ? "on" : "off";
  }

  String msg;
  serializeJson(state, msg);

  // Aggregate topic used by your web app
  String topicPerDevice = String("synkro/devices/") + sDeviceId + "/state";
  sMqtt.publish(topicPerDevice.c_str(), msg.c_str());

  // Optional legacy global topic
  sMqtt.publish("synkro/devices/state", msg.c_str());

  // Also publish per-device state (for future)
  if (sMainLight) {
    sMainLight->publishState(sDeviceId);
  }

  Serial.println("[MQTT] State reported: " + msg);
}

static void sendDiscovery() {
  if (!sMqtt.connected() || !sDeviceId || !sDeviceName) return;

  StaticJsonDocument<256> doc;
  doc["id"]        = sDeviceId;
  doc["name"]      = sDeviceName;
  doc["ip"]        = WiFi.localIP().toString();
  doc["status"]    = "online";
  doc["brokerUrl"] = wsUrlFromIp();
  if (mdnsHost().length()) doc["mdns"] = mdnsHost();

  String msg;
  serializeJson(doc, msg);
  sMqtt.publish("synkro/discovery", msg.c_str());

  Serial.println("[MQTT] Discovery sent: " + msg);
}