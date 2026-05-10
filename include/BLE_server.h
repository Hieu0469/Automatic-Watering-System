#pragma once
#include <Arduino.h>

void ble_server_init();
void ble_send_sensor_data(float temp, float hum, float soil);
bool ble_is_connected();
