#include "ble_provisioning.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#include "../../include/ble_protocol.h"
#include "../../include/board_config.h"
#include "../core/storage.h"
#include "../core/state_machine.h"

namespace ble_provisioning {

static NimBLEServer* server = nullptr;
static NimBLECharacteristic* chDeviceInfo = nullptr;
static NimBLECharacteristic* chWifiConfig = nullptr;
static NimBLECharacteristic* chServerConfig = nullptr;
static NimBLECharacteristic* chStatus = nullptr;
static NimBLECharacteristic* chCommand = nullptr;

static volatile bool pendingBindFlag = false;
static PendingBindRequest pendingBindData;

static volatile bool pendingFactoryReset = false;
static volatile bool pendingReboot = false;

static String buildDeviceInfoJson() {
    JsonDocument doc;
    doc["moduleId"] = storage::getOrCreateModuleId();
    doc["hwVariant"] = BOARD_VARIANT_NAME;
    doc["fwVersion"] = FW_VERSION;
    JsonObject caps = doc["capabilities"].to<JsonObject>();
    caps["screen"] = (bool)HAS_SCREEN;
    caps["ledRing"] = (bool)(HAS_LED_RING || HAS_STATUS_LED);
    caps["battery"] = (bool)HAS_BATTERY_MON;
    caps["nfc"] = (bool)HAS_NFC;
    doc["bound"] = storage::isBound();

    String out;
    serializeJson(doc, out);
    return out;
}

static String buildStatusJson() {
    JsonDocument doc;
    doc["state"] = core::stateToString(core::ctx.state);
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["serverReachable"] = core::ctx.state == core::State::RUNNING;
    const char* err = core::errorToString(core::ctx.error);
    if (strcmp(err, "null") == 0) {
        doc["error"] = nullptr;
    } else {
        doc["error"] = err;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

void notifyStatus() {
    if (!chStatus) return;
    String json = buildStatusJson();
    chStatus->setValue(json);
    chStatus->notify();
}

class SetupCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
        (void)connInfo;
        std::string v = c->getValue();

        if (c == chWifiConfig) {
            JsonDocument doc;
            if (deserializeJson(doc, v)) return;
            String ssid = doc["ssid"] | "";
            String pass = doc["password"] | "";
            if (ssid.length() == 0) return;
            storage::setWifiCredentials(ssid, pass);
            return;
        }

        if (c == chServerConfig) {
            JsonDocument doc;
            if (deserializeJson(doc, v)) return;
            String apiBaseUrl = doc["apiBaseUrl"] | "";
            String bindToken = doc["bindToken"] | "";
            if (apiBaseUrl.length() == 0 || bindToken.length() == 0) return;

            storage::setApiBaseUrl(apiBaseUrl);
            pendingBindData.apiBaseUrl = apiBaseUrl;
            pendingBindData.bindToken = bindToken;
            pendingBindFlag = true;
            return;
        }

        if (c == chCommand) {
            String cmd(v.c_str());
            if (cmd == "FACTORY_RESET") {
                pendingFactoryReset = true;
            } else if (cmd == "REBOOT") {
                pendingReboot = true;
            }
            // ENTER_SETUP : no-op ici, la fenêtre de setup est déjà ouverte
            // puisque c'est la seule situation où ce callback peut tourner.
            return;
        }
    }
};

static SetupCallbacks setupCallbacks;

void begin() {
    String moduleId = storage::getOrCreateModuleId();
    String advName = String(BLE_ADV_NAME_PREFIX) + moduleId.substring(moduleId.length() - 6);

    NimBLEDevice::init(advName.c_str());
    server = NimBLEDevice::createServer();

    NimBLEService* service = server->createService(BLE_SETUP_SERVICE_UUID);

    chDeviceInfo = service->createCharacteristic(
        BLE_CHAR_DEVICE_INFO_UUID, NIMBLE_PROPERTY::READ);
    chDeviceInfo->setValue(buildDeviceInfoJson());

    chWifiConfig = service->createCharacteristic(
        BLE_CHAR_WIFI_CONFIG_UUID, NIMBLE_PROPERTY::WRITE);
    chWifiConfig->setCallbacks(&setupCallbacks);

    chServerConfig = service->createCharacteristic(
        BLE_CHAR_SERVER_CONFIG_UUID, NIMBLE_PROPERTY::WRITE);
    chServerConfig->setCallbacks(&setupCallbacks);

    chStatus = service->createCharacteristic(
        BLE_CHAR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    chStatus->setValue(buildStatusJson());

    chCommand = service->createCharacteristic(
        BLE_CHAR_COMMAND_UUID, NIMBLE_PROPERTY::WRITE);
    chCommand->setCallbacks(&setupCallbacks);

    // Le paquet d'advertising BLE classique est limité à 31 octets : l'UUID
    // de service 128 bits (18 octets avec son en-tête) + le nom complet
    // (~25 octets) dépassent cette limite ensemble ("Data length exceeded").
    // On sépare : l'UUID (nécessaire au filtre navigator.bluetooth.requestDevice
    // côté site) dans le paquet principal, le nom (juste pour l'affichage
    // humain dans le picker du navigateur) dans la scan response.
    NimBLEAdvertisementData advData;
    advData.addServiceUUID(BLE_SETUP_SERVICE_UUID);

    NimBLEAdvertisementData scanRespData;
    scanRespData.setName(std::string(advName.c_str()));

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->setAdvertisementData(advData);
    adv->setScanResponseData(scanRespData);
    adv->enableScanResponse(true);
    adv->start();
}

void stop() {
    NimBLEDevice::getAdvertising()->stop();
    NimBLEDevice::deinit(true);
    server = nullptr;
    chDeviceInfo = chWifiConfig = chServerConfig = chStatus = chCommand = nullptr;
}

bool isPendingBind() {
    return pendingBindFlag;
}

PendingBindRequest consumePendingBind() {
    pendingBindFlag = false;
    return pendingBindData;
}

void loopTask() {
    if (pendingReboot) {
        delay(200);
        ESP.restart();
    }
    if (pendingFactoryReset) {
        storage::factoryReset();
        delay(200);
        ESP.restart();
    }
}

} // namespace ble_provisioning
