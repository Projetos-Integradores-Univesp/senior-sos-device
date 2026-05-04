#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

// ============================================================================
// BLE Manager — XIAO ESP32S3
// Sem GPS — notifyLocation() omitido (sem localização disponível)
// Sinal reportado como RSSI do WiFi
// ============================================================================

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"
#include "espnow_mirror.h"

#define SERVICE_UUID        "12345678-1234-5678-1234-123456789abc"
#define CHAR_STATUS_UUID    "12345678-1234-5678-1234-123456789abe"
#define CHAR_COMMAND_UUID   "12345678-1234-5678-1234-123456789abf"

class BLEManager {
public:
    BLEServer*         pServer       = nullptr;
    BLECharacteristic* pStatusChar   = nullptr;
    BLECharacteristic* pCommandChar  = nullptr;
    bool               deviceConnected = false;

    class ServerCallbacks : public BLEServerCallbacks {
        BLEManager* mgr;
    public:
        ServerCallbacks(BLEManager* m) : mgr(m) {}
        void onConnect(BLEServer*)    override { mgr->deviceConnected = true;  out.println("[BLE] Cliente conectado"); }
        void onDisconnect(BLEServer*) override { mgr->deviceConnected = false; out.println("[BLE] Cliente desconectado"); }
    };

    class CommandCallback : public BLECharacteristicCallbacks {
    public:
        String lastCommand;
        void onWrite(BLECharacteristic* pChar) override {
            lastCommand = pChar->getValue().c_str();
            out.printf("[BLE] Comando: %s\n", lastCommand.c_str());
        }
    };

    CommandCallback cmdCallback;

    void begin() {
        BLEDevice::init(BLE_DEVICE_NAME);
        pServer = BLEDevice::createServer();
        pServer->setCallbacks(new ServerCallbacks(this));

        BLEService* pService = pServer->createService(SERVICE_UUID);

        pStatusChar = pService->createCharacteristic(
            CHAR_STATUS_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        pStatusChar->addDescriptor(new BLE2902());

        pCommandChar = pService->createCharacteristic(
            CHAR_COMMAND_UUID,
            BLECharacteristic::PROPERTY_WRITE
        );
        pCommandChar->setCallbacks(&cmdCallback);

        pService->start();

        BLEAdvertising* pAdv = BLEDevice::getAdvertising();
        pAdv->addServiceUUID(SERVICE_UUID);
        pAdv->setScanResponse(true);
        pAdv->setMinPreferred(0x06);
        pAdv->start();

        out.println("[BLE] Advertising iniciado");
    }

    void stop() {
        BLEDevice::deinit(false);
        out.println("[BLE] Parado");
    }

    void notifyStatus(int battPct, int rssi, const char* state) {
        if (!deviceConnected || !pStatusChar) return;
        char buf[80];
        snprintf(buf, sizeof(buf),
            "{\"batt\":%d,\"rssi\":%d,\"state\":\"%s\"}", battPct, rssi, state);
        pStatusChar->setValue(buf);
        pStatusChar->notify();
    }
};

#endif // BLE_MANAGER_H
