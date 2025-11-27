// src/devices/LightingDevice.cpp
#include "LightingDevice.h"
#include <ArduinoJson.h>

// Use the MQTT runtime helper (notifyMainStateChanged alias)
#include "core/mqtt_manager.h"

LightingDevice::LightingDevice(
  const String& id,
  const String& name,
  const String& room,
  uint8_t relayPin,
  uint8_t buttonPin
) : Device(id, name, "lighting", room),
    _relayPin(relayPin),
    _buttonPin(buttonPin) {}

// ----------------------------------------------------
// Setup
// ----------------------------------------------------
void LightingDevice::begin() {
  // Logical default: lamp OFF
  _on = false;

  pinMode(_relayPin, OUTPUT);
  // For your hardware (NPN optocoupler + NPN relay):
  // HIGH → lamp ON
  // LOW  → lamp OFF
  digitalWrite(_relayPin, LOW);  // lamp OFF at boot

  pinMode(_buttonPin, INPUT_PULLUP);
}

// ----------------------------------------------------
// Local physical handling
// ----------------------------------------------------
void LightingDevice::handle() {
  // Simple button edge detect + debounce
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

  // ❌ IMPORTANT: no MQTT call here.
  // Physical control must work even if Wi-Fi/MQTT/broker are dead.
  // The global state will be reported periodically by mqtt_runtime.
}

void LightingDevice::setOn(bool v) {
  _on = v;
  writeRelay();

  // ✅ This one *is* allowed to touch MQTT,
  // because it's used when a command comes **from** the broker/UI.
  notifyMainStateChanged();  // alias → mqtt_runtime::notifyStateChanged()
}

void LightingDevice::writeRelay() {
  // For YOUR hardware:
  //  _on == true  → lamp ON  → drive HIGH
  //  _on == false → lamp OFF → drive LOW
  digitalWrite(_relayPin, _on ? HIGH : LOW);
}

// ----------------------------------------------------
// MQTT control & per-device state
// ----------------------------------------------------
void LightingDevice::onMqttControl(const String& json) {
  // expects {"action":"on"} | {"action":"off"} or {"toggle":true}
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, json)) return;

  if (doc.containsKey("toggle") && doc["toggle"] == true) {
    // This path is triggered by a "toggle" command from MQTT.
    // It will end up calling setOn(...) (via onMqttControl) or just toggle(),
    // and then we publish state (see setOn).
    toggle();
    // NOTE: toggle() no longer touches MQTT directly.
    // The next periodic report will sync, or you can switch to setOn()
    // if you want guaranteed immediate report even for "toggle".
    return;
  }

  if (doc.containsKey("action")) {
    String a = doc["action"].as<String>();
    if (a == "on") {
      setOn(true);   // this WILL publish MQTT state
    } else if (a == "off") {
      setOn(false);  // this WILL publish MQTT state
    }
  }
}

void LightingDevice::publishState(const String& deviceIdRoot) {
  if (!mqtt()) return;

  // Per-device state topic (for future use):
  // synkro/devices/<DEVICE_ID>/lighting/<DEVICE_ITEM_ID>/state
  const String topic =
      "synkro/devices/" + deviceIdRoot + "/lighting/" + id() + "/state";

  StaticJsonDocument<256> st;
  st["type"] = "lighting";
  st["room"] = room();
  st["name"] = name();
  st["id"]   = id();
  st["state"] = _on ? "on" : "off";  // semantic state

  String payload;
  serializeJson(st, payload);
  mqtt()->publish(topic.c_str(), payload.c_str());
}