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
//  - OPTIONAL: dynamic broker IP updates via MQTT config topic
//
// Dynamic broker IP (Option A)
// ----------------------------
// If your Raspberry Pi publishes a config message like:
//
//   Topic:  synkro/broker/config
//   Payload: { "tcpBrokerIp": "192.168.1.90" }
//
// the firmware will:
//   - update its internal broker IP
//   - persist it to NVS (namespace "mqtt", key "broker_ip")
//   - reconnect to the new broker IP on the next attempt.
//
// On boot, the runtime will:
//   - read "mqtt/broker_ip" from NVS;
//   - if present, use it;
//   - otherwise fall back to the brokerIp passed to begin().

namespace mqtt_runtime {

  // Initialize internal MQTT client and bind the main lighting device.
  //
  // deviceId   : e.g. "synkro_res_p_beta"
  // deviceName : friendly name ("IEFP_AUDITORIUM")
  // brokerIp   : default broker IP (hint / fallback)
  // brokerPort : usually 1883
  // brokerMdns : optional mDNS hostname WITHOUT ".local" (e.g. "synkro-discovery")
  void begin(const char* deviceId,
             const char* deviceName,
             const char* brokerIp,
             uint16_t     brokerPort,
             const char*  brokerMdns,
             LightingDevice* mainLight);

  // Call this in loop() while Wi-Fi is connected.
  // Handles:
  //  - reconnect if needed
  //  - mqtt.loop()
  //  - periodic state + discovery publish
  void loop();

  // Called when devices change state and we want instant aggregate update.
  void notifyStateChanged();

} // namespace mqtt_runtime

// Legacy global alias (so any existing code calling this still compiles)
inline void notifyMainStateChanged() {
  mqtt_runtime::notifyStateChanged();
}