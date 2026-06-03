#include <Arduino.h>
#include "DHT22.h"
#include "BLE_server.h"
#include "SoilSensor.h"
#include "FuzzyLogic.h"
#include "FuzzyConfig.h"

// ─── Cấu hình ────────────────────────────────────────────────────────────────
#define DHT_PIN            15
#define PUMP_PIN            4
#define MAX_WATER_TIME_MS  1500
#define SENSOR_INTERVAL_MS 10000

// Ngưỡng phát hiện hết nước:
// Nếu soil <= SOIL_DRY_THRESHOLD mà sau TANK_FAIL_COUNT chu kỳ soil vẫn
// không tăng >= TANK_MIN_RISE_PCT → cảnh báo hết nước
#define SOIL_DRY_THRESHOLD  40.0f  // % — dưới mức này mới cần theo dõi
#define TANK_MIN_RISE_PCT    1.0f  // % tối thiểu soil phải tăng
#define TANK_FAIL_COUNT         2  // số chu kỳ liên tiếp không tăng → alert
#define MIN_PUMP_TIME_MS         500  // thời gian tối thiểu phải bật bơm để tính là đã tưới
// ─── Instances ───────────────────────────────────────────────────────────────
FuzzyConfig fuzzyConfig;
DHT22Driver dht(DHT_PIN);
SoilSensor  soil;

// ─── Chia sẻ wateringTask ────────────────────────────────────────────────────
volatile float g_water_ms  = 0.0f;
volatile bool  g_data_ready = false;

// ─── Trạng thái bể — chỉ sensorTask ghi ─────────────────────────────────────
static float prevSoil        = -1.0f; // soil chu kỳ trước
static int   failCount       = 0;     // số chu kỳ liên tiếp soil không tăng khi đang khô
static bool  tankAlertActive = false;

// giao tiếp với loop() để gửi BLE (tránh gọi BLE từ task)
static volatile bool  alertPending = false;
static volatile bool  alertIsClear = false;

// ─── Task đọc cảm biến ───────────────────────────────────────────────────────
void sensorTask(void *pvParam) {
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        if (!dht.read()) {
            Serial.println("[Sensor] DHT22 doc loi, bo qua.");
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
            continue;
        }

        float temp    = dht.temperature();
        float hum     = dht.humidity();
        float soilPct = soil.readMoisturePct();
        float pct     = fuzzy_defuzzify(temp, hum, soilPct);
        float wt      = pct * MAX_WATER_TIME_MS / 100.0f;

        // ── Phát hiện hết nước ───────────────────────────────────────────────
        // Điều kiện theo dõi: đất đang khô (cần tưới)
        if (soilPct <= SOIL_DRY_THRESHOLD && prevSoil >= 0.0f) {
            float rise = soilPct - prevSoil;
            // Soil không tăng (hoặc giảm) → đất vẫn khô dù đã qua 1 chu kỳ
            if (rise < TANK_MIN_RISE_PCT && g_water_ms > MIN_PUMP_TIME_MS) {
                failCount++;
                Serial.printf("[Tank] Dat kho, soil khong tang (%.1f%% -> %.1f%%). failCount=%d/%d\n",
                              prevSoil, soilPct, failCount, TANK_FAIL_COUNT);
                if (failCount >= TANK_FAIL_COUNT && !tankAlertActive) {
                    tankAlertActive = true;
                    alertPending    = true;
                    alertIsClear    = false;
                    Serial.println("[Tank] >>> CANH BAO: Co the het nuoc! <<<");
                }
            } else {
                // Soil tăng → bể còn nước, reset
                if (tankAlertActive) {
                    tankAlertActive = false;
                    alertPending    = true;
                    alertIsClear    = true;
                    Serial.println("[Tank] Binh thuong tro lai.");
                }
                failCount = 0;
            }
        } else if (soilPct > SOIL_DRY_THRESHOLD) {
            // Đất đã đủ ẩm → reset bộ đếm
            if (failCount > 0) {
                Serial.printf("[Tank] Dat du am (%.1f%%), reset failCount.\n", soilPct);
            }
            failCount = 0;
            if (tankAlertActive) {
                tankAlertActive = false;
                alertPending    = true;
                alertIsClear    = true;
            }
        }
        prevSoil = soilPct;

        // ── Gửi BLE: "temp,hum,soil,waterMs,failCount" ───────────────────────
        ble_send_sensor_data(temp, hum, soilPct, (int)wt, failCount);
        Serial.printf("[Sensor] Temp=%.1fC  Hum=%.1f%%  Soil=%.1f%%  Water=%.0fms  Fail=%d\n",
                      temp, hum, soilPct, wt, failCount);

        g_water_ms   = wt;
        g_data_ready = true;

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
    }
}

// ─── Task điều khiển bơm ─────────────────────────────────────────────────────
void wateringTask(void *pvParam) {
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, LOW);
    for (;;) {
        if (g_data_ready) {
            g_data_ready = false;
            float wt = g_water_ms;
            if (wt > MIN_PUMP_TIME_MS) {
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

// ─── setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(9600);
    delay(500);

    Preferences prefs;
    prefs.begin("fuzzy", false);
    prefs.clear();
    prefs.end();
    Serial.println("[Debug] Da xoa NVS.");

    Serial.println("=== He Thong Tuoi Tu Dong ===");
    fuzzyConfig.begin();
    dht.begin();
    soil.begin();
    ble_server_init();
    xTaskCreate(sensorTask,   "SensorTask",   4096, NULL, 1, NULL);
    xTaskCreate(wateringTask, "WateringTask", 4096, NULL, 2, NULL);
}

// ─── loop ─────────────────────────────────────────────────────────────────────
void loop() {
    if (ble_config_requested()) {
        ble_clear_config_request();
        ble_send_config(fuzzyConfig.toJSON());
    }
    if (alertPending) {
        alertPending = false;
        ble_send_alert_msg(alertIsClear ? "ALERT:CLEAR" : "ALERT:NO_WATER");
    }
    // Nhắc lại alert mỗi 30s nếu chưa được xử lý
    if (tankAlertActive) {
        static unsigned long lastRepeat = 0;
        if (millis() - lastRepeat > 30000) {
            lastRepeat = millis();
            ble_send_alert_msg("ALERT:NO_WATER");
        }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}