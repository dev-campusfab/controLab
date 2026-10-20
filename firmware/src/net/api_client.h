#pragma once
#include <Arduino.h>

// Client HTTP minimal pour les 3 appels décrits dans docs/API_CONTRACT.md.
// Toutes les fonctions sont bloquantes (timeout ~5s) : à appeler depuis la
// boucle principale, pas depuis un callback BLE (qui tourne sur le task NimBLE).
namespace api_client {

struct BindResult {
    bool ok = false;
    String moduleSecret;
    String errorCode; // "invalid_token", "expired_token", "already_bound", "network"
};

struct ScanResult {
    bool ok = false;
    String result;       // "granted" | "denied" | "unknown_card" | "error"
    String message;
    String ledColorHex;  // ex: "#00c853", vide si absent
    String displayText;
    bool beep = false;
};

struct HeartbeatResult {
    bool ok = false;
    String command;          // "ENTER_SETUP" | "REBOOT" | "FACTORY_RESET" | ""
    uint32_t scanPollIntervalMs = 150;
};

BindResult bind(const String& apiBaseUrl, const String& moduleId, const String& bindToken);

ScanResult reportScan(const String& apiBaseUrl, const String& moduleId,
                       const String& moduleSecret, const String& cardUid);

HeartbeatResult heartbeat(const String& apiBaseUrl, const String& moduleId,
                          const String& moduleSecret, float batteryPercent /* -1 si N/A */);

} // namespace api_client
