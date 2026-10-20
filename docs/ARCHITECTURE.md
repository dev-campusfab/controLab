# ControLab — Architecture générale

## Principe

ControLab est un système d'identification des adhérents d'un fablab par carte NFC.
Il est composé de deux grandes parties :

1. **Modules physiques** (ce dépôt, firmware) : boîtiers à base d'ESP32 (S3 / C3 / C6)
   qui lisent des cartes NFC et exécutent des actions (déverrouillage, pointage,
   autorisation machine, etc.). Il existe plusieurs variantes matérielles
   (avec/sans écran, avec/sans LEDs, compact/full, sur batterie) mais **le
   comportement logiciel est toujours le même** — seules les capacités
   physiques diffèrent (voir `include/board_config.h`).

2. **Plateforme web** (hors de ce dépôt, développée séparément) : gère les
   adhérents, les cartes NFC associées, l'inventaire des modules, la config
   de chaque module et les logs. Voir `docs/WEBSITE_INSTRUCTIONS.md` pour
   les instructions destinées à l'équipe/agent en charge du site.

Le contrat entre les deux (Bluetooth LE d'appairage + API REST d'exploitation)
est décrit dans `docs/BLE_PROTOCOL.md` et `docs/API_CONTRACT.md`. Ces deux
documents sont la **source de vérité** — toute modification doit être faite
des deux côtés en même temps.

## Cycle de vie d'un module

```
┌──────────┐   pas de config BLE/WiFi/token en NVS
│   BOOT    │──────────────────────────────┐
└──────────┘                               ▼
                                     ┌─────────────┐
                                     │   SETUP      │  Advertising BLE,
                                     │ (unbound)    │  attend la procédure
                                     └─────────────┘  de bind depuis le site
                                           │ bind OK (moduleSecret reçu)
                                           ▼
                                     ┌─────────────┐
                                     │ CONNECTING   │  Connexion WiFi avec
                                     └─────────────┘  les creds reçus en BLE
                                           │
                        ┌──────────────────┼───────────────────┐
                        │ échec                                │ succès
                        ▼                                      ▼
                 ┌─────────────┐                        ┌─────────────┐
                 │   ERROR      │ retry avec backoff     │  RUNNING     │
                 └─────────────┘                        └─────────────┘
                                                         lecture NFC en boucle,
                                                         heartbeat périodique,
                                                         appels API sur scan
```

Un module déjà bound peut être repassé en SETUP via :
- la caractéristique BLE `Command` = `ENTER_SETUP` une fois authentifié côté
  site (ouvre une fenêtre BLE sans perdre la config WiFi), ou
- `Command` = `FACTORY_RESET` (efface tout, repasse en SETUP "usine"), ou
- un appui long (5s) sur le bouton BOOT — seul mécanisme de reset qui ne
  dépend ni du site ni d'une connexion BLE/USB active (voir
  `handleFactoryResetButton()` dans `firmware/src/main.cpp`).

## Variantes matérielles (S3)

| Variante      | Écran | LEDs    | Batterie | Usage typique                     |
|---------------|-------|---------|----------|------------------------------------|
| `s3_full`     | OLED  | Anneau  | Oui      | Borne autonome, accueil fablab     |
| `s3_compact`  | Non   | 1 LED   | Non      | Petit boîtier fixé sur machine     |
| `s3_noscreen` | Non   | Anneau  | Oui      | Portique/porte, pas besoin d'écran |

Chaque variante est un environnement PlatformIO distinct (`platformio.ini`),
qui définit des flags de compilation (`-D VARIANT_S3_FULL`, etc.) lus par
`include/board_config.h` pour activer/désactiver `HAS_SCREEN`, `HAS_LED_RING`,
`HAS_BATTERY_MON` et choisir les pins.

## Pourquoi NimBLE et pas le stack BLE Arduino standard

Le stack Bluedroid par défaut d'Arduino-ESP32 est lourd en RAM/flash. On lui
préfère **NimBLE-Arduino** (h2zero), supporté sur S3/C3/C6, plus léger et plus
stable pour un rôle periphérique simple (1 service, quelques caractéristiques).

## Statut d'avancement

- [x] Structure du projet, variantes de board
- [x] Firmware ESP32-S3 : FSM complète, provisioning BLE, NFC, WiFi, API
- [ ] Portage C3 / C6 (même code, juste `board_config.h` à compléter — les
      dossiers `firmware/boards/esp32c3` et `esp32c6` contiennent un
      README avec la marche à suivre)
- [ ] Vérification de compilation réelle (PlatformIO non installé sur la
      machine de dev au moment de l'écriture — à faire : `pio run -e s3_full`)
