// ================== Synkro ESP32 Firmware (FireBeetle 2 ESP32-E) ==================
#include <Arduino.h>
#include <WiFi.h>

#include "core/config.h"

#include "devices/DeviceBase.h"
#include "devices/LightingDevice.h"
#include "core/wifi_manager.h"
#include "core/mqtt_manager.h"


// ------------------ DEVICES ------------------

// Our single lighting device for now
LightingDevice mainRoomLight(
  "MainRoomLight",      // id (matches Firestore seed)
  "Main Room Light",    // display name
  "MainRoom",           // room key
  RELAY_PIN,
  BUTTON_PIN
);

// ------------------ SETUP / LOOP ------------------

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n[Booting FireBeetle 2 ESP32-E]");

  // Local IO
  mainRoomLight.begin();

  // Wi-Fi + provisioning
  wifi_portal::begin(DEVICE_ID, AP_SSID, AP_PASS);

  // If we managed to connect STA, initialize MQTT runtime
  if (wifi_portal::isConnected()) {
    mqtt_runtime::begin(
      DEVICE_ID,
      DEVICE_NAME,
      BROKER_IP,
      BROKER_PORT,
      BROKER_MDNS,
      &mainRoomLight
    );
  }
}

void loop() {
  // Physical local control should ALWAYS work,
  // even if Wi-Fi / MQTT / broker are offline.
  mainRoomLight.handle();

  // If we are in provisioning AP mode, just keep the portal alive.
  if (wifi_portal::isProvisioning()) {
    wifi_portal::loop();
    delay(50);
    return;
  }

  // If STA is not connected yet, let Wi-Fi handle itself.
  if (!wifi_portal::isConnected()) {
    wifi_portal::loop();
    delay(50);
    return;
  }

  // Wi-Fi is up → keep MQTT runtime going
  mqtt_runtime::loop();
}