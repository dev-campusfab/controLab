#include "wifi_manager.h"
#include <WiFi.h>

namespace wifi_manager {

bool connect(const String& ssid, const String& password, uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) {
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

} // namespace wifi_manager
