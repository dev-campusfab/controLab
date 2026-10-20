#include "storage.h"
#include <Preferences.h>
#include <WiFi.h>

namespace storage {

static Preferences prefs;
static const char* NAMESPACE = "controlab";

void begin() {
    prefs.begin(NAMESPACE, false);
}

String getOrCreateModuleId() {
    String cached = prefs.getString("moduleId", "");
    if (cached.length() > 0) {
        return cached;
    }

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[16];
    snprintf(buf, sizeof(buf), "s3-%02x%02x%02x", mac[3], mac[4], mac[5]);
    String id(buf);
    prefs.putString("moduleId", id);
    return id;
}

bool isBound() {
    return prefs.getBool("bound", false);
}

void setBound(bool bound) {
    prefs.putBool("bound", bound);
}

void setWifiCredentials(const String& ssid, const String& password) {
    prefs.putString("wifiSsid", ssid);
    prefs.putString("wifiPass", password);
}

String getWifiSsid() {
    return prefs.getString("wifiSsid", "");
}

String getWifiPassword() {
    return prefs.getString("wifiPass", "");
}

bool hasWifiCredentials() {
    return getWifiSsid().length() > 0;
}

void setApiBaseUrl(const String& url) {
    prefs.putString("apiBaseUrl", url);
}

String getApiBaseUrl() {
    return prefs.getString("apiBaseUrl", "");
}

void setModuleSecret(const String& secret) {
    prefs.putString("moduleSecret", secret);
}

String getModuleSecret() {
    return prefs.getString("moduleSecret", "");
}

void factoryReset() {
    // Le moduleId n'est pas effacé : il est de toute façon redérivé de la
    // MAC de façon identique, et le conserver évite de perturber un futur
    // nouveau bind qui référencerait l'ancien id côté site par erreur.
    prefs.remove("bound");
    prefs.remove("wifiSsid");
    prefs.remove("wifiPass");
    prefs.remove("apiBaseUrl");
    prefs.remove("moduleSecret");
}

} // namespace storage
