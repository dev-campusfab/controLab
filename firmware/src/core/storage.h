#pragma once
#include <Arduino.h>

// Enveloppe autour de Preferences (NVS) pour tout ce que le module doit
// retenir entre deux reboots : identité, creds WiFi, config serveur, secret.
namespace storage {

void begin();

// Identifiant stable du module, dérivé de la MAC au premier boot puis
// mis en cache en NVS (format "s3-xxxxxx").
String getOrCreateModuleId();

bool isBound();
void setBound(bool bound);

void setWifiCredentials(const String& ssid, const String& password);
String getWifiSsid();
String getWifiPassword();
bool hasWifiCredentials();

void setApiBaseUrl(const String& url);
String getApiBaseUrl();

void setModuleSecret(const String& secret);
String getModuleSecret();

void factoryReset();

} // namespace storage
