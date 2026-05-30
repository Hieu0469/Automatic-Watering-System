/*
 * BLE Server — 2 characteristics:
 *  CHAR_SENSOR  (NOTIFY)       : ESP32 → Web  "temp,hum,soil"
 *  CHAR_CONFIG  (WRITE/NOTIFY) : Web → ESP32  lệnh cấu hình  |  ESP32 → Web  JSON config
 *
 * Khi Web kết nối, ESP32 tự động gửi config hiện tại qua CHAR_CONFIG notify.
 * Web ghi lệnh vào CHAR_CONFIG write để thay đổi tham số.
 */
#include "BLE_server.h"
#include "FuzzyConfig.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID     "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CHAR_SENSOR_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define CHAR_CONFIG_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"

static BLEServer         *pServer      = nullptr;
static BLECharacteristic *pSensorChar  = nullptr;
static BLECharacteristic *pConfigChar  = nullptr;
static bool               deviceConnected = false;
static bool               configRequested = false;  // gửi config sau khi connect

// ─── Callback nhận lệnh từ Web ───────────────────────────────────────────────
class ConfigCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) override {
        String cmd = pChar->getValue().c_str();
        cmd.trim();
        Serial.printf("[BLE] Nhan lenh: %s\n", cmd.c_str());

        if (fuzzyConfig.applyCommand(cmd)) {
            // Phản hồi ACK về web
            String ack = "ACK:" + cmd.substring(0, 20);
            pChar->setValue(ack.c_str());
            pChar->notify();
        } else if (cmd == "GET_CONFIG") {
            // Web yêu cầu đọc config
            configRequested = true;
        } else {
            pChar->setValue("ERR:unknown_cmd");
            pChar->notify();
        }
    }
};

// ─── Callback kết nối ────────────────────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pSvr) override {
        deviceConnected  = true;
        configRequested  = true;   // Tự động gửi config khi web vừa kết nối
        Serial.println("[BLE] Client da ket noi!");
    }
    void onDisconnect(BLEServer *pSvr) override {
        deviceConnected = false;
        Serial.println("[BLE] Client ngat ket noi. Phat song lai...");
        delay(500);
        pSvr->startAdvertising();
    }
};

// ─── Init ─────────────────────────────────────────────────────────────────────
void ble_server_init() {
    BLEDevice::init("ESP32_TuoiTieu");

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Tăng MTU để gửi JSON config lớn hơn 20 bytes
    BLEDevice::setMTU(185);

    BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID), 30);

    // Char 1: sensor data notify
    pSensorChar = pService->createCharacteristic(
        CHAR_SENSOR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pSensorChar->addDescriptor(new BLE2902());

    // Char 2: config write + notify
    pConfigChar = pService->createCharacteristic(
        CHAR_CONFIG_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pConfigChar->addDescriptor(new BLE2902());
    pConfigChar->setCallbacks(new ConfigCallbacks());

    pService->start();

    BLEAdvertising *pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->setMinPreferred(0x06);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Server da khoi tao, dang phat song...");
}

// ─── Gửi dữ liệu cảm biến ────────────────────────────────────────────────────
void ble_send_sensor_data(float temp, float hum, float soil) {
    if (!deviceConnected || !pSensorChar) return;
    char buf[50];
    snprintf(buf, sizeof(buf), "%.1f,%.1f,%.1f", temp, hum, soil);
    pSensorChar->setValue(buf);
    pSensorChar->notify();
}

// ─── Gửi config JSON về web (gọi từ loop() khi configRequested) ──────────────
void ble_send_config(const String &json) {
    if (!deviceConnected || !pConfigChar) return;
    // BLE MTU ~185 byte — JSON ~480 byte → cần chia gói
    const int CHUNK = 180;
    int len = json.length();
    for (int i = 0; i < len; i += CHUNK) {
        String chunk = json.substring(i, min(i + CHUNK, len));
        // Đánh dấu gói: "CFG_S:" (start), "CFG_M:" (middle), "CFG_E:" (end)
        String tagged;
        if (i == 0 && (i + CHUNK) >= len)       tagged = "CFG_SE:" + chunk;
        else if (i == 0)                          tagged = "CFG_S:"  + chunk;
        else if ((i + CHUNK) >= len)              tagged = "CFG_E:"  + chunk;
        else                                      tagged = "CFG_M:"  + chunk;
        pConfigChar->setValue(tagged.c_str());
        pConfigChar->notify();
        delay(30);  // BLE cần khoảng trống giữa các gói
    }
    Serial.println("[BLE] Da gui config JSON.");
}

bool ble_is_connected() { return deviceConnected; }
bool ble_config_requested() { return configRequested; }
void ble_clear_config_request() { configRequested = false; }