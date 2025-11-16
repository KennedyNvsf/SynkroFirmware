#pragma once

// Small Wi-Fi + provisioning helper for Synkro.
// Encapsulates:
//  - NVS credentials ("wifi" namespace: ssid, pass)
//  - AP provisioning portal on http://<ap_ip>/
//  - STA connection attempts

namespace wifi_portal {

  // Initialize Wi-Fi stack and either:
  //  - connect to saved Wi-Fi, OR
  //  - start provisioning AP if no/failed creds or hard reset
  //
  // deviceId: used as WiFi hostname and AP hint
  // apSsid / apPass: credentials for the local provisioning AP
  void begin(const char* deviceId,
             const char* apSsid,
             const char* apPass);

  // True while we're in provisioning mode (AP + captive form).
  bool isProvisioning();

  // True if STA is connected to user Wi-Fi (and not in provisioning).
  bool isConnected();

  // Optional hook, currently a no-op except for future extensibility.
  // Call this in loop() even if you don't strictly need it.
  void loop();
}