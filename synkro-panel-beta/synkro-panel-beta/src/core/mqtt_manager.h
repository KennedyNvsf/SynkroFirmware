 #pragma once

#include <stdint.h>

class LightingDevice;

// Runtime MQTT / discovery helper for Synkro.
// Handles:
//  - MQTT connection & reconnection
//  - LWT topic
//  - aggregate state on synkro/devices/<ID>/state (with "light")
//  - discovery on synkro/discovery
//  - fanning control JSON to devices

namespace mqtt_runtime {

  // Initialize internal MQTT client and bind the main lighting device.
  //
  // deviceId   : e.g. "synkro_res_p_beta"
  // deviceName : friendly name ("IEFP_AUDITORIUM")
  // brokerIp   : TCP broker for ESP32 (e.g. "192.168.1.90")
  // brokerPort : usually 1883
  // brokerMdns : optional mDNS hostname WITHOUT ".local" (e.g. "synkro-discovery")
  void begin(const char* deviceId,
             const char* deviceName,
             const char* brokerIp,
             uint16_t    brokerPort,
             const char* brokerMdns,
             LightingDevice* mainLight);

  // Call this in loop() while Wi-Fi is connected.
  // Handles:
  //  - reconnect if needed
  //  - mqtt.loop()
  //  - periodic state + discovery publish
  void loop();

  // Called when devices change state and we want instant aggregate update.
  void notifyStateChanged();
}

// Legacy global alias (so any existing code calling this still compiles)
inline void notifyMainStateChanged() {
  mqtt_runtime::notifyStateChanged();
}