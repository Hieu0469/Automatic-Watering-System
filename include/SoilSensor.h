#pragma once
#include <Arduino.h>

// ─── Cấu hình chân — giữ nguyên như ESP-IDF gốc ─────────────────────────────
// D0 → GPIO5  (digital)
// A0 → GPIO34 (analog, ADC1_CH6)

#define SOIL_D0_PIN   5
#define SOIL_A0_PIN   34

// Ngưỡng ADC (0–4095): hiệu chỉnh theo thực tế
// raw thấp = ẩm, raw cao = khô
#define SOIL_THRESHOLD_WET  1200
#define SOIL_THRESHOLD_DRY  2800

// Số mẫu lấy trung bình để giảm nhiễu
#define SOIL_ADC_SAMPLES    16

class SoilSensor {
public:
    void begin() {
        pinMode(SOIL_D0_PIN, INPUT_PULLUP);
        // GPIO34 chỉ dùng analogRead, không cần pinMode
    }

    // Đọc độ ẩm đất (0–100%)
    // Map ngược: raw thấp = ẩm = 100%, raw cao = khô = 0%
    float readMoisturePct() {
        int raw = _readRawAvg();
        if (raw <= SOIL_THRESHOLD_WET) return 100.0f;
        if (raw >= SOIL_THRESHOLD_DRY) return 0.0f;
        float pct = 100.0f - (float)(raw - SOIL_THRESHOLD_WET)
                              / (SOIL_THRESHOLD_DRY - SOIL_THRESHOLD_WET) * 100.0f;
        return constrain(pct, 0.0f, 100.0f);
    }

    // true = đất ẩm (D0 = LOW, LM393 open-collector)
    bool readDigital() {
        return (digitalRead(SOIL_D0_PIN) == LOW);
    }

    int readRaw() { return _readRawAvg(); }

private:
    int _readRawAvg() {
        long sum = 0;
        for (int i = 0; i < SOIL_ADC_SAMPLES; i++) {
            sum += analogRead(SOIL_A0_PIN);
            delayMicroseconds(500);
        }
        return (int)(sum / SOIL_ADC_SAMPLES);
    }
};
