# Octavio Synth : carte de façade v1.1

Carte de façade avec un RP2040 qui se branche en USB-MIDI sur la carte principale (CM5).

- 4 couches, 292 × 147 mm, en forme de L autour de l'écran.
- 8 anneaux de 12 LED autour des encodeurs, plus 4 LED de partie et 10 LED de page.
- 9 encodeurs, 14 boutons Choc et le potentiomètre de volume.

## Couches
| Couche | Rôle |
|---|---|
| F.Cu | Composants et pistes, remplissage GND |
| In1.Cu | Plan de masse (GND) |
| In2.Cu | Plan +5V_LED |
| B.Cu | Pistes, remplissage GND |

Les broches GND et +5V_LED des CMS rejoignent leur plan interne par un via placé juste à côté.

## Règles de routage
- Signaux : piste 0,15 mm, isolation 0,13 mm.
- +3V3 : piste 0,25 mm.
- Alimentation +5V : piste 0,6 mm.
- Vias : 0,6 / 0,3 mm.

Ces valeurs sont dans les capacités standard 4 couches de JLCPCB.

## À faire dans KiCad (version 7 ou 8) avant de commander
1. Ouvrir `octavio-facade.kicad_pro`. Accepter la mise à jour du format si KiCad la propose.
2. Ajouter la bibliothèque `footprints/` sous le nom **Octavio** dans *Préférences > Gérer les bibliothèques d'empreintes*.
3. PCB : appuyer sur **B** pour remplir les zones, puis lancer le **DRC**.
4. Schéma : lancer l'**ERC**. Le schéma est généré avec des étiquettes de réseau sur chaque broche, il n'a pas de fils.
5. Vérifier l'empreinte des encodeurs. J'ai utilisé l'empreinte EC11E, qu'on retrouve couramment pour le Bourns PEC11R ; comparer avec la fiche technique du PEC11R-4220F-S0024.
6. Chez JLCPCB, vérifier l'aperçu de placement. Les rotations de certaines pièces (RP2040, USB-C, LED 2020) demandent parfois un ajustement de ±90° ou 180°.

## Fichiers
- `octavio-facade.kicad_pcb` et `.kicad_sch` : le projet.
- `jlcpcb-bom.csv` et `jlcpcb-cpl.csv` : assemblage complet chez JLCPCB, pièces traversantes comprises. La carte arrive entièrement montée.
- `pieces-a-souder.csv` : il ne reste que le connecteur SWD (J4, 3 picots), facultatif. Il ne sert qu'au débogage ; le firmware se charge par USB.

## Pièces traversantes posées par JLCPCB
| Repère | Pièce | LCSC |
|---|---|---|
| ENC1 à ENC9 | Encodeurs Bourns PEC11R-4220F-S0024 | C143797 |
| SW11 à SW20, SW31 à SW34 | Interrupteurs Kailh Choc v1 rouges (linéaires) CPG135001D01 | C400229 |
| RV1 | Potentiomètre Alps RK09L1140A2U, 10 kΩ linéaire | C470448 |

- La façade contient des LED WS2812 : JLCPCB demande alors l'assemblage standard.
- Pour les encodeurs et le potentiomètre, la position donnée est le centre de la pièce. Vérifie dans l'aperçu de JLCPCB que leurs broches tombent dans les trous.
- Les interrupteurs Choc rouges sont linéaires, sans clic. Pour un toucher différent, remplace C400229 dans `jlcpcb-bom.csv` par un autre Choc v1 de LCSC.
- `octavio-facade-preview.png` : aperçu du routage.

## Notes
- Les 110 LED à pleine puissance en blanc peuvent consommer environ 4 A. Le firmware devra limiter la luminosité, par exemple à 25 % maximum.
- JP1 (non monté) relie le 5V de l'USB-C au +5V_LED, pour tester la carte seule.
- Les anneaux ont un condensateur de 100 nF toutes les 3 LED.

## Allumer et éteindre avec DÉFILER (v1.1)
Tenir DÉFILER (le poussoir de l'encodeur 9) 2 secondes allume ou éteint la machine. Trois pièces ont été ajoutées pour ça ; les autres n'ont pas bougé.

- **D3 (B5819W)** relie le +5V_LED (le 5 V du connecteur GPIO du Pi, par J3) au +5V du RP2040. Le Pi 5 coupe ses ports USB quand il est arrêté, mais pas ses broches 5 V : la façade reste donc alimentée et peut attendre l'appui.
- **Q1 (2N7002) et R32 (100 k)** : le GPIO28 du RP2040 commande Q1, qui tire à la masse le signal du bouton d'alimentation du Pi 5. R32 garde Q1 bloqué tant que le RP2040 démarre.
- **J5 (JST-PH 2 broches)** : 1 = bouton d'alimentation du Pi, 2 = masse.

### Câblage côté Pi
1. Souder une barrette de 2 picots (2,54 mm) sur **J2**, les deux pastilles du bouton d'alimentation du Pi 5, entre la prise de la pile RTC et le bord de la carte.
2. Relier J5 à ces picots avec un câble JST-PH 2 broches vers Dupont femelle 2,54 mm. La broche 2 (masse) va sur la pastille reliée à la masse du Pi : vérifie-la au multimètre (continuité avec une broche GND du connecteur GPIO).

### Réglages du Pi
- Recommandé : `sudo rpi-eeprom-config -e`, puis `POWER_OFF_ON_HALT=1`. À l'arrêt, le Pi consomme alors presque rien, et le bouton d'alimentation le rallume toujours.
- octavio-box éteint avec `sudo -n systemctl poweroff`. L'utilisateur qui le lance doit pouvoir le faire sans mot de passe. C'est déjà le cas pour l'utilisateur par défaut de Raspberry Pi OS ; sinon, ajouter `TONUTILISATEUR ALL=(root) NOPASSWD: /usr/bin/systemctl poweroff` avec `sudo visudo`.
