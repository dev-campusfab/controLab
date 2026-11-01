#include "wifi_manager.h"
#include <WiFi.h>

namespace wifi_manager {

bool connect(const String& ssid, const String& password, uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    // On gère nous-mêmes les tentatives/timeouts (voir main.cpp) : désactiver
    // la reconnexion auto du driver pour éviter qu'il continue à retenter
    // l'association en arrière-plan après notre propre timeout. Important
    // avec certains points d'accès WPA3/SAE qui répondent avec un
    // "association comeback time" de plusieurs minutes (reason 208,
    // ASSOC_COMEBACK_TIME_TOO_LONG) — sans ça, le driver WiFi retente seul
    // pendant toute cette fenêtre et monopolise la radio partagée avec le
    // BLE, qui devient alors injoignable depuis le téléphone.
    WiFi.setAutoReconnect(false);
    WiFi.begin(ssid.c_str(), password.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) {
            WiFi.disconnect(true); // annule proprement la tentative en cours
            return false;
        }
        delay(250);
    }
    return true;
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void disconnect() {
    WiFi.disconnect(true);
}

void debugScanAndPrint() {
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks();
    Serial.printf("[WiFi] Scan: %d reseau(x) visible(s)\n", n);
    for (int i = 0; i < n; i++) {
        Serial.printf("  [%d] \"%s\" rssi=%d chan=%d enc=%d\n",
                      i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                      (int)WiFi.encryptionType(i));
    }
    WiFi.scanDelete();
}

} // namespace wifi_manager
