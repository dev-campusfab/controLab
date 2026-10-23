# ControLab — Contrat API REST module ↔ site

Utilisé après l'appairage BLE (voir `BLE_PROTOCOL.md`). Le module n'appelle
jamais le site en dehors de ces endpoints. Tout est en HTTPS, JSON.

Authentification : header `Authorization: Bearer {moduleSecret}` sur tous
les appels sauf `POST /modules/bind`.

## `POST /modules/bind`

Appelé par le module juste après avoir reçu `ServerConfig` en BLE et s'être
connecté au WiFi.

Requête :
```json
{ "moduleId": "s3-7af2c1", "bindToken": "a1b2c3..." }
```

Réponse 200 :
```json
{ "moduleId": "s3-7af2c1", "moduleSecret": "<secret long-lived>" }
```

Erreurs : `400` token invalide, `410` token expiré, `409` module déjà bound
(le site doit alors exiger une confirmation explicite avant de régénérer un
`bindToken` pour un module déjà appairé — sinon un module volé/perdu pourrait
être réappairé silencieusement).

## `POST /modules/{moduleId}/heartbeat`

Appelé toutes les ~60s par le module en état `RUNNING`.

Requête :
```json
{
  "fwVersion": "0.1.0",
  "uptimeSec": 3600,
  "wifiRssi": -58,
  "batteryPercent": 87,
  "pendingSetupWindow": false
}
```
(`batteryPercent` absent/`null` si la variante n'a pas de batterie.)

Réponse 200 — le site peut pousser des ordres via ce canal plutôt que
d'exiger un serveur joignable depuis l'extérieur côté module :
```json
{
  "ok": true,
  "command": null,
  "scanPollIntervalMs": 150
}
```
`command` peut valoir `"ENTER_SETUP"`, `"REBOOT"`, `"FACTORY_RESET"`,
`"RELOAD_CONFIG"`, `null`. Le module exécute l'ordre au heartbeat suivant
qu'il le reçoit. `RELOAD_CONFIG` signifie que `hwConfig` (voir
`GET /modules/{moduleId}/hw-config` ci-dessous) a changé côté site depuis la
dernière lecture — le module est invité à le relire.

## `POST /modules/{moduleId}/scan`

Appelé à chaque lecture de carte NFC. **Uniquement pour les modules dont
`capabilities.nfc` vaut `true`** (voir `BLE_PROTOCOL.md`) — un module sans
lecteur NFC n'appelle jamais cet endpoint, le backend n'a rien de
particulier à faire pour ces modules-là côté scan.

Requête :
```json
{ "cardUid": "04A224B2C15E80", "timestampMs": 169000000000 }
```

Réponse 200 — c'est le site qui décide de l'action, le module se contente
d'afficher/signaler :
```json
{
  "result": "granted",
  "message": "Bonjour Alex",
  "ledColor": "#00c853",
  "displayText": "Bonjour\nAlex C.",
  "beep": true
}
```
`result` : `"granted"`, `"denied"`, `"unknown_card"`, `"error"`.
`ledColor` / `displayText` ignorés si la variante n'a pas la capacité
correspondante (le module sait déjà via `board_config.h` ce qu'il peut
afficher ; le site n'a pas besoin de connaître les capacités pour construire
cette réponse — le module trie côté firmware).

## `GET /modules/{moduleId}/hw-config`

> **`nfc`/`led`/`screen` : implémentés des deux côtés** (site et firmware —
> `firmware/src/net/api_client.cpp::getHwConfig()`,
> `firmware/src/core/hw_config.cpp::applyFromJson()`, persisté en NVS).
> **`automations` : côté site implémenté, côté firmware PAS ENCORE** — voir
> plus bas, c'est une proposition de contrat écrite en même temps que le
> site, pas un comportement déjà consommé en production.

Pensé pour être appelé par le module une fois au démarrage (après `BOUND_OK`
ou au passage en `RUNNING`), et de nouveau chaque fois qu'un heartbeat répond
`command: "RELOAD_CONFIG"` — pas à chaque heartbeat, le contenu ne change que
quand un admin modifie la configuration matérielle du module sur le site.

Réponse 200 :
```json
{
  "nfc": {
    "enabled": true,
    "model": "pn532",
    "interface": "i2c",
    "pins": { "sda": 21, "scl": 22, "irq": null, "reset": null }
  },
  "led": {
    "enabled": true,
    "model": "ws2812",
    "pin": 5,
    "count": 12
  },
  "screen": {
    "messages": {
      "granted": "Bonjour {prenom}",
      "denied": "Accès refusé",
      "unknown_card": "Carte inconnue",
      "outside_schedule": "Hors horaires d'accès"
    }
  },
  "automations": [
    {
      "id": "r1",
      "enabled": true,
      "trigger": { "type": "on_idle" },
      "actions": [
        { "type": "set_color", "color": "#001133", "brightness": 30 }
      ]
    },
    {
      "id": "r2",
      "enabled": true,
      "trigger": { "type": "on_denied_streak", "count": 3, "window_sec": 10 },
      "actions": [
        { "type": "blink", "color": "#FF0000", "times": 5, "interval_ms": 200 },
        { "type": "wait", "ms": 500 },
        { "type": "set_color", "color": "#001133", "brightness": 30 }
      ]
    }
  ]
}
```

- `nfc.interface` vaut `"i2c"` (pins `sda`/`scl`, `irq`/`reset` optionnels,
  `null` si non câblés) ou `"spi"` (pins `sck`/`mosi`/`miso`/`ss` ; la clé
  `pins` prend alors cette forme à la place — pas les deux en même temps).
- `led.model` : `"ws2812"`, `"sk6812"` ou `"ws2811"` — un seul fil de
  données, pas de LED à horloge séparée (type APA102) dans cette v1.
- `screen.messages.{granted,denied,unknown_card,outside_schedule}` : gabarits
  de texte avec un seul paramètre substituable, `{prenom}` (remplacé par le
  prénom du membre identifié, vide sinon). Ce sont EXACTEMENT les mêmes
  textes que `POST /modules/{moduleId}/scan` renvoie déjà dans
  `message`/`displayText` — cet endpoint ne fait qu'exposer la configuration
  qui les a produits, il n'y a pas de nouveau champ à gérer côté affichage
  temps réel, seulement cette lecture de config au démarrage/sur
  `RELOAD_CONFIG`.
- Toute section peut être absente (pas encore configurée côté site) : traiter
  comme `{"enabled": false}` (ou `[]` pour `automations`) plutôt que comme
  une erreur.
- Mêmes garanties que `heartbeat`/`scan` : `Authorization: Bearer
  {moduleSecret}`, 401 si invalide.

**`automations` — scénarios LED ("Shortcuts" : déclencheur + chaîne
d'actions), PAS ENCORE interprétés côté firmware.** Vocabulaire fermé
(volontairement pas d'expression libre — évite d'avoir à écrire un vrai
parseur de conditions), déjà validé côté site (toute règle/action hors de
cette liste, ou aux valeurs hors bornes, est supprimée avant stockage) :

Déclencheurs (`trigger.type`) :
- `on_idle` — aucun scan récent, module au repos.
- `on_granted` / `on_denied` / `on_unknown_card` — résultat d'un scan.
- `on_denied_streak` — `count` refus en moins de `window_sec` secondes
  (`count` 1–20, `window_sec` 1–3600).

Actions (`actions`, exécutées dans l'ordre, séquentiellement) :
- `set_color` : `color` (`#RRGGBB`), `brightness` (0–100).
- `blink` : `color`, `times` (1–20), `interval_ms` (20–10000).
- `breathe` : `color`, `duration_ms` (100–10000) — fondu entrant/sortant.
- `wait` : `ms` (0–10000) — pause avant l'action suivante de la même règle.
- `beep` : `times` (1–20) — ignoré si la variante n'a pas de buzzer, même
  tolérance que `ledColor`/`displayText` plus haut.
- `turn_off` — éteint la LED.

**Ce que le firmware devrait faire pour que ça serve à quelque chose** (hors
scope du site, pour que l'équipe firmware sache par où commencer) :
- Pour `nfc`/`led`/`screen` : déjà fait (voir `hw_config.cpp`).
- Pour `automations` : maintenir un historique court des derniers résultats
  de scan avec leur horodatage (pour évaluer `on_denied_streak`) et un
  timestamp "dernier événement" (pour `on_idle`) ; exécuter les actions de
  façon NON bloquante (scheduler à base de `millis()`, comme le `tick()`
  existant d'`ui_feedback.cpp` — jamais de `delay()` bloquant dans la boucle
  principale) ; une seule file d'actions active à la fois (pas de vraie
  concurrence en v1) — un nouveau déclenchement interrompt proprement la
  séquence en cours plutôt que de l'empiler ; si aucun scénario ne
  correspond à un déclencheur donné, garder le comportement actuel codé en
  dur (`colorForState()`/`showScanResult()`) — rétrocompatible, remplacement
  progressif plutôt que tout ou rien.
- Dans tous les cas : valider les pins/valeurs reçues avant de les appliquer
  (une valeur invalide ne doit jamais pouvoir planter ou endommager le
  module) — déjà fait côté `hw_config.cpp::isValidPin()`, à reprendre pour
  les paramètres d'`automations`.

## `POST /modules/{moduleId}/enter-setup`

Appelé par le site (authentifié utilisateur, pas par le module) quand un
admin veut reconfigurer un module déjà bound (ex. changer de WiFi). Le site
appelle cet endpoint, le backend marque `pendingSetupWindow = true`, que le
module verra au prochain heartbeat (ou immédiatement si vous ajoutez un
canal push/MQTT plus tard — hors scope v1). Le module rouvre alors
l'advertising BLE de setup pendant 5 minutes, en conservant son
`moduleSecret` existant (pas de nouveau bind nécessaire si c'est juste pour
changer le WiFi — à voir côté site si vous voulez quand même forcer un
nouveau `bindToken` par sécurité).

## Notes pour l'implémentation site

- Le `moduleSecret` doit être traité comme un mot de passe (hashé en base
  si vous voulez éviter qu'une fuite DB le expose en clair — comparaison en
  temps constant).
- Les endpoints `heartbeat` et `scan` doivent être rapides (<300ms) : le
  module a un timeout HTTP côté firmware d'environ 15s mais ne doit pas
  bloquer longtemps la lecture NFC suivante.
- Prévoir un rate-limit raisonnable par module (ex. 1 scan/sec max côté
  serveur) au cas où un module boucle en erreur.
