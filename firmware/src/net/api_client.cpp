#include "api_client.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "../../include/ble_protocol.h" // FW_VERSION

namespace api_client {

// 5s était trop court pour un premier appel HTTPS (négociation TLS incluse,
// potentiellement un cold start du serveur) : provoquait un read Timeout
// (-11) même quand le WiFi et le serveur allaient bien.
static const uint32_t HTTP_TIMEOUT_MS = 15000;

static void applyAuthHeader(HTTPClient& http, const String& moduleSecret) {
    http.addHeader("Authorization", "Bearer " + moduleSecret);
    http.addHeader("Content-Type", "application/json");
}

BindResult bind(const String& apiBaseUrl, const String& moduleId, const String& bindToken) {
    BindResult res;
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(apiBaseUrl + "/modules/bind")) {
        res.errorCode = "network";
        return res;
    }
    http.addHeader("Content-Type", "application/json");

    JsonDocument body;
    body["moduleId"] = moduleId;
    body["bindToken"] = bindToken;
    String payload;
    serializeJson(body, payload);

    int code = http.POST(payload);
    if (code == 200) {
        JsonDocument resp;
        DeserializationError err = deserializeJson(resp, http.getString());
        if (!err && resp["moduleSecret"].is<const char*>()) {
            res.ok = true;
            res.moduleSecret = resp["moduleSecret"].as<String>();
        } else {
            res.errorCode = "network";
        }
    } else if (code == 400) {
        res.errorCode = "invalid_token";
    } else if (code == 410) {
        res.errorCode = "expired_token";
    } else if (code == 409) {
        res.errorCode = "already_bound";
    } else {
        res.errorCode = "network";
    }
    http.end();
    return res;
}

ScanResult reportScan(const String& apiBaseUrl, const String& moduleId,
                       const String& moduleSecret, const String& cardUid) {
    ScanResult res;
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(apiBaseUrl + "/modules/" + moduleId + "/scan")) {
        res.result = "error";
        return res;
    }
    applyAuthHeader(http, moduleSecret);

    JsonDocument body;
    body["cardUid"] = cardUid;
    body["timestampMs"] = (uint64_t)millis(); // horloge locale, pas d'horloge réseau synchronisée en v1
    String payload;
    serializeJson(body, payload);

    int code = http.POST(payload);
    if (code == 200) {
        JsonDocument resp;
        if (!deserializeJson(resp, http.getString())) {
            res.ok = true;
            res.result = resp["result"] | "error";
            res.message = resp["message"] | "";
            res.ledColorHex = resp["ledColor"] | "";
            res.displayText = resp["displayText"] | "";
            res.beep = resp["beep"] | false;
        }
    } else {
        res.result = "error";
    }
    http.end();
    return res;
}

HeartbeatResult heartbeat(const String& apiBaseUrl, const String& moduleId,
                          const String& moduleSecret, float batteryPercent) {
    HeartbeatResult res;
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(apiBaseUrl + "/modules/" + moduleId + "/heartbeat")) {
        return res;
    }
    applyAuthHeader(http, moduleSecret);

    JsonDocument body;
    body["fwVersion"] = FW_VERSION;
    body["uptimeSec"] = millis() / 1000;
    body["wifiRssi"] = WiFi.RSSI();
    if (batteryPercent >= 0) {
        body["batteryPercent"] = batteryPercent;
    } else {
        body["batteryPercent"] = nullptr;
    }
    String payload;
    serializeJson(body, payload);

    int code = http.POST(payload);
    if (code == 200) {
        JsonDocument resp;
        if (!deserializeJson(resp, http.getString())) {
            res.ok = resp["ok"] | false;
            res.command = resp["command"] | "";
            res.scanPollIntervalMs = resp["scanPollIntervalMs"] | 150;
        }
        Serial.printf("[API] heartbeat OK (200)\n");
    } else {
        Serial.printf("[API] heartbeat echec, code HTTP=%d (401=secret invalide -> rebind necessaire)\n", code);
    }
    http.end();
    return res;
}

HwConfigResult getHwConfig(const String& apiBaseUrl, const String& moduleId,
                           const String& moduleSecret) {
    HwConfigResult res;
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(apiBaseUrl + "/modules/" + moduleId + "/hw-config")) {
        return res;
    }
    http.addHeader("Authorization", "Bearer " + moduleSecret);

    int code = http.GET();
    if (code == 200) {
        res.ok = true;
        res.json = http.getString();
    } else {
        Serial.printf("[API] hw-config echec, code HTTP=%d\n", code);
    }
    http.end();
    return res;
}

} // namespace api_client
