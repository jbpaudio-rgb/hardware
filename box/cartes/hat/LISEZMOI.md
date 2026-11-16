# Octavio Synth : carte principale (HAT pour Raspberry Pi 5 1 Go) v1

Cette carte se branche sur le connecteur 40 broches du Pi 5. Le Pi s'occupe de l'écran (DSI), de la carte microSD et de l'USB de la façade. La carte ajoute le reste :

- **Audio :**
  - DAC PCM5122 pour la sortie ligne.
  - ADC PCM1863 pour l'entrée ligne.
  - Ampli casque TPA6130A2.
  - Deux oscillateurs : 24,576 MHz et 22,5792 MHz.
- **MIDI :** IN et OUT sur jacks 3,5 mm TRS de type A. L'entrée est isolée par un optocoupleur H11L1.
- **Alimentation :**
  - 3,3 V propre pour l'audio (régulateur XC6206).
  - 5 V protégé par fusible pour les LED de la façade (J7, même brochage que J3 sur la façade).
  - 5 V pour l'écran tactile (J8, picots 2,54 mm).
- **EEPROM d'identification HAT** (CAT24C32, adresse 0x50).

## Dimensions et mécanique
- 86 × 56,5 mm, 4 couches. Couches internes : In1 = GND, In2 = +3V3A.
- La partie 65 × 56,5 mm couvre le Pi, avec les 4 trous M2,5 aux positions standard. La bande de 21 mm à gauche dépasse le bord microSD du Pi.
- Cette bande porte les 4 jacks 3,5 mm, qui forment le panneau arrière. De haut en bas : MIDI IN, MIDI OUT, LINE IN (espacés de 13 mm), puis PHONES. Le devant des jacks affleure le bord de la carte.
- La sortie ligne utilise deux jacks 1/4 vissés dans le panneau arrière du boîtier. Ils sont reliés par un câble à J2, un connecteur JST PH vertical à 3 broches (B3B-PH-K-S, posé par JLCPCB) : 1 = gauche (pointe du jack L), 2 = droite (pointe du jack R), 3 = masse (corps des deux jacks).
- Le connecteur 2×20 femelle se soude **sous** la carte. L'empreinte KiCad est celle d'un header mâle, placée pour que la broche 1 tombe sur la broche 1 du Pi.
- Le Pi est refroidi par un dissipateur passif de 7 mm de haut au plus. La carte se monte sur des entretoises de 11 mm avec le connecteur femelle standard de 8,5 mm.

## Alimentation
On utilise l'alimentation officielle USB-C 27 W du Pi 5, branchée sur le Pi. La carte prend son 5 V sur les broches 2 et 4 du connecteur. Dans le boîtier, une rallonge USB-C à montage sur panneau ramène la prise à l'arrière.

## Configuration Raspberry Pi OS (`/boot/firmware/config.txt`)
```
dtoverlay=hifiberry-dacplusadcpro
dtoverlay=uart2-pi5
```
- Le circuit audio reprend celui de la HiFiBerry DAC+ADC Pro, donc le pilote standard fonctionne :
  - PCM5122 à l'adresse 0x4D, maître d'horloge.
  - Ses GPIO3 et GPIO6 activent les oscillateurs 48 kHz et 44,1 kHz.
  - PCM1863 à l'adresse 0x4A.
- L'ampli casque (TPA6130A2, adresse 0x60) est désactivé dans cet overlay, qui n'a pas de paramètre pour l'activer. octavio-box l'allumera et réglera son volume directement en I2C.
- MIDI : UART2, GPIO4 = TX et GPIO5 = RX, à 31 250 bauds.

## Broches utilisées sur le connecteur
| Broche | Signal | Rôle |
|---|---|---|
| 1, 17 | 3V3 | EEPROM, optocoupleur MIDI |
| 2, 4 | 5V | alimentation de la carte |
| 3 / 5 | GPIO2/3 | I2C (DAC, ADC, ampli casque) |
| 7 | GPIO4 | MIDI OUT (TX2) |
| 29 | GPIO5 | MIDI IN (RX2) |
| 12 / 35 | GPIO18 / 19 | I2S BCK / LRCK |
| 38 / 40 | GPIO20 / 21 | I2S données ADC → Pi / Pi → DAC |
| 27 / 28 | ID_SD / ID_SC | EEPROM HAT |

## À faire dans KiCad avant de commander
1. Ouvrir `octavio-hat.kicad_pro`. Toutes les empreintes viennent des bibliothèques standard de KiCad.
2. Appuyer sur **B** pour remplir les zones, lancer le **DRC** sur le circuit et l'**ERC** sur le schéma.
3. Chez JLCPCB, vérifier l'aperçu de placement : rotations des CI, et broche 1 des oscillateurs = OE.
4. **Stock JLCPCB à vérifier.** Le PCM5122PWR (C962071) et le PCM1863DBTR (C529569) ont parfois peu de stock. Une alternative pour le DAC est le PCM5122PW (C1540085).
5. Vérifier l'empreinte du TPA6130A2 (QFN 4×4, pastille centrale 2,7 mm) avec la fiche technique TI du boîtier RTJ.

## Fichiers
- `octavio-hat.kicad_pcb` et `.kicad_sch` : le projet. Le circuit est entièrement routé ; aucune connexion manquante ni erreur d'isolation dans mes vérifications.
- `jlcpcb-bom.csv` et `jlcpcb-cpl.csv` : assemblage complet chez JLCPCB, pièces traversantes comprises. La carte arrive entièrement montée.
- `pieces-a-souder.csv` : vide, il n'y a plus rien à souder sur la carte. Il faut seulement, à part, deux jacks 1/4 mono de panneau (par exemple Switchcraft 11) et un câble JST PH à 3 fils.

## Pièces traversantes posées par JLCPCB
| Repère | Pièce | LCSC | Face |
|---|---|---|---|
| J1 | Connecteur femelle 2×20, 2,54 mm, 8,5 mm de haut | C2977589 | **Dessous** (il se branche sur le Pi) |
| J3 à J6 | Jacks 3,5 mm CUI SJ1-3533NG | C4992459 | Dessus |
| J2 | JST PH vertical 3 broches B3B-PH-K-S | C131339 | Dessus |
| J8 | Picots mâles 1×2, 2,54 mm | C2905434 | Dessus |

- J1 est la seule pièce sous la carte. Dans le fichier de placement, elle est marquée « Bottom ». Si le devis la refuse en assemblage économique, choisis l'assemblage standard.
- Pour les pièces traversantes, la position donnée est le centre de la pièce. JLCPCB montre quand même un aperçu : vérifie que les jacks pointent vers le bord de la carte et que le connecteur 2×20 est bien dessous.

## Notes
- Le niveau ligne de sortie est de 2,1 Vrms. L'entrée de l'ampli casque passe par un diviseur 4,7k/2,2k.
- Pour écrire l'EEPROM HAT, faire un pont de soudure sur JP1 (WP à la masse), puis l'enlever.
- La broche de blindage (S) du jack MIDI IN n'est pas connectée, ce qui évite les boucles de masse (norme MIDI).
