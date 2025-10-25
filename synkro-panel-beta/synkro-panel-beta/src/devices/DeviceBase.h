#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

class Device {
public:
  explicit Device(const String& id, const String& name, const String& category, const String& room)
  : _id(id), _name(name), _category(category), _room(room) {}

  virtual ~Device() = default;

  const String& id() const { return _id; }
  const String& name() const { return _name; }
  const String& category() const { return _category; }
  const String& room() const { return _room; }

  // lifecycle
  virtual void begin() = 0;
  virtual void handle() = 0;

  // mqtt integration (called by room or manager)
  virtual void attachMqtt(PubSubClient* client) { _mqtt = client; }
  virtual void onMqttControl(const String& json) = 0;
  virtual void publishState(const String& deviceIdRoot) = 0;

protected:
  PubSubClient* mqtt() const { return _mqtt; }

private:
  String _id;
  String _name;
  String _category;
  String _room;
  PubSubClient* _mqtt = nullptr;
};
