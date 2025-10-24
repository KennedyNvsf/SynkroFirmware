#include "io_manager.h"
#include "config.h"
#include <Arduino.h>
#include "mqtt_manager.h"

bool lampState = false;
unsigned long lastButtonPress = 0;

void setLamp(bool on) {
  lampState = on;
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  Serial.println(on ? "[LAMP] ON" : "[LAMP] OFF");
}

bool getLampState() {
  return lampState;
}

void handleButton() {
  static bool lastState = HIGH;
  bool current = digitalRead(BUTTON_PIN);
  if (lastState == HIGH && current == LOW && millis() - lastButtonPress > DEBOUNCE_DELAY) {
    lastButtonPress = millis();
    setLamp(!lampState);
    reportState();
  }
  lastState = current;
}
