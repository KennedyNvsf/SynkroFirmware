#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <functional>
#include "config.h"

class MqttManager {
public:
  using LampCommandFn = std::function<void(bool on)>;

  MqttManager(
    Client& netClient,
    const String& deviceId,
    const String& deviceName);

  void begin(const IPAddress& brokerIp, uint16_t brokerPort);
  void tick(unsigned long nowMs);                 // call from loop()
  void onLampChanged(bool lampOn);                // call when IO changes
  void forceReport(unsigned long nowMs);          // optional: force a report now

  // NEW: set a handler to execute when control messages arrive
  void setLampCommandHandler(LampCommandFn cb) { _onLampCommand = std::move(cb); }

  bool connected() { return _mqtt.connected(); }

private:
  // connection
  void tryConnectOnce();
  void publishLwtOnline();
  void subscribeControl();
  static void _staticCallback(char* topic, byte* payload, unsigned int len);
  void _handleMessage(char* topic, byte* payload, unsigned int len);

  // topics/payloads
  void publishState();
  void publishDiscovery();

private:
  PubSubClient _mqtt;
  String _deviceId;
  String _deviceName;

  // topics
  String _topicState;    // synkro/devices/<id>/state
  String _topicCtrl;     // synkro/devices/<id>/control
  String _topicLwt;      // synkro/devices/<id>/lwt

  // timers (non-blocking)
  unsigned long _nextReconnectAt = 0;
  unsigned long _nextReportAt    = 0;

  // cached IO state to include in JSON
  bool _lampOn = false;

  // handler for control commands (web -> device)
  LampCommandFn _onLampCommand = nullptr;

  // constants
  static constexpr unsigned long RECONNECT_INTERVAL = 2000;  // 2s
  static constexpr unsigned long REPORT_INTERVAL    = 10000; // 10s
};