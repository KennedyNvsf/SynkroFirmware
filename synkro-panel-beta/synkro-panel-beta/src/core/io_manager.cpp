#include "io_manager.h"

IoManager::IoManager(uint8_t relayPin, uint8_t buttonPin)
: _relayPin(relayPin), _buttonPin(buttonPin) {}

void IoManager::begin() {
  pinMode(_relayPin, OUTPUT);
  digitalWrite(_relayPin, LOW);
  pinMode(_buttonPin, INPUT_PULLUP);
  _lastBtn = digitalRead(_buttonPin);
}

bool IoManager::tick(unsigned long nowMs) {
  bool btn = digitalRead(_buttonPin);
  bool changed = false;

  // falling edge with debounce
  if (_lastBtn == HIGH && btn == LOW && (nowMs - _lastDebounceAt) > DEBOUNCE_MS) {
    _lastDebounceAt = nowMs;
    _lampOn = !_lampOn;
    digitalWrite(_relayPin, _lampOn ? HIGH : LOW);
    changed = true; // physical change happened immediately
  }
  _lastBtn = btn;
  return changed;
}

void IoManager::setLamp(bool on) {
  _lampOn = on;
  digitalWrite(_relayPin, _lampOn ? HIGH : LOW);
}