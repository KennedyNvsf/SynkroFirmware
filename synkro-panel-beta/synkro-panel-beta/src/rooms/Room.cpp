#include "Room.h"

void Room::addDevice(Device* d) {
  _devices.push_back(d);
}

void Room::beginAll() {
  for (auto* d : _devices) d->begin();
}

void Room::handleAll() {
  for (auto* d : _devices) d->handle();
}

void Room::attachMqttAll(PubSubClient* client) {
  for (auto* d : _devices) d->attachMqtt(client);
}

void Room::publishAll(const String& deviceIdRoot) {
  for (auto* d : _devices) d->publishState(deviceIdRoot);
}