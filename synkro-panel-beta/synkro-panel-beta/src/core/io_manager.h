#pragma once
#include <Arduino.h>

class IoManager {
public:
  IoManager(uint8_t relayPin, uint8_t buttonPin);

  void begin();
  // returns true if the lamp state changed because of a physical press
  bool tick(unsigned long nowMs);

  bool lampOn() const { return _lampOn; }
  void setLamp(bool on);

private:
  uint8_t _relayPin;
  uint8_t _buttonPin;

  bool _lampOn = false;
  bool _lastBtn = HIGH;
  unsigned long _lastDebounceAt = 0;
  static constexpr unsigned long DEBOUNCE_MS = 60;
};