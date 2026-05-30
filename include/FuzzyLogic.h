#pragma once
#include <Arduino.h>
#include "FuzzyConfig.h"

// ─── Hằng số đầu ra ──────────────────────────────────────────────────────────
#define WATER_NONE  0
#define WATER_LOW   1
#define WATER_HIGH  2

// ─── Hàm tam giác chung ──────────────────────────────────────────────────────
// a=chân trái, b=đỉnh, c=chân phải
// Khi a<=0 hoặc a==b: mở trái (trả 1 từ 0..b)
// Khi c>=max hoặc b==c: mở phải (trả 1 từ b..max)
inline float triMF(float x, float a, float b, float c) {
    if (x <= a) return (a == b) ? 1.0f : 0.0f;
    if (x >= c) return (b == c) ? 1.0f : 0.0f;
    if (x <= b) return (b == a) ? 1.0f : (x - a) / (b - a);
    return (c == b) ? 1.0f : (c - x) / (c - b);
}

// ─── Wrappers đọc từ FuzzyConfig (runtime, không hardcode) ───────────────────
inline float mem_soil_low (float s) { auto &p=fuzzyConfig.params; return triMF(s,p.soil_low.a, p.soil_low.b, p.soil_low.c);  }
inline float mem_soil_med (float s) { auto &p=fuzzyConfig.params; return triMF(s,p.soil_med.a, p.soil_med.b, p.soil_med.c);  }
inline float mem_soil_high(float s) { auto &p=fuzzyConfig.params; return triMF(s,p.soil_hi.a,  p.soil_hi.b,  p.soil_hi.c);   }

inline float mem_temp_hot (float t) { auto &p=fuzzyConfig.params; return triMF(t,p.temp_hot.a, p.temp_hot.b, p.temp_hot.c);  }
inline float mem_temp_med (float t) { auto &p=fuzzyConfig.params; return triMF(t,p.temp_med.a, p.temp_med.b, p.temp_med.c);  }
inline float mem_temp_cold(float t) { auto &p=fuzzyConfig.params; return triMF(t,p.temp_cold.a,p.temp_cold.b,p.temp_cold.c); }

inline float mem_hum_low  (float h) { auto &p=fuzzyConfig.params; return triMF(h,p.hum_low.a,  p.hum_low.b,  p.hum_low.c);  }
inline float mem_hum_med  (float h) { auto &p=fuzzyConfig.params; return triMF(h,p.hum_med.a,  p.hum_med.b,  p.hum_med.c);  }
inline float mem_hum_high (float h) { auto &p=fuzzyConfig.params; return triMF(h,p.hum_hi.a,   p.hum_hi.b,   p.hum_hi.c);   }

// ─── Defuzzify ────────────────────────────────────────────────────────────────
inline float fuzzy_defuzzify(float temp, float hum, float soil) {
    float t[3] = { mem_temp_hot(temp),  mem_temp_med(temp),  mem_temp_cold(temp)  };
    float h[3] = { mem_hum_low(hum),    mem_hum_med(hum),    mem_hum_high(hum)    };
    float s[3] = { mem_soil_low(soil),  mem_soil_med(soil),  mem_soil_high(soil)  };

    float wHigh = 0, wLow = 0, wNone = 0;

    for (int m = 0; m < 3; m++)
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) {
                float firing = min(min(t[j], h[i]), s[m]);
                int   output = fuzzyConfig.params.rules[m][i][j];
                if      (output == WATER_HIGH) wHigh = max(wHigh, firing);
                else if (output == WATER_LOW)  wLow  = max(wLow,  firing);
                else                           wNone = max(wNone, firing);
            }

    float total = wHigh + wLow + wNone;
    if (total == 0.0f) return 0.0f;
    return (wHigh * 100.0f + wLow * 50.0f) / total;
}