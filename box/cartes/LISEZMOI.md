# Cartes électroniques de l'Octavio Synth

| Dossier | Carte |
|---|---|
| `facade/` | Carte de façade : RP2040, 9 encodeurs avec anneaux de LED, 14 boutons Choc, volume (4 couches) |
| `hat/` | Carte principale (HAT du Raspberry Pi 5) : DAC, ADC, ampli casque, MIDI sur jacks 3,5 mm (4 couches) |

Chaque dossier contient le projet KiCad (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`), les fichiers de commande JLCPCB (`jlcpcb-bom.csv`, `jlcpcb-cpl.csv`), un aperçu et un LISEZMOI. Les deux cartes sont prévues pour arriver entièrement montées de chez JLCPCB.

`generateur/` contient les scripts Python qui ont produit chaque carte (placement, routage avec Freerouting, vérifications, fichiers JLCPCB). Ils supposent les bibliothèques KiCad et Freerouting 1.9 installées ; les chemins sont en haut de `design.py`. Pour une petite retouche, il est plus simple d'ouvrir directement le projet dans KiCad.
