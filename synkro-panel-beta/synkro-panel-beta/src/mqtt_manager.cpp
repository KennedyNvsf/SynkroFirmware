#include "mqtt_manager.h"
#include "config.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "io_manager.h"

WiFiClient espClient;
PubSubClient mqtt(espClient);

String wsUrlFromIp() { return "ws://" + String(BROKER_IP) + ":9001"; }
String mdnsHost() { return String(BROKER_MDNS) + ".local"; }

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.printf("[MQTT] %s: %s\n", topic, msg.c_str());

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg)) return;

  if (doc.containsKey("action")) {
    String action = doc["action"];
    if (action == "on") setLamp(true);
    else if (action == "off") setLamp(false);
    reportState();
  }
}

void reportState() {
  if (!mqtt.connected()) return;
  StaticJsonDocument<256> state;
  state["deviceId"] = DEVICE_ID;
  state["name"] = DEVICE_NAME;
  state["uptime"] = millis() / 1000;
  state["status"] = "online";
  state["lamp"] = getLampState() ? "on" : "off";
  state["ip"] = WiFi.localIP().toString();
  state["brokerUrl"] = wsUrlFromIp();
  state["mdns"] = mdnsHost();
  String msg; serializeJson(state, msg);
  mqtt.publish(("synkro/devices/" + String(DEVICE_ID) + "/state").c_str(), msg.c_str());
  Serial.println("[MQTT] State: " + msg);
}

void sendDiscovery() {
  if (!mqtt.connected()) return;
  StaticJsonDocument<256> doc;
  doc["id"] = DEVICE_ID;
  doc["name"] = DEVICE_NAME;
  doc["ip"] = WiFi.localIP().toString();
  doc["status"] = "online";
  doc["brokerUrl"] = wsUrlFromIp();
  doc["mdns"] = mdnsHost();
  String msg; serializeJson(doc, msg);
  mqtt.publish("synkro/discovery", msg.c_str());
}

void connectToMQTT() {
  mqtt.setServer(BROKER_IP, BROKER_PORT);
  mqtt.setCallback(mqttCallback);
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting...");
    String lwtTopic = "synkro/devices/" + String(DEVICE_ID) + "/lwt";
    if (mqtt.connect(DEVICE_ID, lwtTopic.c_str(), 1, true, "offline")) {
      mqtt.publish(lwtTopic.c_str(), "online", true);
      mqtt.subscribe(("synkro/devices/" + String(DEVICE_ID) + "/control").c_str());
      reportState();
      sendDiscovery();
      Serial.println("connected!");
    } else {
      Serial.println("failed, retrying in 5s");
      delay(5000);
    }
  }
}

void mqttLoop() {
  if (!mqtt.connected()) connectToMQTT();
  mqtt.loop();
}
