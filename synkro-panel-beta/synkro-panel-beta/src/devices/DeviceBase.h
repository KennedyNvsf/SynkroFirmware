// src/devices/DeviceBase.h
#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

// Simple abstract base class for any controllable device
class Device {
public:
  Device(const String& id,
         const String& name,
         const String& category,
         const String& room)
    : _id(id),
      _name(name),
      _category(category),
      _room(room) {}

  virtual ~Device() {}

  const String& id() const       { return _id; }
  const String& name() const     { return _name; }
  const String& category() const { return _category; }
  const String& room() const     { return _room; }

  // Called from main.cpp during setup
  virtual void begin() = 0;

  // Called from loop() regularly
  virtual void handle() = 0;

  // Called when MQTT control message for THIS device arrives
  virtual void onMqttControl(const String& json) = 0;

  // Optional per-device state topic
  virtual void publishState(const String& deviceIdRoot) = 0;

  //
  // MQTT wiring
  //
  // Global setter (used from main.cpp)
  static void setMqttClient(PubSubClient* c) { _mqtt = c; }

  // Backward-compatible per-device hook (used by Room.cpp)
  // This just forwards to the static setter, so all devices share
  // the same PubSubClient instance.
  void attachMqtt(PubSubClient* c) { _mqtt = c; }

protected:
  PubSubClient* mqtt() const { return _mqtt; }

private:
  String _id;
  String _name;
  String _category;   // "lighting", "security", etc.
  String _room;       // "MainRoom", "Kitchen", etc.

  static PubSubClient* _mqtt;
};