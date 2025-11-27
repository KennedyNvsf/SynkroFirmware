#pragma once

// ---------- Wi-Fi AP (Provisioning) ----------
#define AP_SSID   "Synkro_Setup"
#define AP_PASS   "12345678"

// ---------- Device Identity ----------
#define DEVICE_ID   "synkro_res_p_beta"
#define DEVICE_NAME "IEFP_AUDITORIUM"

// ---------- MQTT Broker (TCP for ESP32) ----------
#define BROKER_IP     "10.30.14.37"
//home test "192.168.1.90"
#define BROKER_PORT  1883

// Optional: mDNS hostname of the broker (without .local) — hint for UI
#ifndef BROKER_MDNS
  #define BROKER_MDNS "synkro-discovery"
#endif

// ---------- GPIO ----------
#define BUTTON_PIN   25
#define RELAY_PIN    26

