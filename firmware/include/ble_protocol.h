#pragma once
// Constantes du protocole BLE de provisioning.
// Doit rester synchronisé avec docs/BLE_PROTOCOL.md et avec le code web
// (voir docs/WEBSITE_INSTRUCTIONS.md) — ne pas changer un UUID sans
// répercuter côté site.

#define BLE_SETUP_SERVICE_UUID       "6c6f6c61-0001-4a3b-9c2e-5f3a1b2c0001"
#define BLE_CHAR_DEVICE_INFO_UUID    "6c6f6c61-0002-4a3b-9c2e-5f3a1b2c0001"
#define BLE_CHAR_WIFI_CONFIG_UUID    "6c6f6c61-0003-4a3b-9c2e-5f3a1b2c0001"
#define BLE_CHAR_SERVER_CONFIG_UUID  "6c6f6c61-0004-4a3b-9c2e-5f3a1b2c0001"
#define BLE_CHAR_STATUS_UUID         "6c6f6c61-0005-4a3b-9c2e-5f3a1b2c0001"
#define BLE_CHAR_COMMAND_UUID        "6c6f6c61-0006-4a3b-9c2e-5f3a1b2c0001"

#define BLE_ADV_NAME_PREFIX          "ControLab-Setup-"

#define FW_VERSION                   "0.1.0"
