#pragma once
#include <Arduino.h>

void ble_server_init();
void ble_send_sensor_data(float temp, float hum, float soil, int waterMs, int failCount);
void ble_send_config(const String &json);
void ble_send_alert_msg(const String &msg);
bool ble_is_connected();
bool ble_config_requested();
void ble_clear_config_request();