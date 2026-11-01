#pragma once
#include <Arduino.h>

namespace wifi_manager {

// Tentative de connexion bloquante, avec retries internes.
// Retourne true si connecté avant le timeout global.
bool connect(const String& ssid, const String& password, uint32_t timeoutMs = 20000);

bool isConnected();
void disconnect();

// Scanne et logue sur Serial les SSID visibles (debug). Bloquant (~2-3s).
void debugScanAndPrint();

} // namespace wifi_manager
