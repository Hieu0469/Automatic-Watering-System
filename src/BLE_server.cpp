/*
 * BLE Server dùng ESP32 Arduino built-in
 * Header BLEDevice.h có sẵn trong esp32 arduino core — không cần cài thêm
 */
#include "BLE_server.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"

static BLEServer         *pServer         = nullptr;
static BLECharacteristic *pCharacteristic = nullptr;
static bool               deviceConnected  = false;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pSvr) override {
        deviceConnected = true;
        Serial.println("[BLE] Client da ket noi!");
    }
    void onDisconnect(BLEServer *pSvr) override {
        deviceConnected = false;
        Serial.println("[BLE] Client ngat ket noi. Phat song lai...");
        delay(500);
        pSvr->startAdvertising();
    }
};

void ble_server_init() {
    BLEDevice::init("ESP32_TuoiTieu");

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    // BLE2902 descriptor bắt buộc để Web Bluetooth nhận notification
    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->setMinPreferred(0x06);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Server da khoi tao, dang phat song...");
}

void ble_send_sensor_data(float temp, float hum, float soil) {
    if (!deviceConnected || !pCharacteristic) return;

    char buf[50];
    snprintf(buf, sizeof(buf), "%.1f,%.1f,%.1f", temp, hum, soil);
    pCharacteristic->setValue(buf);
    pCharacteristic->notify();
    Serial.printf("[BLE] Da gui: %s\n", buf);
}

bool ble_is_connected() { return deviceConnected; }
