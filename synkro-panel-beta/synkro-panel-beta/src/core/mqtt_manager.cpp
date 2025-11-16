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

static WiFiClient   sEspClient;
static PubSubClient sMqtt(sEspClient);

static unsigned long sLastReport      = 0;
static const unsigned long REPORT_MS  = 10000UL;

// forward declarations
static void connectToMQTT();
static void reportState();
static void sendDiscovery();
static void mqttCallback(char* topic, byte* payload, unsigned int length);

static String wsUrlFromIp() {
  // WebSocket URL for your Pi's broker (used for webapp)
  return "ws://" + String(sBrokerIp) + ":9001";
}

static String mdnsHost() {
  if (!sBrokerMdns || !strlen(sBrokerMdns)) return String("");
  return String(sBrokerMdns) + ".local";
}

// -------- public API --------

void mqtt_runtime::begin(const char* deviceId,
                         const char* deviceName,
                         const char* brokerIp,
                         uint16_t    brokerPort,
                         const char* brokerMdns,
                         LightingDevice* mainLight) {
  sDeviceId   = deviceId;
  sDeviceName = deviceName;
  sBrokerIp   = brokerIp;
  sBrokerPort = brokerPort;
  sBrokerMdns = brokerMdns;
  sMainLight  = mainLight;

  // Let devices publish per-device state via Device::mqtt()
  Device::setMqttClient(&sMqtt);

  sMqtt.setServer(sBrokerIp, sBrokerPort);
  sMqtt.setCallback(mqttCallback);

  connectToMQTT();
}

void mqtt_runtime::loop() {
  if (!sMqtt.connected()) {
    connectToMQTT();
  }

  sMqtt.loop();

  unsigned long now = millis();
  if (now - sLastReport > REPORT_MS) {
    sLastReport = now;
    reportState();
    sendDiscovery();  // keep discovery fresh
  }
}

void mqtt_runtime::notifyStateChanged() {
  // Lightweight: just republish aggregate state right now.
  reportState();
}

// -------- internal helpers --------

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("[MQTT] Message on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(msg);

  if (!sDeviceId || !sMainLight) return;

  // Global control topic: synkro/devices/<DEVICE_ID>/control
  const String baseControl =
      String("synkro/devices/") + sDeviceId + "/control";

  if (String(topic) == baseControl) {
    // Fan out to devices as needed (for now: single main light)
    sMainLight->onMqttControl(msg);
    // and immediately push aggregate state for the web UI
    reportState();
  }
}

static void connectToMQTT() {
  if (!sDeviceId) return;

  while (!sMqtt.connected()) {
    Serial.print("[MQTT] Connecting...");

    // LWT: synkro/devices/<ID>/lwt retained "offline"
    String lwtTopic =
        String("synkro/devices/") + sDeviceId + "/lwt";

    bool ok = sMqtt.connect(
        sDeviceId,
        lwtTopic.c_str(),
        1,
        true,
        "offline"
    );

    if (ok) {
      Serial.println("connected!");

      // Immediately publish ONLINE (retained) to the LWT topic
      sMqtt.publish(lwtTopic.c_str(), "online", true);

      // Subscribe to global control
      String controlTopic =
          String("synkro/devices/") + sDeviceId + "/control";
      sMqtt.subscribe(controlTopic.c_str());
      Serial.println("[MQTT] Subscribed to " + controlTopic);

      // Announce current state + discovery
      reportState();
      sendDiscovery();
    } else {
      Serial.print("failed, rc=");
      Serial.print(sMqtt.state());
      Serial.println(" retrying in 5s...");
      delay(5000);
    }
  }
}

static void reportState() {
  if (!sMqtt.connected() || !sDeviceId || !sDeviceName) return;

  StaticJsonDocument<256> state;
  state["deviceId"] = sDeviceId;
  state["name"]     = sDeviceName;
  state["uptime"]   = millis() / 1000;
  state["status"]   = "online";
  state["ip"]       = WiFi.localIP().toString();
  state["brokerUrl"]= wsUrlFromIp();
  if (mdnsHost().length()) state["mdns"] = mdnsHost();

  // IMPORTANT: this matches Firestore liveKey = "light"
  if (sMainLight) {
    state["light"] = sMainLight->isOn() ? "on" : "off";
  }

  String msg;
  serializeJson(state, msg);

  // Aggregate topic used by your web app
  String topicPerDevice =
      String("synkro/devices/") + sDeviceId + "/state";
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