# Façade Octavio : protocole USB-MIDI

La façade est un périphérique USB-MIDI nommé **Octavio Facade**. Tous les messages passent sur le **canal MIDI 16**.

## Façade → octavio-box
| Contrôle | Message |
|---|---|
| Encodeurs 1 à 9 | CC 16 à 24, valeur relative : 1 à 63 = vers la droite, 127 à 65 = vers la gauche (127 = -1). Accéléré quand on tourne vite. |
| Poussoirs des encodeurs 1 à 9 | Notes 0 à 8 (note on vélocité 127 en appuyant, note off en relâchant) |
| Boutons de page ENGINE, VOICE, + (PAGE 2), FILTER, ENV, MOD, SEQ, FX, MIXER, KEYS (clavier) | Notes 16 à 25, note-on à l'appui et note-off au relâchement (le mode clavier s'en sert) |
| Boutons de partie 1 à 4 | Notes 32 à 35 |
| Potentiomètre de volume | CC 7, de 0 à 127 |
| DÉFILER (poussoir de l'encodeur 9) tenu 2 s | Note 9 (on puis off) : octavio-box garde la session et éteint la machine |
| Identité (au branchement, ou en réponse à la demande) | `F0 7D 4F 02 <version majeure> <version mineure> <nb encodeurs> <nb anneaux> F7` |

## octavio-box → façade
| Message | Effet |
|---|---|
| CC 16 à 23 | Valeur de l'anneau 1 à 8 (0 à 127), sans changer sa couleur |
| `F0 7D 4F 01 F7` | Demande d'identité |
| `F0 7D 4F 10 <anneau 0-7> <valeur> <r> <g> <b> <mode> F7` | Anneau complet. Mode : 0 = arc, 1 = point, 2 = bipolaire (centré à midi), 3 = éteint. Couleurs de 0 à 127. |
| `F0 7D 4F 11 <led 0-109> <r> <g> <b> F7` | Une LED. Elle reste fixée jusqu'au message 14. |
| `F0 7D 4F 12 <début> <nombre> (<r> <g> <b>)… F7` | Plusieurs LED à la suite |
| `F0 7D 4F 13 <luminosité 0-127> F7` | Luminosité générale (48 par défaut) |
| `F0 7D 4F 14 F7` | Libère les LED fixées par 11 ou 12 |

## Numéros de LED
- 0 à 95 : anneaux. L'anneau n (0 à 7) occupe les LED 12n à 12n+11. La LED k est à 15 + 30k degrés, en partant de 3 h et dans le sens des aiguilles d'une montre.
  - L'arc va de 7 h 30 à 4 h 30, dans l'ordre k = 4, 5, … 11, 0, 1.
  - Les LED k = 2 et 3, en bas, montrent la couleur de l'anneau en faible intensité.
- 96 à 99 : LED des boutons de partie 1 à 4.
- 100 à 109 : LED des boutons de page, dans l'ordre ENGINE … KEYS.

La façade limite elle-même le courant total des LED à environ 1,2 A.

## Allumer et éteindre (v1.1)
- La façade reste alimentée par le 5 V du connecteur GPIO (J3, via D3) quand le Pi est arrêté : le Pi 5 coupe ses ports USB à l'arrêt, mais pas ses broches 5 V.
- Sans hôte USB depuis 5 s (Pi arrêté), toutes les LED s'éteignent.
- DÉFILER tenu 2 s :
  - Pi allumé : note 9. octavio-box enregistre la session (`~/octavio/.derniere-session`, rechargée au démarrage) puis lance `systemctl poweroff`.
  - Pi arrêté : GPIO28 passe à 1 pendant 300 ms. Q1 (2N7002) tire alors à la masse les pastilles du bouton d'alimentation du Pi 5 (J2), reliées à J5, et le Pi démarre.
- L'appui court reste lecture / arrêt (note 8).
