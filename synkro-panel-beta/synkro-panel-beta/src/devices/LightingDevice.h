#pragma once

#include "DeviceBase.h"

class LightingDevice : public Device {
public:
  LightingDevice(const String& id,
                 const String& name,
                 const String& room,
                 uint8_t relayPin,
                 uint8_t buttonPin);

  void begin() override;
  void handle() override;
  void onMqttControl(const String& json) override;
  void publishState(const String& deviceIdRoot) override;

  bool isOn() const { return _on; }
  void setOn(bool v);

private:
  void toggle();
  void writeRelay();

  uint8_t _relayPin;
  uint8_t _buttonPin;
  bool    _on = false;
  unsigned long _lastButtonMs = 0;
};