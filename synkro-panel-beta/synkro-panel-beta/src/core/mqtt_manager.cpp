#include "mqtt_manager.h"

static MqttManager* s_self = nullptr;

MqttManager::MqttManager(Client& netClient,
                         const String& deviceId,
                         const String& deviceName)
: _mqtt(netClient),
  _deviceId(deviceId),
  _deviceName(deviceName)
{
  s_self = this;
  _mqtt.setCallback(&_staticCallback);

  _topicState = "synkro/devices/" + _deviceId + "/state";
  _topicCtrl  = "synkro/devices/" + _deviceId + "/control";
  _topicLwt   = "synkro/devices/" + _deviceId + "/lwt";
}

void MqttManager::begin(const IPAddress& brokerIp, uint16_t brokerPort) {
  _mqtt.setServer(brokerIp, brokerPort);
}

void MqttManager::tick(unsigned long nowMs) {
  // maintain connection (non-blocking)
  if (!_mqtt.connected()) {
    if (nowMs >= _nextReconnectAt) {
      tryConnectOnce();
      _nextReconnectAt = nowMs + RECONNECT_INTERVAL;
    }
    return; // don't loop/publish when disconnected
  }

  _mqtt.loop();

  // periodic state+discovery
  if (nowMs >= _nextReportAt) {
    publishState();
    publishDiscovery();
    _nextReportAt = nowMs + REPORT_INTERVAL;
  }
}

void MqttManager::onLampChanged(bool lampOn) {
  _lampOn = lampOn;
  if (_mqtt.connected()) {
    publishState(); // advertise immediately when connected
  }
}

void MqttManager::forceReport(unsigned long /*nowMs*/) {
  if (_mqtt.connected()) publishState();
}

void MqttManager::tryConnectOnce() {
  if (_mqtt.connected()) return;

  // PubSubClient LWT via connect(clientId, willTopic, qos, retain, willMessage)
  bool ok = _mqtt.connect(_deviceId.c_str(),
                          _topicLwt.c_str(),
                          1, true,
                          "offline");

  if (!ok) return;

  publishLwtOnline();
  subscribeControl();

  // first announce
  publishState();
  publishDiscovery();
}

void MqttManager::publishLwtOnline() {
  _mqtt.publish(_topicLwt.c_str(), "online", true /*retain*/);
}

void MqttManager::subscribeControl() {
  _mqtt.subscribe(_topicCtrl.c_str());
}

void MqttManager::_staticCallback(char* topic, byte* payload, unsigned int len) {
  if (s_self) s_self->_handleMessage(topic, payload, len);
}

void MqttManager::_handleMessage(char* topic, byte* payload, unsigned int len) {
  if (String(topic) != _topicCtrl) return;

  String msg;
  msg.reserve(len);
  for (unsigned int i = 0; i < len; ++i) msg += (char)payload[i];

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg)) return;

  // Expect {"action":"on"} or {"action":"off"}
  if (doc.containsKey("action")) {
    const String action = doc["action"].as<String>();
    if (_onLampCommand) {
      if (action == "on")  _onLampCommand(true);
      if (action == "off") _onLampCommand(false);
      // NOTE:
      // IoManager will flip GPIO immediately and then call onLampChanged(...)
      // so we do NOT publish state here to avoid double-publish.
    }
  }
}

void MqttManager::publishState() {
  StaticJsonDocument<256> state;
  state["deviceId"] = _deviceId;
  state["name"]     = _deviceName;
  state["uptime"]   = millis() / 1000;
  state["status"]   = "online";
  state["lamp"]     = _lampOn ? "on" : "off";
  state["ip"]       = WiFi.localIP().toString();
  state["brokerUrl"]= String("ws://") + WiFi.localIP().toString() + ":9001";
#ifdef BROKER_MDNS
  if (String(BROKER_MDNS).length()) {
    state["mdns"] = String(BROKER_MDNS) + ".local";
  }
#endif

  String msg;
  serializeJson(state, msg);

  _mqtt.publish(_topicState.c_str(), msg.c_str(), false);
  _mqtt.publish("synkro/devices/state", msg.c_str(), false);
  _mqtt.publish(_topicLwt.c_str(), "online", true);
}

void MqttManager::publishDiscovery() {
  StaticJsonDocument<256> doc;
  doc["id"]        = _deviceId;
  doc["name"]      = _deviceName;
  doc["ip"]        = WiFi.localIP().toString();
  doc["status"]    = "online";
  doc["brokerUrl"] = String("ws://") + WiFi.localIP().toString() + ":9001";
#ifdef BROKER_MDNS
  if (String(BROKER_MDNS).length()) {
    doc["mdns"] = String(BROKER_MDNS) + ".local";
  }
#endif

  String msg;
  serializeJson(doc, msg);
  _mqtt.publish("synkro/discovery", msg.c_str(), false);
}