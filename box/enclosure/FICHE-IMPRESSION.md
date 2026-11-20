# Octavio Synth — fiche d'impression 3D / 3D print sheet

Matière / material : **PETG** (ou ASA), noir / black. Le texte et le trait du bouton de volume en blanc / labels and the volume knob's line in white.
Couches / layers : 0,2 mm (0,1 mm pour les capuchons / for the keycaps), 4 parois / walls, remplissage / infill 20 %.

## Pièces / parts

| Fichier / file | Qté / qty | Couleur / colour | Sur le plateau / on the bed | Supports |
| --- | --- | --- | --- | --- |
| `octavio-capot.stl` + `octavio-inscriptions.stl` | 1 | noir + blanc (un seul objet, 2 couleurs / one object, 2 colours) | façade contre le plateau / front face down | seulement les trous arrière et la poche de l'écran / rear holes and screen pocket only |
| `octavio-fond.stl` | 1 | noir | à plat / flat | non / none |
| `octavio-cadre.stl` | 1 | noir | à plat / flat | non / none |
| `octavio-capuchons-1.30-plaque.stl` + `octavio-capuchons-texte.stl` | 1 plaque (14 capuchons) | noir + blanc | dessus contre le plateau / top face down | non / none |
| `octavio-boutons-6.10-plaque.stl` + `octavio-boutons-traits.stl` | 1 plaque (10 boutons) | noir + blanc | dessus contre le plateau / top face down | non / none |

Sans imprimante multicolore / no multi-colour printer: imprimer le noir seul ; le texte reste gravé en creux de 0,6 mm
(à remplir de peinture blanche) / print the black parts only, the text stays engraved 0.6 mm deep (fill with white paint).

## Essais d'abord / test pieces first

- façade / front : `octavio-test-facade.stl` (60 × 95 mm, façade contre le plateau / front face down) : un coin de la
  poche de l'écran, un encodeur et ses trous de LED, un bouton de partie, trois boutons de page. Vérifier que l'encodeur,
  les capuchons et la vitre y entrent / check that the encoder, the keycaps and the glass fit;

Les ajustements dépendent de l'imprimante. Imprimer d'abord ces petites pièces et garder la taille qui tient sans forcer :
fits depend on the printer. Print these small pieces first and keep the size that holds without forcing:

- capuchons / keycaps : `octavio-capuchon-1.25.stl`, `-1.30`, `-1.35` (épaisseur des pattes / leg thickness) → la plaque
  du même numéro / the plate with the same number ;
- boutons / knobs : `octavio-bouton-test-6.00.stl`, `-6.10`, `-6.20` (alésage en D / D bore, axes de 6 mm) → la plaque
  du même numéro / the plate with the same number.

Les plaques 1.30 et 6.10 sont les valeurs les plus probables / 1.30 and 6.10 are the most likely sizes.

## Avant le capot / before the case

La poche de l'écran suit les cotes approximatives de Raspberry Pi. Attendre d'avoir l'écran (Touch Display 2, 5 po) et
mesurer l'épaisseur de la vitre au bord et la hauteur des plots à l'arrière avant d'imprimer le capot.
The screen pocket follows Raspberry Pi's approximate dimensions: measure the Touch Display 2 (glass thickness at the edge,
height of the rear posts) before printing the case.

Tailles / sizes : capot et fond 308 × 163 mm (capot 42 mm de haut). Un plateau de 256 mm est trop petit : il faut au
moins 310 mm (Bambu H2D, Prusa XL, Creality K2 Plus…) ou un service d'impression.
Case and bottom are 308 × 163 mm (case 42 mm high): a 256 mm bed is too small, at least 310 mm is needed.
