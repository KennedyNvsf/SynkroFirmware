#pragma once
#include <Arduino.h>
#include <vector>
#include <PubSubClient.h>
#include "../devices/DeviceBase.h"

class Room {
public:
  explicit Room(const String& name) : _name(name) {}

  void addDevice(Device* d);
  void beginAll();
  void handleAll();
  void attachMqttAll(PubSubClient* client);

  // broadcast publish state for all devices in room
  void publishAll(const String& deviceIdRoot);

  const String& name() const { return _name; }
  const std::vector<Device*>& devices() const { return _devices; }

private:
  String _name;
  std::vector<Device*> _devices;
};
