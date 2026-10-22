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

> **Côté site : implémenté.** Côté firmware : **pas encore** — cette section
> est une proposition de contrat à l'intention de l'équipe firmware, écrite
> en même temps que le site, pas un endpoint déjà consommé en production.
> Rien ne lit ni n'applique `hwConfig` aujourd'hui côté module.

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
      "unknown_card": "Carte inconnue"
    }
  }
}
```

- `nfc.interface` vaut `"i2c"` (pins `sda`/`scl`, `irq`/`reset` optionnels,
  `null` si non câblés) ou `"spi"` (pins `sck`/`mosi`/`miso`/`ss` ; la clé
  `pins` prend alors cette forme à la place — pas les deux en même temps).
- `led.model` : `"ws2812"`, `"sk6812"` ou `"ws2811"` — un seul fil de
  données, pas de LED à horloge séparée (type APA102) dans cette v1.
- `screen.messages.{granted,denied,unknown_card}` : gabarits de texte avec un
  seul paramètre substituable, `{prenom}` (remplacé par le prénom du membre
  identifié, vide sinon). Ce sont EXACTEMENT les mêmes textes que
  `POST /modules/{moduleId}/scan` renvoie déjà dans `message`/`displayText` —
  cet endpoint ne fait qu'exposer la configuration qui les a produits, il n'y
  a pas de nouveau champ à gérer côté affichage temps réel, seulement cette
  lecture de config au démarrage/sur `RELOAD_CONFIG`.
- Toute section peut être absente (pas encore configurée côté site) : traiter
  comme `{"enabled": false}` plutôt que comme une erreur.
- Mêmes garanties que `heartbeat`/`scan` : `Authorization: Bearer
  {moduleSecret}`, 401 si invalide.

**Ce que le firmware devrait faire pour que ça serve à quelque chose** (hors
scope du site, pour que l'équipe firmware sache par où commencer) :
remplacer les constantes de compilation de `board_config.h`
(`HAS_SCREEN`/pins figées par variante) par une lecture de cette config au
démarrage, stockée en NVS pour fonctionner même hors-ligne entre deux
synchronisations — et valider les pins reçues avant de les appliquer (une
pin invalide ne doit jamais pouvoir planter ou endommager le module).

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
