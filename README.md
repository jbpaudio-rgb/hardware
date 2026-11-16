# Octavio Synth — synthé matériel

Un synthé de table à 4 parties : Raspberry Pi 5, carte HAT audio, façade RP2040 (9 encodeurs, 14 boutons, LED), écran tactile 5 po, boîtier imprimé en 3D. Il fait tourner les moteurs des plugins Octavio (MINI, PRO70, A26, J8, CS8, VC3, M20, EASEL, VACT), plus DX7, DFAM, DR8, SAMPLER et LINE IN.

## Où trouver quoi

| Dossier | Contenu |
| --- | --- |
| `box/cartes/facade` | Carte de façade (KiCad), Gerbers, BOM et CPL pour JLCPCB : `octavio-facade-gerbers-jlcpcb.zip`, `jlcpcb-bom.csv`, `jlcpcb-cpl.csv` |
| `box/cartes/hat` | Carte HAT audio (KiCad), Gerbers, BOM et CPL pour JLCPCB |
| `box/enclosure` | Boîtier : fichiers STL à imprimer, guide de montage 3D (`guide-montage.html`), aperçu 3D, liste de quincaillerie (`LISEZMOI.md`) |
| `box/facade-fw` | Micrologiciel du RP2040 de la façade |
| `box/systeme` | Installation sur le Pi : services, transfert de fichiers par USB-C |
| `box/web` | Simulateur dans le navigateur (`octavio-box.html`) |
| `box/` | Le programme du synthé (C++) : `Box.h`, `Panel.h`, `ui/`, `main.cpp` |
| `core/` | Les moteurs de son et les effets, partagés avec les plugins |

Le détail du programme est dans [`box/README.md`](box/README.md).

## Compiler

```
cmake -S box -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Sur le Raspberry Pi, il faut ALSA et SDL2 (`sudo apt install libasound2-dev libsdl2-dev`). Sans carte son ni écran : `-DOCTAVIO_NO_DEVICES=ON`.

Le simulateur : `WASI_SDK=/chemin/vers/wasi-sdk bash box/web/build.sh`.

## Commander les cartes

Dans chaque dossier de carte : le zip de Gerbers, `jlcpcb-bom.csv` et `jlcpcb-cpl.csv`. Chez JLCPCB : 4 couches, 1,6 mm, assemblage Standard côté Top. Les étapes sont dans le `LISEZMOI.md` de chaque carte.
