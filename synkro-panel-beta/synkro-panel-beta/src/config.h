#pragma once

// ================== CONFIG ==================
#define AP_SSID        "Synkro_Setup"
#define AP_PASS        "12345678"
#define DEVICE_ID      "synkro_res_p_beta"
#define DEVICE_NAME    "IEFP_AUDITORIUM"
#define BROKER_IP      "192.168.1.90"
#define BROKER_PORT    1883
#define BROKER_MDNS    "synkro-discovery"

// GPIO
#define RELAY_PIN      25
#define BUTTON_PIN     26

// Timings
#define REPORT_INTERVAL  10000UL
#define DEBOUNCE_DELAY   300UL