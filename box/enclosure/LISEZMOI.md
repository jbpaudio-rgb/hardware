# Boîtier Octavio Synth (impression 3D)

Le boîtier est prévu en noir, avec les inscriptions en blanc. Toutes les pièces s'impriment en PETG ou en ASA :

| Fichier | Pièce | Comment l'imprimer |
|---|---|---|
| `octavio-capot.stl` | Façade et côtés | Façade à plat sur le plateau, avec supports seulement dans les trous du panneau arrière |
| `octavio-fond.stl` | Fond, avec les plots du Raspberry Pi | À plat |
| `octavio-cadre.stl` | Pont sous l'écran : quatre vis le relient à l'écran et tirent la vitre sur son rebord | À plat |
| `octavio-inscriptions.stl` | Inscriptions blanches, toutes en anglais : « SINECRAFT » en petit sur le dessus, au-dessus de l'écran, en lettres pixelisées (pixels de 0,7 mm, 35,7 × 4,9 mm, dessinées dans `inscriptions.py`) ; « VOLUME » sous le potentiomètre ; le logo OCTAVIO (70 × 12,6 mm) au milieu de la partie libre du panneau arrière ; le nom de chaque prise à l'arrière | Avec le capot, en deuxième filament (voir plus bas) |
| `octavio-capuchon-1.25.stl`, `-1.30`, `-1.35` | Capuchon Choc v1 surélevé, sans texte, pour tester l'épaisseur des pattes | Dessus à plat sur le plateau, pattes vers le haut, couches de 0,1 mm |
| `octavio-capuchons-1.30-plaque.stl` (ou `-1.25`, `-1.35`) + `octavio-capuchons-texte.stl` | Les 14 capuchons avec leur nom : ENGINE, VOICE, + (la page 2), FILTER, ENV, MOD, SEQ, FX, MIXER, KEYS (un petit clavier dessiné, 13 × 6,5 mm) et 1 à 4 | En noir, avec le texte en blanc, comme le capot |

### Inscriptions blanches
Elles sont incrustées sur 0,6 mm dans la façade, soit les 3 premières couches de 0,2 mm, puisque le capot s'imprime façade sur le plateau.
- **Imprimante multicolore** (AMS, MMU, etc.) : importe `octavio-capot.stl` et `octavio-inscriptions.stl` ensemble, comme un seul objet en deux pièces (dans Bambu Studio ou Orca, réponds « oui » à la question « charger comme un seul objet »). Mets le capot en noir et les inscriptions en blanc. Le texte ressort parfaitement net et à fleur.
- **Imprimante une couleur** : imprime le capot seul. Le texte est alors gravé en creux de 0,6 mm ; remplis-le de peinture acrylique blanche, puis essuie le surplus avec un chiffon à peine humide.
- Pour les capuchons, c'est la même chose : charge la plaque qui correspond à la bonne épaisseur de pattes et `octavio-capuchons-texte.stl` ensemble, comme un seul objet. Le fichier texte s'aligne sur les trois plaques. Les noms des pages font 2,3 mm de haut, en police étroite, pour que PRESET et FILTER tiennent sur 12 mm. Les chiffres des parties font 6 mm.
- Le texte se change dans `inscriptions.py` (listes `PANEL`, `PAGES` et `PARTS`). Le logo est lu dans `web/octavio-app/logo-only.svg` ; il est sur le panneau arrière, centré dans la partie libre à gauche des prises (vu de devant) ; sa largeur et sa position se règlent avec `LOGO_W`, `REAR_FREE` et `LOGO_CZ`. L'encodeur 9 n'a plus d'inscription. Lance-le, puis le générateur.

### Capuchons surélevés
Les capuchons dépassent de 1,5 mm au-dessus de la façade, soit 2,9 mm au-dessus du creux en L. La valeur `CAP_RAISE` dans `enclosure.cpp` règle cette hauteur.
- Ils sont imprimés tous ensemble sur une plaque (version avec texte), dans l'ordre : rangée du bas ENGINE à ENV, au milieu MOD à KEYS, en haut 1 à 4.
- Leur dessous reprend celui d'un capuchon Choc MBK : plaque, petite jupe et deux pattes espacées de 5,7 mm. Seul le dessus est plus épais, donc la course de 3 mm reste libre.
- Les pattes doivent faire 1,25 à 1,35 mm d'épaisseur une fois imprimées. Imprime un capuchon de test de chaque version, garde celle qui tient bien sur l'interrupteur sans forcer, puis imprime la plaque correspondante.
- Noir avec texte blanc, assorti au boîtier. Pour des capuchons blancs, inverse les deux filaments.
- Des capuchons MBK du commerce marchent aussi, mais ils arrivent au ras du creux au lieu de dépasser.

## Dimensions
- **Extérieur :** 308 × 163 × 45 mm.
- **Façade :** 3 mm d'épaisseur. Les écrous des encodeurs et du potentiomètre doivent pouvoir se visser.
- **Murs :** 2,5 mm.
- **Fond :** 3 mm.

Les cotes de la façade reprennent celles de la carte de façade (`fpb/design.py`), au dixième de millimètre près :
- 8 encodeurs, chacun entouré de 12 trous de 2 mm pour les LED de son anneau ;
- l'encodeur 9 et le potentiomètre de volume ;
- 4 ouvertures pour les boutons de partie, chacune avec un trou pour sa LED ;
- une seule fente pour les 10 boutons de page. Leurs LED éclairent par le haut de la fente.
- un grand creux de 1,4 mm de profondeur en forme de L : la rangée des boutons de page, reliée aux boutons de partie, à l'encodeur 9 et au potentiomètre. Dans ce creux, la façade ne fait plus que 1,6 mm d'épaisseur.
  La marge est la même tout autour de chaque groupe de boutons : 5,75 mm autour des boutons de page (comme au-dessus, où sont leurs LED), 5,25 mm autour des boutons de partie (comme à droite du 4).
- une ouverture ajustée par capuchon, avec 0,25 mm de jeu tout autour, et un petit trou de 2 mm au-dessus de chacun pour sa LED. Entre deux boutons de page, la cloison fait 1 mm. Si un capuchon frotte (carte un peu décalée, imprimante qui fait des trous trop petits), augmente `CAP_GAP` dans `enclosure.cpp`, par exemple à 0,35 mm.

## Intérieur
- **Carte de façade :** vissée sous la façade sur 6 plots M3 de 7 mm, aux trous H1 à H6.
- **Écran Touch Display 2 5 po, au ras du boîtier :**
  - La vitre (143,4 × 91,46 mm, coins de 5 mm) est logée dans une poche de 0,7 mm creusée dans le dessus, avec 0,2 mm de jeu tout autour. Elle affleure le dessus du boîtier.
  - Sous sa bordure noire, elle repose sur un rebord de 6,7 mm aux deux bouts et 8,1 mm sur les côtés. Le reste de la façade (2,3 mm à cet endroit) est ouvert pour le boîtier métallique de l'écran, avec 1 mm de jeu, dans un sens comme dans l'autre (nappe à gauche ou à droite).
  - **Fixation par vis, sans colle :** le pont (`octavio-cadre.stl`) se visse sur 4 plots du capot (3 en M2,5 et un en M2 dans le coin étroit à droite de l'écran). Ses deux barres passent sous les 4 plots filetés M2,5 au dos de l'écran, ceux qui servent à y fixer un Pi (49 × 58 mm). Quatre vis M2,5 × 8 traversent les barres et se vissent dans ces plots : elles tirent l'écran vers le bas, et la vitre vient s'appuyer sur le rebord.
  - Il reste 0,6 mm entre les plots de l'écran et le pont : c'est la vitre qui touche en premier, jamais le pont qui la pousse vers le haut.
  - Les trous des barres sont des fentes, pour que l'écran puisse se monter nappe d'un côté ou de l'autre.
  - En option, une bande de mousse adhésive fine (0,2 mm au plus) sur le rebord, contre les vibrations. Si tu en mets une, enlève son épaisseur de `LENS` pour garder la vitre au ras.
  - Les cotes viennent du dessin du « product brief » de Raspberry Pi, qui précise qu'elles sont approximatives. Mesure au pied à coulisse l'épaisseur de la vitre au bord (`LENS`) et la hauteur des plots du Pi au dos (`SCREEN_DEPTH`) avant d'imprimer la version finale.
- **Raspberry Pi 5 et sa carte principale :**
  - Le Pi est tourné de 90°, vissé sur 4 plots du fond (inserts M2,5), sous la carte de façade, côté droit.
  - Pas de ventilateur : un dissipateur passif de 7 mm de haut au plus, collé sur le processeur du Pi.
  - Le HAT est monté sur des entretoises de 11 mm, avec un connecteur 2×20 femelle standard de 8,5 mm. Pas besoin de connecteur surélevé.
  - Les ports USB du Pi sont vers l'avant, à côté du câble de la façade.
- **Jeu vertical :** environ 2,7 mm entre le dessous de la carte de façade (avec les pattes soudées) et le haut des pièces du HAT, et 3,4 mm au-dessus des jacks 1/4.

## Panneau arrière (vu de derrière, de gauche à droite)
Chaque prise a son nom écrit au-dessus, en blanc incrusté de 0,6 mm (dans `octavio-inscriptions.stl`, avec les autres inscriptions), à l'endroit quand on regarde l'arrière.
1. **MIDI IN, MIDI OUT, LINE IN, PHONES :** trous de 6,6 mm alignés sur les jacks 3,5 mm du HAT. MIDI IN, MIDI OUT et LINE IN sont espacés de 13 mm de centre à centre. Le devant des jacks affleure le bord du HAT, et leur nez traverse le mur d'environ 1,2 mm.
2. **OUT R, OUT L :** trous de 9,6 mm pour deux jacks 6,35 mm mono à visser (par exemple Switchcraft 11), reliés à J2 sur le HAT.
3. **POWER (USB-C) :** fente de 12 × 6,5 mm et deux trous M3 à 18 mm d'entraxe, pour la rallonge de panneau SparkFun CAB-15455 (15 cm, alimentation et données), vissée de l'intérieur avec ses deux vis M3. La rallonge va jusqu'au port USB-C du Pi, qui est tourné vers l'intérieur. Elle sert à brancher l'alimentation et à transférer des fichiers depuis un ordinateur.
4. **SD CARD :** fente de 14 × 3 mm et deux trous M2,5 à 23 mm d'entraxe, pour une rallonge microSD à visser sur panneau (environ 15 cm). La fente du Pi est inaccessible une fois le boîtier fermé (sous le Pi, à environ 2 cm du mur arrière) : la rallonge s'y branche et ramène la carte à l'arrière, pour changer de carte ou y copier des sons. Vérifie l'entraxe des oreilles de celle que tu achètes (`SD_EARS` dans `enclosure.cpp`). N'enlève jamais la carte quand la machine est allumée : c'est elle qui fait démarrer le Pi.

Le logo OCTAVIO est au milieu de la partie libre du panneau arrière, à gauche des prises vu de derrière.

Le boîtier n'a plus de fentes d'aération (ni derrière, ni sur le côté, ni dans le fond).

## Visserie et pièces à acheter
- **Inserts filetés à chaud :**
  - M3 : 10 (4 coins et 6 pour la carte de façade).
  - M2,5 : 7 (3 pour le pont de l'écran et 4 pour le Pi).
- **Vis :**
  - M3 × 8 : 10.
  - M2,5 × 6 : 3, et une vis M2 × 6 autotaraudeuse (pont de l'écran sur le capot).
  - M2,5 × 8 : 4 (pont dans les plots au dos de l'écran).
- **Pour le Pi :**
  - 4 entretoises M2,5 mâle-femelle de 11 mm, plus 4 vis M2,5 × 6 pour le HAT.
  - Un dissipateur passif pour Pi 5, de 7 mm de haut au plus.
  - Le connecteur 2×20 femelle standard du HAT (8,5 mm).
- **Câbles :** une rallonge USB-C de panneau SparkFun CAB-15455 (vis M3 fournies), une rallonge microSD à visser sur panneau (environ 15 cm, avec 2 vis M2,5), et un câble JST PH à 3 fils (pas de 2 mm) entre J2 et les deux jacks de sortie.
- **Boutons d'encodeur :** 18 mm de diamètre au plus, sinon ils cachent les LED des anneaux (qui sont à 11 mm du centre).
- **Pieds :** 4 pieds caoutchouc de 12 mm. Le fond a des creux pour les placer.

## À vérifier avant la version finale
- **Hauteur des boutons Choc avec leurs capuchons :** les creux supposent que le dessus du capuchon est à 8,8 mm au-dessus de la carte. Mesure tes vrais capuchons : la valeur `CHOC_TOP` en haut de `enclosure.cpp` règle la profondeur des creux.
- **Bande de test :** imprimer d'abord un morceau de façade (un encodeur, un bouton, un coin de la poche de l'écran) pour vérifier les tolérances de ton imprimante.
- **Impression de la poche de l'écran :** le capot s'imprime façade contre le plateau, donc le rebord sous la vitre est en porte-à-faux au-dessus de la poche (0,7 mm de haut). Active des supports seulement dans la poche (supports touchant le plateau). Le dessous du rebord est caché sous la vitre, son aspect n'a pas d'importance.
- **Rallonge USB-C :** la découpe est faite pour la SparkFun CAB-15455 (trous à 18 mm). Un autre modèle à oreilles M3 de 18 mm convient aussi.
- **Température du Pi sans ventilateur :** jouer un preset lourd sur les 4 parties pendant 15 à 20 minutes, boîtier fermé, et lire `vcgencmd measure_temp`. Sous 75 °C environ, c'est bon. Sinon, remettre des fentes d'aération (elles existaient dans une version précédente du générateur) ou ajouter un petit ventilateur de 30 mm à basse vitesse.

## Modifier le boîtier
Toutes les cotes sont en haut de `enclosure.cpp` : `HIN`, `PANEL`, `BOARD_GAP`, `PI_STANDOFF`, etc. Le programme les vérifie lui-même : il signale si le boîtier touche la carte de façade, le Pi ou l'écran, puis il réécrit tous les fichiers STL. Il utilise la bibliothèque Manifold ; voir `CMakeLists.txt`.
