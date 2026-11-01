#include <Arduino.h>
#include "../include/board_config.h"
#include "core/storage.h"
#include "core/state_machine.h"
#include "core/hw_config.h"
#include "ble/ble_provisioning.h"
#include "net/wifi_manager.h"
#include "net/api_client.h"
#include "nfc/nfc_reader.h"
#include "ui/ui_feedback.h"

#if HAS_BATTERY_MON
static float readBatteryPercent() {
    // Pont diviseur générique : à calibrer selon le schéma réel de la
    // variante (référence, ratio du diviseur). Placeholder volontairement
    // simple tant que le matériel batterie n'est pas figé.
    int raw = analogRead(PIN_BATTERY_ADC);
    float voltage = (raw / 4095.0f) * 3.3f * 2.0f; // x2 si diviseur 1:1
    float percent = (voltage - 3.3f) / (4.2f - 3.3f) * 100.0f;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent;
}
#endif

static uint32_t lastHeartbeatMs = 0;
static const uint32_t HEARTBEAT_INTERVAL_MS = 60000;
static uint32_t scanPollIntervalMs = 150;
static uint32_t lastScanPollMs = 0;

// Factory reset matériel : appui long sur le bouton BOOT. Seul mécanisme de
// reset qui ne dépend ni du site ni d'une connexion BLE/USB active.
static const uint32_t FACTORY_RESET_HOLD_MS = 5000;
static bool buttonHeld = false;
static uint32_t buttonPressStartMs = 0;
static uint32_t lastHoldUiUpdateMs = 0;

static void onStateChanged() {
    ui_feedback::showState(core::ctx.state, core::ctx.error);
    ble_provisioning::notifyStatus();
}

static void handleFactoryResetButton() {
    // Bouton BOOT, actif à l'état bas (pull-up interne).
    bool pressed = digitalRead(PIN_SETUP_BUTTON) == LOW;

    if (!pressed) {
        if (buttonHeld) {
            buttonHeld = false;
            // Appui relâché avant le seuil : on restaure l'affichage normal.
            ui_feedback::showState(core::ctx.state, core::ctx.error);
        }
        return;
    }

    if (!buttonHeld) {
        buttonHeld = true;
        buttonPressStartMs = millis();
    }

    uint32_t held = millis() - buttonPressStartMs;
    if (held >= FACTORY_RESET_HOLD_MS) {
        storage::factoryReset();
        delay(200);
        ESP.restart();
    }

    if (millis() - lastHoldUiUpdateMs >= 100) {
        lastHoldUiUpdateMs = millis();
        ui_feedback::showFactoryResetHold((float)held / FACTORY_RESET_HOLD_MS);
    }
}

static void enterSetupMode() {
    core::setState(core::State::SETUP);
    ble_provisioning::begin();
}

// Récupère GET /modules/{id}/hw-config (voir docs/API_CONTRACT.md), applique
// la config reçue (validée et persistée en NVS par hw_config) puis
// réinitialise le NFC et l'anneau de LEDs en conséquence. Appelé une fois à
// l'entrée en RUNNING, et à nouveau si un heartbeat répond RELOAD_CONFIG.
// En cas d'échec réseau, on réapplique simplement la config déjà connue
// (NVS ou valeurs compilées) : le module reste utilisable hors-ligne.
static void syncHwConfig() {
    String apiBaseUrl = storage::getApiBaseUrl();
    String moduleId = storage::getOrCreateModuleId();
    String secret = storage::getModuleSecret();

    api_client::HwConfigResult res = api_client::getHwConfig(apiBaseUrl, moduleId, secret);
    if (res.ok) {
        hw_config::applyFromJson(res.json);
    }

    nfc_reader::begin();
    ui_feedback::reloadLedConfig();
}

static void attemptBindAndRun(const String& apiBaseUrl, const String& bindToken) {
    core::setState(core::State::CONNECTING);
    String ssid = storage::getWifiSsid();
    String pass = storage::getWifiPassword();
    Serial.printf("[WiFi] Tentative de connexion a ssid=\"%s\"\n", ssid.c_str());
    wifi_manager::debugScanAndPrint();

    if (!wifi_manager::connect(ssid, pass)) {
        core::setError(core::ErrorCode::WIFI_TIMEOUT);
        return;
    }

    core::setState(core::State::BINDING);
    String moduleId = storage::getOrCreateModuleId();
    api_client::BindResult res = api_client::bind(apiBaseUrl, moduleId, bindToken);

    if (!res.ok) {
        if (res.errorCode == "invalid_token") {
            core::setError(core::ErrorCode::BIND_TOKEN_INVALID);
        } else if (res.errorCode == "expired_token") {
            core::setError(core::ErrorCode::BIND_TOKEN_EXPIRED);
        } else {
            core::setError(core::ErrorCode::SERVER_UNREACHABLE);
        }
        return;
    }

    storage::setModuleSecret(res.moduleSecret);
    storage::setBound(true);
    core::setState(core::State::RUNNING);
    ble_provisioning::stop();
    syncHwConfig();
}

void setup() {
    Serial.begin(115200);
    storage::begin();
    // Charge la config NVS (ou les valeurs compilées de board_config.h si
    // rien n'a encore été synchronisé) avant tout ce qui en dépend
    // (ui_feedback::begin() construit l'anneau de LEDs à partir de ça).
    hw_config::begin();
    ui_feedback::begin();
    pinMode(PIN_SETUP_BUTTON, INPUT_PULLUP);

    // Confirmation visuelle de boot, affichée avant tout le reste (BLE,
    // WiFi...) pour que ce soit visible même si la suite plante vite.
    ui_feedback::showBootOk();
    delay(3000);

    core::onStateChangeCallback(onStateChanged);

    if (!storage::isBound() || !storage::hasWifiCredentials()) {
        enterSetupMode();
        return;
    }

    // Déjà bound : tentative de reconnexion directe, sans repasser par BLE.
    core::setState(core::State::CONNECTING);
    if (wifi_manager::connect(storage::getWifiSsid(), storage::getWifiPassword())) {
        core::setState(core::State::RUNNING);
        syncHwConfig();
    } else {
        core::setError(core::ErrorCode::WIFI_TIMEOUT);
        // On ouvre quand même le BLE pour permettre une reconfiguration WiFi
        // depuis le site sans devoir faire un factory reset.
        enterSetupMode();
    }
}

static void runningLoop() {
    uint32_t now = millis();

    if (now - lastScanPollMs >= scanPollIntervalMs) {
        lastScanPollMs = now;
        String uid;
        if (nfc_reader::poll(uid)) {
            String apiBaseUrl = storage::getApiBaseUrl();
            String moduleId = storage::getOrCreateModuleId();
            String secret = storage::getModuleSecret();
            api_client::ScanResult res = api_client::reportScan(apiBaseUrl, moduleId, secret, uid);
            ui_feedback::showScanResult(res.result, res.message, res.ledColorHex,
                                        res.displayText, res.beep);
        }
    }

    if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatMs = now;
        String apiBaseUrl = storage::getApiBaseUrl();
        String moduleId = storage::getOrCreateModuleId();
        String secret = storage::getModuleSecret();
#if HAS_BATTERY_MON
        float battery = readBatteryPercent();
#else
        float battery = -1;
#endif
        api_client::HeartbeatResult hb = api_client::heartbeat(apiBaseUrl, moduleId, secret, battery);
        if (hb.ok) {
            scanPollIntervalMs = hb.scanPollIntervalMs;
            if (hb.command == "REBOOT") {
                ESP.restart();
            } else if (hb.command == "FACTORY_RESET") {
                storage::factoryReset();
                ESP.restart();
            } else if (hb.command == "ENTER_SETUP") {
                enterSetupMode();
            } else if (hb.command == "RELOAD_CONFIG") {
                syncHwConfig();
            }
        }
    }
}

void loop() {
    handleFactoryResetButton();
    if (buttonHeld) {
        // Pendant l'appui, on n'anime/ne traite rien d'autre : l'écran et
        // les LEDs sont dédiés au retour de progression du reset.
        delay(5);
        return;
    }

    ui_feedback::tick();

    switch (core::ctx.state) {
        case core::State::SETUP:
            ble_provisioning::loopTask();
            if (ble_provisioning::isPendingBind()) {
                auto req = ble_provisioning::consumePendingBind();
                attemptBindAndRun(req.apiBaseUrl, req.bindToken);
            }
            break;

        case core::State::RUNNING:
            runningLoop();
            if (!wifi_manager::isConnected()) {
                core::setState(core::State::CONNECTING);
            }
            break;

        case core::State::CONNECTING:
            if (wifi_manager::connect(storage::getWifiSsid(), storage::getWifiPassword(), 5000)) {
                core::setState(core::State::RUNNING);
            }
            break;

        case core::State::ERROR:
            // Reste affiché ; en SETUP le BLE tourne déjà et peut recevoir
            // une nouvelle tentative de configuration depuis le site.
            ble_provisioning::loopTask();
            if (ble_provisioning::isPendingBind()) {
                auto req = ble_provisioning::consumePendingBind();
                attemptBindAndRun(req.apiBaseUrl, req.bindToken);
            }
            break;

        default:
            break;
    }

    delay(5);
}
