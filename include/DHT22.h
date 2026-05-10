#pragma once
/*
 * Wrapper mỏng cho Adafruit DHT library
 * Dùng thư viện Adafruit vì nó xử lý timing DHT22 bằng
 * cách tắt interrupt trong lúc đọc — ổn định nhất trên ESP32
 *
 * Cài qua platformio.ini:
 *   adafruit/DHT sensor library @ ^1.4.6
 *   adafruit/Adafruit Unified Sensor @ ^1.1.14
 */
#include <Arduino.h>
#include <DHT.h>

class DHT22Driver {
public:
    DHT22Driver(uint8_t pin) : _dht(pin, DHT22) {}

    void begin() {
        _dht.begin();
        Serial.println("[DHT22] Khoi tao xong.");
    }

    // Đọc cảm biến — trả về true nếu thành công
    // Adafruit DHT tự cache kết quả 2 giây
    bool read() {
        _temp = _dht.readTemperature();
        _hum  = _dht.readHumidity();

        if (isnan(_temp) || isnan(_hum)) {
            Serial.println("[DHT22] Doc loi — kiem tra day noi GPIO15");
            return false;
        }
        return true;
    }

    float temperature() { return _temp; }
    float humidity()    { return _hum;  }

private:
    DHT   _dht;
    float _temp = NAN;
    float _hum  = NAN;
};
