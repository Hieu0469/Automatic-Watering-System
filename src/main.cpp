#include <Arduino.h>
#include "DHT22.h"
#include "BLE_server.h"
#include "SoilSensor.h"
#include "FuzzyLogic.h"
#include "FuzzyConfig.h"

// ─── Cấu hình chân ───────────────────────────────────────────────────────────
#define DHT_PIN            15
#define PUMP_PIN            4
#define MAX_WATER_TIME_MS  1500
#define SENSOR_INTERVAL_MS 10000

// ─── Instance toàn cục (khai báo extern trong FuzzyConfig.h) ─────────────────
FuzzyConfig fuzzyConfig;

DHT22Driver dht(DHT_PIN);
SoilSensor  soil;

volatile float water_time_ms = 0.0f;
volatile bool  data_ready    = false;

// ─── Task đọc cảm biến ───────────────────────────────────────────────────────
void sensorTask(void *pvParam) {
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        if (!dht.read()) {
            Serial.println("[Sensor] DHT22 doc loi, bo qua.");
        } else {
            float temp    = dht.temperature();
            float hum     = dht.humidity();
            float soilPct = soil.readMoisturePct();
            float pct     = fuzzy_defuzzify(temp, hum, soilPct);
            float wt      = pct * MAX_WATER_TIME_MS / 100.0f;

            Serial.printf("[Sensor] Temp=%.1fC  Hum=%.1f%%  Soil=%.1f%%  Water=%.0fms\n",
                          temp, hum, soilPct, wt);

            ble_send_sensor_data(temp, hum, soilPct);
            water_time_ms = wt;
            data_ready    = true;
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
    }
}

// ─── Task điều khiển bơm ─────────────────────────────────────────────────────
void wateringTask(void *pvParam) {
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, LOW);
    for (;;) {
        if (data_ready) {
            data_ready = false;
            float wt = water_time_ms;
            if (wt > 0) {
                Serial.printf("[Watering] Bat bom %.0f ms\n", wt);
                digitalWrite(PUMP_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS((uint32_t)wt));
                digitalWrite(PUMP_PIN, LOW);
                Serial.println("[Watering] Tat bom.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ─── setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(9600);
    delay(500);
    Serial.println("=== He Thong Tuoi Tu Dong ===");

    fuzzyConfig.begin();   // Đọc NVS trước khi khởi tạo BLE
    dht.begin();
    soil.begin();
    ble_server_init();

    xTaskCreate(sensorTask,   "SensorTask",   4096, NULL, 1, NULL);
    xTaskCreate(wateringTask, "WateringTask", 2048, NULL, 2, NULL);
}

// ─── loop — xử lý gửi config khi web yêu cầu ────────────────────────────────
void loop() {
    if (ble_config_requested()) {
        ble_clear_config_request();
        String json = fuzzyConfig.toJSON();
        ble_send_config(json);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}