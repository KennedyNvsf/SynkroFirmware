#include "LightingDevice.h"
#include <ArduinoJson.h>

LightingDevice::LightingDevice(
    const String& id,
    const String& name,
    const String& room,
    uint8_t relayPin,
    uint8_t buttonPin
)
: Device(id, name, "lighting", room),
  _relayPin(relayPin),
  _buttonPin(buttonPin) {}

void LightingDevice::begin() {
  pinMode(_relayPin, OUTPUT);
  digitalWrite(_relayPin, LOW);
  pinMode(_buttonPin, INPUT_PULLUP);
}

void LightingDevice::handle() {
  // simple button edge detect + debounce
  static bool lastState = HIGH;
  bool cur = digitalRead(_buttonPin);
  unsigned long now = millis();

  if (lastState == HIGH && cur == LOW && (now - _lastButtonMs) > 300UL) {
    _lastButtonMs = now;
    toggle();
  }
  lastState = cur;
}

void LightingDevice::toggle() {
  _on = !_on;
  writeRelay();
}

void LightingDevice::setOn(bool v) {
  _on = v;
  writeRelay();
}

void LightingDevice::writeRelay() {
  digitalWrite(_relayPin, _on ? HIGH : LOW);
}

void LightingDevice::onMqttControl(const String& json) {
  // expects {"action":"on"} | {"action":"off"} or {"toggle":true}
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, json)) return;

  if (doc.containsKey("toggle") && doc["toggle"] == true) {
    toggle();
    return;
  }
  if (doc.containsKey("action")) {
    String a = doc["action"].as<String>();
    if (a == "on") setOn(true);
    else if (a == "off") setOn(false);
  }
}

void LightingDevice::publishState(const String& deviceIdRoot) {
  if (!mqtt()) return;

  // per-device state (namespaced under device root)
  // synkro/devices/<DEVICE_ID>/lighting/<DEVICE_ITEM_ID>/state
  const String topic = "synkro/devices/" + deviceIdRoot + "/lighting/" + id() + "/state";

  StaticJsonDocument<256> st;
  st["type"] = "lighting";
  st["room"] = room();
  st["name"] = name();
  st["id"]   = id();
  st["state"] = _on ? "on" : "off";

  String payload;
  serializeJson(st, payload);
  mqtt()->publish(topic.c_str(), payload.c_str());
}
