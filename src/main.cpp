#include <Arduino.h>
#include "DHT22.h"
#include "BLE_server.h"
#include "SoilSensor.h"
#include "FuzzyLogic.h"

// ─── Cấu hình chân — giữ nguyên từ ESP-IDF gốc ──────────────────────────────
#define DHT_PIN         15    // GPIO15 — DHT22 data
#define PUMP_PIN         4    // GPIO4  — relay/van bơm
// SMS V1: D0 → GPIO5, A0 → GPIO34  (định nghĩa trong SoilSensor.h)

#define MAX_WATER_TIME_MS  2000   // ms tưới tối đa
#define SENSOR_INTERVAL_MS 10000  // chu kỳ đọc cảm biến (10 giây)

// ─── Đối tượng ───────────────────────────────────────────────────────────────
DHT22Driver dht(DHT_PIN);
SoilSensor  soil;

// ─── Biến chia sẻ giữa 2 task ────────────────────────────────────────────────
volatile float water_time_ms = 0.0f;
volatile bool  data_ready    = false;

// ─── Task đọc cảm biến → fuzzy → BLE ─────────────────────────────────────────
void sensorTask(void *pvParam) {
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        if (!dht.read()) {
            Serial.println("[Sensor] DHT22 doc loi, bo qua.");
        } else {
            float temp     = dht.temperature();
            float hum      = dht.humidity();
            float soilPct  = soil.readMoisturePct();

            float pct = fuzzy_defuzzify(temp, hum, soilPct);
            float wt  = pct * MAX_WATER_TIME_MS / 100.0f;

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
            } else {
                Serial.println("[Watering] Khong can tuoi.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ─── setup & loop ────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(9600);
    delay(500);
    Serial.println("=== He Thong Tuoi Tu Dong ===");

    dht.begin();
    soil.begin();
    ble_server_init();

    xTaskCreate(sensorTask,   "SensorTask",   4096, NULL, 1, NULL);
    xTaskCreate(wateringTask, "WateringTask", 2048, NULL, 2, NULL);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
