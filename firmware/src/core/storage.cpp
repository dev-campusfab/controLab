#include "storage.h"
#include <Preferences.h>
#include <esp_mac.h>

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

    // esp_read_mac() lit directement l'eFuse : contrairement à
    // WiFi.macAddress(), elle ne dépend pas du driver WiFi déjà initialisé.
    // Appelée ici (très tôt au boot, avant tout WiFi.mode()), WiFi.macAddress()
    // peut renvoyer une adresse à zéro sur certains cores si le driver n'a
    // jamais été démarré — bug vécu en test : le moduleId "s3-000000" en
    // résultant reste ensuite caché en NVS pour toujours (un factory reset
    // ne le recalcule pas, par design), rendant le module impossible à
    // réappairer correctement. D'où l'importance de ne jamais mettre en
    // cache un MAC manifestement invalide (tout à zéro).
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    bool allZero = (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0;
    if (allZero) {
        // Mieux vaut ne rien mettre en cache et réessayer au prochain appel
        // (prochain reboot) qu'un id bidon figé définitivement.
        return "s3-unknown";
    }

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
