#pragma once
#include <Arduino.h>
#include <Preferences.h>

/*
 * FuzzyConfig — lưu/đọc tham số fuzzy từ NVS (Non-Volatile Storage)
 *
 * Cấu trúc NVS namespace "fuzzy":
 *   Hàm membership: "s_low_a/b/c", "s_med_a/b/c", "s_hi_a/b/c"
 *                   "h_low_a/b/c", "h_med_a/b/c", "h_hi_a/b/c"
 *                   "t_hot_a/b/c", "t_med_a/b/c", "t_cold_a/b/c"
 *   Ma trận luật  : "r_L_00".."r_L_22", "r_M_00".."r_M_22", "r_H_00".."r_H_22"
 */

struct MFParams {
    float a, b, c;
};

struct FuzzyParams {
    // Độ ẩm đất
    MFParams soil_low, soil_med, soil_hi;
    // Độ ẩm không khí
    MFParams hum_low,  hum_med,  hum_hi;
    // Nhiệt độ
    MFParams temp_hot, temp_med, temp_cold;
    // Ma trận luật [soil:0-2][hum:0-2][temp:0-2]
    // 0=NONE, 1=LOW, 2=HIGH
    int rules[3][3][3];
};

class FuzzyConfig {
public:
    FuzzyParams params;

    void begin() {
        loadDefaults();
        _prefs.begin("fuzzy", false);
        // Đọc từ NVS, nếu chưa có thì giữ default
        if (_prefs.isKey("s_low_a")) {
            loadFromNVS();
            Serial.println("[Config] Da tai tu NVS.");
        } else {
            Serial.println("[Config] Dung gia tri mac dinh.");
        }
        _prefs.end();
    }

    // Gọi sau khi nhận BLE — parse chuỗi JSON đơn giản và lưu NVS
    // Định dạng nhận: "MF:s_low_a:20.0" hoặc "RL:1:2:0:2" (soil,hum,temp,val)
    bool applyCommand(const String &cmd) {
        if (cmd.startsWith("MF:")) {
            return _parseMF(cmd.substring(3));
        } else if (cmd.startsWith("RL:")) {
            return _parseRule(cmd.substring(3));
        } else if (cmd == "RESET") {
            loadDefaults();
            saveToNVS();
            Serial.println("[Config] Reset ve mac dinh.");
            return true;
        }
        return false;
    }

    void saveToNVS() {
        _prefs.begin("fuzzy", false);
        // MF soil
        _saveTriple("s_low", params.soil_low);
        _saveTriple("s_med", params.soil_med);
        _saveTriple("s_hi",  params.soil_hi);
        // MF hum
        _saveTriple("h_low", params.hum_low);
        _saveTriple("h_med", params.hum_med);
        _saveTriple("h_hi",  params.hum_hi);
        // MF temp
        _saveTriple("t_hot",  params.temp_hot);
        _saveTriple("t_med",  params.temp_med);
        _saveTriple("t_cold", params.temp_cold);
        // Rules
        char key[10];
        const char *pfx[3] = {"rL","rM","rH"};
        for (int s = 0; s < 3; s++)
            for (int h = 0; h < 3; h++)
                for (int t = 0; t < 3; t++) {
                    snprintf(key, sizeof(key), "%s_%d%d", pfx[s], h, t);
                    _prefs.putInt(key, params.rules[s][h][t]);
                }
        _prefs.end();
        Serial.println("[Config] Da luu NVS.");
    }

    // Sinh chuỗi JSON đầy đủ để gửi lại web khi kết nối (đọc config hiện tại)
    String toJSON() {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\"mf\":{"
            "\"sl\":[%.1f,%.1f,%.1f],"
            "\"sm\":[%.1f,%.1f,%.1f],"
            "\"sh\":[%.1f,%.1f,%.1f],"
            "\"hl\":[%.1f,%.1f,%.1f],"
            "\"hm\":[%.1f,%.1f,%.1f],"
            "\"hh\":[%.1f,%.1f,%.1f],"
            "\"th\":[%.1f,%.1f,%.1f],"
            "\"tm\":[%.1f,%.1f,%.1f],"
            "\"tc\":[%.1f,%.1f,%.1f]},"
            "\"rl\":[[%d,%d,%d],[%d,%d,%d],[%d,%d,%d]],"
            "\"rm\":[[%d,%d,%d],[%d,%d,%d],[%d,%d,%d]],"
            "\"rh\":[[%d,%d,%d],[%d,%d,%d],[%d,%d,%d]]}",
            params.soil_low.a,  params.soil_low.b,  params.soil_low.c,
            params.soil_med.a,  params.soil_med.b,  params.soil_med.c,
            params.soil_hi.a,   params.soil_hi.b,   params.soil_hi.c,
            params.hum_low.a,   params.hum_low.b,   params.hum_low.c,
            params.hum_med.a,   params.hum_med.b,   params.hum_med.c,
            params.hum_hi.a,    params.hum_hi.b,    params.hum_hi.c,
            params.temp_hot.a,  params.temp_hot.b,  params.temp_hot.c,
            params.temp_med.a,  params.temp_med.b,  params.temp_med.c,
            params.temp_cold.a, params.temp_cold.b, params.temp_cold.c,
            params.rules[0][0][0], params.rules[0][0][1], params.rules[0][0][2],
            params.rules[0][1][0], params.rules[0][1][1], params.rules[0][1][2],
            params.rules[0][2][0], params.rules[0][2][1], params.rules[0][2][2],
            params.rules[1][0][0], params.rules[1][0][1], params.rules[1][0][2],
            params.rules[1][1][0], params.rules[1][1][1], params.rules[1][1][2],
            params.rules[1][2][0], params.rules[1][2][1], params.rules[1][2][2],
            params.rules[2][0][0], params.rules[2][0][1], params.rules[2][0][2],
            params.rules[2][1][0], params.rules[2][1][1], params.rules[2][1][2],
            params.rules[2][2][0], params.rules[2][2][1], params.rules[2][2][2]
        );
        return String(buf);
    }

    void loadDefaults() {
        params.soil_low  = {-1,  0, 30};
        params.soil_med  = {25, 32, 40};
        params.soil_hi   = {35, 100, 101};
        params.hum_low   = {-1,  0, 60};
        params.hum_med   = {40, 60, 80};
        params.hum_hi    = {60, 100, 101};
        params.temp_hot  = {30, 50, 51};
        params.temp_med  = {15, 25, 30};
        params.temp_cold = {-1,  0, 20};
        int def[3][3][3] = {
            {{2,2,1},{2,1,1},{1,1,1}},
            {{2,1,1},{1,1,0},{1,0,0}},
            {{0,0,0},{0,0,0},{0,0,0}}
        };
        memcpy(params.rules, def, sizeof(def));
    }

private:
    Preferences _prefs;

    void _saveTriple(const char *prefix, const MFParams &m) {
        char k[10];
        snprintf(k,sizeof(k),"%s_a",prefix); _prefs.putFloat(k, m.a);
        snprintf(k,sizeof(k),"%s_b",prefix); _prefs.putFloat(k, m.b);
        snprintf(k,sizeof(k),"%s_c",prefix); _prefs.putFloat(k, m.c);
    }
    MFParams _loadTriple(const char *prefix) {
        char k[10]; MFParams m;
        snprintf(k,sizeof(k),"%s_a",prefix); m.a = _prefs.getFloat(k, 0);
        snprintf(k,sizeof(k),"%s_b",prefix); m.b = _prefs.getFloat(k, 0);
        snprintf(k,sizeof(k),"%s_c",prefix); m.c = _prefs.getFloat(k, 0);
        return m;
    }
    void loadFromNVS() {
        params.soil_low  = _loadTriple("s_low");
        params.soil_med  = _loadTriple("s_med");
        params.soil_hi   = _loadTriple("s_hi");
        params.hum_low   = _loadTriple("h_low");
        params.hum_med   = _loadTriple("h_med");
        params.hum_hi    = _loadTriple("h_hi");
        params.temp_hot  = _loadTriple("t_hot");
        params.temp_med  = _loadTriple("t_med");
        params.temp_cold = _loadTriple("t_cold");
        char key[10];
        const char *pfx[3] = {"rL","rM","rH"};
        for (int s = 0; s < 3; s++)
            for (int h = 0; h < 3; h++)
                for (int t = 0; t < 3; t++) {
                    snprintf(key, sizeof(key), "%s_%d%d", pfx[s], h, t);
                    params.rules[s][h][t] = _prefs.getInt(key, 0);
                }
    }

    // "s_low_a:20.5"  →  cập nhật params.soil_low.a = 20.5 rồi lưu
    bool _parseMF(const String &s) {
        int colon = s.indexOf(':');
        if (colon < 0) return false;
        String key = s.substring(0, colon);
        float val  = s.substring(colon+1).toFloat();

        MFParams *mf = nullptr;
        int field = 0; // 0=a, 1=b, 2=c

        struct { const char *k; MFParams *p; } map[] = {
            {"s_low",&params.soil_low},{"s_med",&params.soil_med},{"s_hi",&params.soil_hi},
            {"h_low",&params.hum_low}, {"h_med",&params.hum_med}, {"h_hi",&params.hum_hi},
            {"t_hot",&params.temp_hot},{"t_med",&params.temp_med},{"t_cold",&params.temp_cold}
        };
        for (auto &e : map) {
            if (key.startsWith(e.k)) {
                mf = e.p;
                char last = key.charAt(key.length()-1);
                field = (last=='a') ? 0 : (last=='b') ? 1 : 2;
                break;
            }
        }
        if (!mf) return false;
        if (field==0) mf->a = val;
        else if (field==1) mf->b = val;
        else mf->c = val;
        saveToNVS();
        Serial.printf("[Config] MF %s = %.1f\n", key.c_str(), val);
        return true;
    }

    // "0:1:2:1"  →  rules[0][1][2] = 1
    bool _parseRule(const String &s) {
        int p1=s.indexOf(':'), p2=s.indexOf(':',p1+1), p3=s.indexOf(':',p2+1);
        if (p1<0||p2<0||p3<0) return false;
        int si  = s.substring(0,p1).toInt();
        int hi  = s.substring(p1+1,p2).toInt();
        int ti  = s.substring(p2+1,p3).toInt();
        int val = s.substring(p3+1).toInt();
        if (si<0||si>2||hi<0||hi>2||ti<0||ti>2||val<0||val>2) return false;
        params.rules[si][hi][ti] = val;
        saveToNVS();
        Serial.printf("[Config] Rule[%d][%d][%d] = %d\n", si, hi, ti, val);
        return true;
    }
};

extern FuzzyConfig fuzzyConfig;