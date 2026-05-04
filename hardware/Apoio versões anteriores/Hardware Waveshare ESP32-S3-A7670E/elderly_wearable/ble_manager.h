#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"

// ============================================================================
// BLE — Companion app communication & location beacon
// ============================================================================

// Custom service / characteristic UUIDs
#define SERVICE_UUID        "12345678-1234-5678-1234-123456789abc"
#define CHAR_LOCATION_UUID  "12345678-1234-5678-1234-123456789abd"
#define CHAR_STATUS_UUID    "12345678-1234-5678-1234-123456789abe"
#define CHAR_COMMAND_UUID   "12345678-1234-5678-1234-123456789abf"

class BLEManager {
public:
    BLEServer*         pServer         = nullptr;
    BLECharacteristic* pLocationChar   = nullptr;
    BLECharacteristic* pStatusChar     = nullptr;
    BLECharacteristic* pCommandChar    = nullptr;
    bool               deviceConnected = false;

    // Callback class to track connections
    class ServerCallbacks : public BLEServerCallbacks {
        BLEManager* mgr;
    public:
        ServerCallbacks(BLEManager* m) : mgr(m) {}
        void onConnect(BLEServer* s)    override { mgr->deviceConnected = true;  Serial.println("[BLE] Client connected"); }
        void onDisconnect(BLEServer* s) override { mgr->deviceConnected = false; Serial.println("[BLE] Client disconnected"); }
    };

    // Callback for incoming commands from companion app
    class CommandCallback : public BLECharacteristicCallbacks {
    public:
        String lastCommand;
        void onWrite(BLECharacteristic* pChar) override {
            lastCommand = pChar->getValue().c_str();
            Serial.printf("[BLE] Command received: %s\n", lastCommand.c_str());
        }
    };

    CommandCallback cmdCallback;

    void begin() {
        BLEDevice::init(BLE_DEVICE_NAME);
        pServer = BLEDevice::createServer();
        pServer->setCallbacks(new ServerCallbacks(this));

        BLEService* pService = pServer->createService(SERVICE_UUID);

        // Location characteristic (notify — push GPS coords to phone)
        pLocationChar = pService->createCharacteristic(
            CHAR_LOCATION_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        pLocationChar->addDescriptor(new BLE2902());

        // Status characteristic (notify — battery, signal, etc.)
        pStatusChar = pService->createCharacteristic(
            CHAR_STATUS_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        pStatusChar->addDescriptor(new BLE2902());

        // Command characteristic (write — phone sends commands)
        pCommandChar = pService->createCharacteristic(
            CHAR_COMMAND_UUID,
            BLECharacteristic::PROPERTY_WRITE
        );
        pCommandChar->setCallbacks(&cmdCallback);

        pService->start();

        BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        pAdvertising->start();

        Serial.println("[BLE] Advertising started");
    }

    void stop() {
        BLEDevice::deinit(false);
        Serial.println("[BLE] Stopped");
    }

    // Push GPS location to connected phone
    void notifyLocation(double lat, double lon, float alt) {
        if (!deviceConnected) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.6f,%.6f,%.1f", lat, lon, alt);
        pLocationChar->setValue(buf);
        pLocationChar->notify();
    }

    // Push device status to connected phone
    void notifyStatus(int battPct, int signal, const char* state) {
        if (!deviceConnected) return;
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"batt\":%d,\"sig\":%d,\"state\":\"%s\"}", battPct, signal, state);
        pStatusChar->setValue(buf);
        pStatusChar->notify();
    }
};

#endif // BLE_MANAGER_H
