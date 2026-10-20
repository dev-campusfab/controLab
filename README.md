# ControLab

Système d'identification des adhérents de fablab par carte NFC, déployable
sur plusieurs modules physiques (ESP32-S3 / C3 / C6, avec ou sans écran,
avec ou sans LEDs, compact ou full, sur secteur ou batterie).

## Structure du dépôt

- `firmware/` — firmware des modules (PlatformIO). Développement actif sur
  ESP32-S3 (`s3_full`, `s3_compact`, `s3_noscreen`). Voir
  `firmware/boards/esp32c3` et `firmware/boards/esp32c6` pour le statut du
  portage sur ces cibles.
- `docs/ARCHITECTURE.md` — vue d'ensemble du système et cycle de vie d'un
  module.
- `docs/BLE_PROTOCOL.md` — protocole Bluetooth LE d'appairage
  (module ↔ téléphone/site), contrat figé entre firmware et site web.
- `docs/API_CONTRACT.md` — API REST utilisée par un module déjà appairé.
- `docs/WEBSITE_INSTRUCTIONS.md` — instructions pour l'équipe/agent en
  charge de la plateforme web (non incluse dans ce dépôt).

## Démarrer avec le firmware

```sh
cd firmware
pio run -e s3_full      # ou s3_compact / s3_noscreen
pio run -e s3_full -t upload
pio device monitor
```

Nécessite [PlatformIO](https://platformio.org/). Non vérifié par
compilation réelle au moment de l'écriture — à faire avant tout flashage.
