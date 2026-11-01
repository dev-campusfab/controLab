#include "hw_config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include "../../include/board_config.h"

namespace hw_config {

static Preferences prefs;
static NfcConfig currentNfc;
static LedConfig currentLed;

// Valeurs de repli pour irq/reset NFC quand le site ne les précise pas
// (champs optionnels du contrat) : celles compilées pour ce board si la
// variante a HAS_NFC, sinon -1 (aucune valeur sensée de toute façon).
#if HAS_NFC
static const int kDefaultNfcIrq = PIN_NFC_IRQ;
static const int kDefaultNfcReset = PIN_NFC_RESET;
#else
static const int kDefaultNfcIrq = -1;
static const int kDefaultNfcReset = -1;
#endif

static bool isPinReserved(int pin) {
    if (pin == PIN_SETUP_BUTTON) return true;
#if HAS_SCREEN
    if (pin == PIN_SCREEN_SCK || pin == PIN_SCREEN_MOSI || pin == PIN_SCREEN_DC ||
        pin == PIN_SCREEN_CS || pin == PIN_SCREEN_RESET) {
        return true;
    }
#endif
#if HAS_BATTERY_MON
    if (pin == PIN_BATTERY_ADC) return true;
#endif
#if HAS_STATUS_LED
    if (pin == PIN_STATUS_LED) return true;
#endif
    return false;
}

// `required=false` : -1 (absent/null côté JSON) est accepté, le champ sera
// alors rempli avec une valeur de repli par l'appelant.
static bool isValidPin(int pin, bool required) {
    if (pin < 0) return !required;
    if (pin > 48) return false; // au-delà du GPIO max d'un ESP32-S3
    if (isPinReserved(pin)) return false;
    return true;
}

static void applyBoardDefaults() {
    currentNfc = NfcConfig();
#if HAS_NFC
    currentNfc.enabled = true;
    currentNfc.interface = "i2c";
    currentNfc.sda = PIN_I2C_SDA;
    currentNfc.scl = PIN_I2C_SCL;
    currentNfc.irq = PIN_NFC_IRQ;
    currentNfc.reset = PIN_NFC_RESET;
#endif

    currentLed = LedConfig();
#if HAS_LED_RING
    currentLed.enabled = true;
    currentLed.model = "ws2812";
    currentLed.pin = PIN_LED_RING_DATA;
    currentLed.count = LED_RING_COUNT;
#endif
}

void begin() {
    prefs.begin("hwconfig", false);
    // Valeurs de repli d'abord (fonctionne dès le premier boot, avant tout
    // contact réseau), puis on tente de charger une config déjà synchronisée.
    applyBoardDefaults();

    String stored = prefs.getString("raw", "");
    if (stored.length() > 0) {
        applyFromJson(stored);
    }
}

const NfcConfig& nfc() {
    return currentNfc;
}

const LedConfig& led() {
    return currentLed;
}

bool applyFromJson(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        return false;
    }

    bool appliedSomething = false;

    if (doc["nfc"].is<JsonObject>()) {
        JsonObject n = doc["nfc"];
        bool enabled = n["enabled"] | false;
        if (!enabled) {
            currentNfc = NfcConfig();
            appliedSomething = true;
        } else {
            String iface = n["interface"] | "";
            if (iface == "i2c") {
                JsonObject pins = n["pins"];
                int sda = pins["sda"] | -1;
                int scl = pins["scl"] | -1;
                int irq = pins["irq"] | -1;
                int reset = pins["reset"] | -1;

                if (isValidPin(sda, true) && isValidPin(scl, true) &&
                    isValidPin(irq, false) && isValidPin(reset, false)) {
                    NfcConfig c;
                    c.enabled = true;
                    c.interface = "i2c";
                    c.sda = sda;
                    c.scl = scl;
                    c.irq = (irq >= 0) ? irq : kDefaultNfcIrq;
                    c.reset = (reset >= 0) ? reset : kDefaultNfcReset;
                    currentNfc = c;
                    appliedSomething = true;
                }
                // Pins invalides/en conflit : on ignore cette section et on
                // garde la config NFC précédente (repli ou dernière valide).
            }
            // interface "spi" : pas encore géré côté firmware (nfc_reader.cpp
            // ne pilote le PN532 qu'en I2C pour l'instant), ignoré sans
            // erreur — voir docs/API_CONTRACT.md.
        }
    }

    if (doc["led"].is<JsonObject>()) {
        JsonObject l = doc["led"];
        bool enabled = l["enabled"] | false;
        if (!enabled) {
            currentLed = LedConfig();
            appliedSomething = true;
        } else {
            int pin = l["pin"] | -1;
            int count = l["count"] | 0;
            String model = l["model"] | "ws2812";

            if (isValidPin(pin, true) && count > 0 && count <= 64) {
                LedConfig c;
                c.enabled = true;
                c.model = model;
                c.pin = pin;
                c.count = count;
                currentLed = c;
                appliedSomething = true;
            }
        }
    }

    if (appliedSomething) {
        prefs.putString("raw", json);
    }
    return appliedSomething;
}

} // namespace hw_config
