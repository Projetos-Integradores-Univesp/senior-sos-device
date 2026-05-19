#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

// ============================================================================
// BLE Manager — ElderGuard XIAO ESP32S3
// Provisionamento via GATT: SSID + senha WiFi + device ID gravados em NVS.
// ============================================================================

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>
#include "config.h"
#include "espnow_mirror.h"

class BLEManager {
public:
    String  rxSsid;
    String  rxPass;
    String  rxDeviceId;
    bool    provisionComplete = false;
    bool    resetRequested    = false;
    bool    ackReceived       = false;
    bool    deviceConnected   = false;

    // -------------------------------------------------------------------------
    // begin() — inicia BLE advertising
    //
    // ADVERTISING: usa apenas addServiceUUID() + setScanResponse(true).
    //
    // Razão: montar BLEAdvertisementData manualmente com UUID de 128 bits
    // (16 bytes) + nome (9 bytes) + overhead (4 bytes) = 29 bytes, muito perto
    // do limite de 31 bytes do pacote BLE primário. Qualquer byte extra trunca
    // o pacote e o UUID desaparece silenciosamente. A biblioteca ESP32 BLE
    // Arduino coloca o UUID no pacote primário e o nome no scan response
    // automaticamente quando setScanResponse(true) — esta é a forma correta.
    // -------------------------------------------------------------------------
    void begin() {
        BLEDevice::init(BLE_ADV_NAME);

        // Definir o nome via API GAP diretamente garante que o scan response
        // carregue o nome completo independente da versão da biblioteca

        pServer = BLEDevice::createServer();
        pServer->setCallbacks(new ServerCB(this));

        BLEService* svc = pServer->createService(SERVICE_PROV_UUID);

        pSsidChar = svc->createCharacteristic(CHAR_SSID_UUID,
            BLECharacteristic::PROPERTY_WRITE);
        pSsidChar->setCallbacks(new SsidCB(this));

        pPassChar = svc->createCharacteristic(CHAR_PASS_UUID,
            BLECharacteristic::PROPERTY_WRITE);
        pPassChar->setCallbacks(new PassCB(this));

        pIdChar = svc->createCharacteristic(CHAR_ID_UUID,
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_READ);
        pIdChar->setCallbacks(new IdCB(this));
        {
            Preferences prefs;
            prefs.begin(NVS_NAMESPACE, true);
            String curId = prefs.getString(NVS_KEY_DEVICE_ID, "");
            prefs.end();
            if (curId.length() > 0) pIdChar->setValue(curId.c_str());
        }

        pStatusChar = svc->createCharacteristic(CHAR_STATUS_UUID,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY);
        pStatusChar->addDescriptor(new BLE2902());

        pCmdChar = svc->createCharacteristic(CHAR_CMD_UUID,
            BLECharacteristic::PROPERTY_WRITE);
        pCmdChar->setCallbacks(new CmdCB(this));

        svc->start();

        // Advertising: UUID no pacote primário, nome no scan response
        BLEAdvertising* adv = BLEDevice::getAdvertising();
        adv->addServiceUUID(SERVICE_PROV_UUID);
        adv->setScanResponse(true);   // nome vai no scan response automaticamente
        adv->setMinPreferred(0x06);   // iOS
        adv->start();

        out.printf("[BLE] Advertising: \"%s\"  UUID: %s\n",
                   BLE_ADV_NAME, SERVICE_PROV_UUID);
        out.println("[BLE] Aguardando app para provisionamento...");
    }

    void stop() {
        // Parar advertising antes de deinit para evitar corrupção de heap.
        // Se possível, prefer BLEDevice::getAdvertising()->stop() + ESP.restart()
        // em vez de chamar stop() + deinit() antes de restart.
        if (BLEDevice::getAdvertising() != nullptr) {
            BLEDevice::getAdvertising()->stop();
        }
        delay(200);   // aguardar callbacks pendentes
        BLEDevice::deinit(false);
        out.println("[BLE] Parado");
    }

    void notifyStatus(int battPct, int rssi, const char* state,
                      const char* deviceId = "") {
        if (!deviceConnected || !pStatusChar) return;
        char buf[128];
        snprintf(buf, sizeof(buf),
            "{\"batt\":%d,\"rssi\":%d,\"state\":\"%s\",\"id\":\"%s\"}",
            battPct, rssi, state, deviceId);
        pStatusChar->setValue(buf);
        pStatusChar->notify();
        out.printf("[BLE] Notify: %s\n", buf);
    }

    void notifyProvisioned(const char* deviceId) {
        if (!pStatusChar) return;
        char buf[96];
        snprintf(buf, sizeof(buf),
            "{\"provisioned\":true,\"id\":\"%s\"}", deviceId);
        pStatusChar->setValue(buf);
        if (deviceConnected) pStatusChar->notify();
        out.printf("[BLE] Provisionado: %s\n", buf);
    }

    void notifyError(const char* reason) {
        if (!pStatusChar) return;
        char buf[80];
        snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", reason);
        pStatusChar->setValue(buf);
        if (deviceConnected) pStatusChar->notify();
        out.printf("[BLE] Erro: %s\n", buf);
    }

    static void saveAndReboot(const char* ssid, const char* pass,
                              const char* deviceId) {
        out.printf("[BLE] Gravando NVS: ssid=%s id=%s\n", ssid, deviceId);
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putString(NVS_KEY_WIFI_SSID,  ssid);
        prefs.putString(NVS_KEY_WIFI_PASS,  pass);
        prefs.putString(NVS_KEY_DEVICE_ID,  deviceId);
        prefs.putUChar(NVS_KEY_PROVISIONED, 1);
        prefs.end();
        out.println("[BLE] NVS gravado — reiniciando...");
        delay(500);
        ESP.restart();
    }

    static void factoryReset() {
        out.println("[BLE] Factory reset — limpando NVS...");
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.clear();
        prefs.end();
        delay(500);
        ESP.restart();
    }

    static bool isProvisioned() {
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, true);
        uint8_t prov = prefs.getUChar(NVS_KEY_PROVISIONED, 0);
        prefs.end();
        return (prov == 1);
    }

    static bool loadCredentials(char* ssid, size_t ssidLen,
                                char* pass, size_t passLen,
                                char* deviceId, size_t idLen) {
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, true);
        String s = prefs.getString(NVS_KEY_WIFI_SSID, "");
        String p = prefs.getString(NVS_KEY_WIFI_PASS, "");
        String d = prefs.getString(NVS_KEY_DEVICE_ID, "");
        prefs.end();
        if (s.length() == 0 || d.length() == 0) return false;
        strncpy(ssid,     s.c_str(), ssidLen-1); ssid[ssidLen-1]   = '\0';
        strncpy(pass,     p.c_str(), passLen-1); pass[passLen-1]   = '\0';
        strncpy(deviceId, d.c_str(), idLen-1);   deviceId[idLen-1] = '\0';
        return true;
    }

private:
    BLEServer*         pServer     = nullptr;
    BLECharacteristic* pSsidChar   = nullptr;
    BLECharacteristic* pPassChar   = nullptr;
    BLECharacteristic* pIdChar     = nullptr;
    BLECharacteristic* pStatusChar = nullptr;
    BLECharacteristic* pCmdChar    = nullptr;

    struct ServerCB : public BLEServerCallbacks {
        BLEManager* m;
        ServerCB(BLEManager* mgr) : m(mgr) {}
        void onConnect(BLEServer*) override {
            m->deviceConnected = true;
            out.println("[BLE] App conectado");
        }
        void onDisconnect(BLEServer*) override {
            m->deviceConnected = false;
            out.println("[BLE] App desconectado — reiniciando advertising");
            BLEDevice::getAdvertising()->start();
        }
    };

    struct SsidCB : public BLECharacteristicCallbacks {
        BLEManager* m;
        SsidCB(BLEManager* mgr) : m(mgr) {}
        void onWrite(BLECharacteristic* c) override {
            m->rxSsid = c->getValue().c_str();
            out.printf("[BLE] SSID: \"%s\"\n", m->rxSsid.c_str());
            m->checkProvisionComplete();
        }
    };

    struct PassCB : public BLECharacteristicCallbacks {
        BLEManager* m;
        PassCB(BLEManager* mgr) : m(mgr) {}
        void onWrite(BLECharacteristic* c) override {
            m->rxPass = c->getValue().c_str();
            out.println("[BLE] Senha recebida");
            m->checkProvisionComplete();
        }
    };

    struct IdCB : public BLECharacteristicCallbacks {
        BLEManager* m;
        IdCB(BLEManager* mgr) : m(mgr) {}
        void onWrite(BLECharacteristic* c) override {
            m->rxDeviceId = c->getValue().c_str();
            out.printf("[BLE] Device ID: \"%s\"\n", m->rxDeviceId.c_str());
            m->checkProvisionComplete();
        }
    };

    struct CmdCB : public BLECharacteristicCallbacks {
        BLEManager* m;
        CmdCB(BLEManager* mgr) : m(mgr) {}
        void onWrite(BLECharacteristic* c) override {
            String cmd = c->getValue().c_str();
            out.printf("[BLE] Cmd: \"%s\"\n", cmd.c_str());
            if (cmd == "ack")   m->ackReceived    = true;
            if (cmd == "reset") m->resetRequested = true;
        }
    };

    void checkProvisionComplete() {
        if (rxSsid.length() > 0 && rxDeviceId.length() > 0) {
            provisionComplete = true;
            out.println("[BLE] Campos recebidos — aguardando sketch");
        }
    }
};

#endif // BLE_MANAGER_H
