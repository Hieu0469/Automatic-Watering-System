#pragma once
#include <Arduino.h>

// ─── Hằng số đầu ra fuzzy ────────────────────────────────────────────────────
#define WATER_NONE  0
#define WATER_LOW   1
#define WATER_HIGH  2

// ─── Ma trận luật (giữ nguyên từ main.c gốc) ─────────────────────────────────
// Hàng: độ ẩm không khí [thấp, vừa, cao]
// Cột : nhiệt độ         [cao,  vừa, thấp]

static const int RULE_MOIS_LOW[3][3] = {
    {2, 2, 2},
    {2, 2, 1},
    {2, 1, 1}
};
static const int RULE_MOIS_MED[3][3] = {
    {2, 1, 1},
    {1, 1, 0},
    {1, 0, 0}
};
static const int RULE_MOIS_HIGH[3][3] = {
    {1, 0, 0},
    {0, 0, 0},
    {0, 0, 0}
};

// ─── Hàm membership ──────────────────────────────────────────────────────────

inline float mem_soil_low(float s) {
    if (s <= 30.0f) return 1.0f;
    if (s >= 50.0f) return 0.0f;
    return (50.0f - s) / 20.0f;
}
inline float mem_soil_med(float s) {
    if (s <= 30.0f || s >= 70.0f) return 0.0f;
    if (s < 50.0f) return (s - 30.0f) / 20.0f;
    return (70.0f - s) / 20.0f;
}
inline float mem_soil_high(float s) {
    if (s <= 50.0f) return 0.0f;
    if (s >= 70.0f) return 1.0f;
    return (s - 50.0f) / 20.0f;
}

inline float mem_temp_hot(float t) {
    if (t <= 25.0f) return 0.0f;
    if (t >= 30.0f) return 1.0f;
    return (t - 25.0f) / 5.0f;
}
inline float mem_temp_med(float t) {
    if (t <= 20.0f || t >= 30.0f) return 0.0f;
    if (t < 25.0f) return (t - 20.0f) / 5.0f;
    return (30.0f - t) / 5.0f;
}
inline float mem_temp_cold(float t) {
    if (t <= 20.0f) return 1.0f;
    if (t >= 25.0f) return 0.0f;
    return (25.0f - t) / 5.0f;
}

inline float mem_hum_low(float h) {
    if (h <= 40.0f) return 1.0f;
    if (h >= 60.0f) return 0.0f;
    return (60.0f - h) / 20.0f;
}
inline float mem_hum_med(float h) {
    if (h <= 40.0f || h >= 80.0f) return 0.0f;
    if (h < 60.0f) return (h - 40.0f) / 20.0f;
    return (80.0f - h) / 20.0f;
}
inline float mem_hum_high(float h) {
    if (h <= 60.0f) return 0.0f;
    if (h >= 80.0f) return 1.0f;
    return (h - 60.0f) / 20.0f;
}

// ─── Defuzzify — trả về % thời gian tưới (0–100) ─────────────────────────────
inline float fuzzy_defuzzify(float temp, float hum, float soil) {
    float t[3] = { mem_temp_hot(temp),  mem_temp_med(temp),  mem_temp_cold(temp)  };
    float h[3] = { mem_hum_low(hum),    mem_hum_med(hum),    mem_hum_high(hum)    };
    float s[3] = { mem_soil_low(soil),  mem_soil_med(soil),  mem_soil_high(soil)  };

    float wHigh = 0, wLow = 0, wNone = 0;

    // Áp dụng 3 bảng luật theo mức độ ẩm đất
    const int (*rules[3])[3] = { RULE_MOIS_LOW, RULE_MOIS_MED, RULE_MOIS_HIGH };

    for (int m = 0; m < 3; m++) {           // soil level
        for (int i = 0; i < 3; i++) {       // hum level
            for (int j = 0; j < 3; j++) {   // temp level
                float firing = min(min(t[j], h[i]), s[m]);
                int   output = rules[m][i][j];
                if      (output == WATER_HIGH) wHigh = max(wHigh, firing);
                else if (output == WATER_LOW)  wLow  = max(wLow,  firing);
                else                           wNone = max(wNone, firing);
            }
        }
    }

    float total = wHigh + wLow + wNone;
    if (total == 0.0f) return 0.0f;

    // Centroid defuzzification: HIGH=100%, LOW=50%, NONE=0%
    return (wHigh * 100.0f + wLow * 50.0f) / total;
}
