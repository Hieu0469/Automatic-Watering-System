#pragma once
#include <Arduino.h>

void ble_server_init();
void ble_send_sensor_data(float temp, float hum, float soil);
void ble_send_config(const String &json);
bool ble_is_connected();
bool ble_config_requested();
void ble_clear_config_request();