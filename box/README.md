# Octavio Box

The program of the Octavio hardware synth: a 4-part multitimbral synth. Each part has one of the 11 Octavio engines
(`core/synth`), its own 8-slot FX rack (`core/FxEngine.h`) and its own MIDI channel; plus a mixer, MIDI in and a
shared tempo. It uses the plugins' C++ engines
as they are (no JUCE), with [RtAudio](https://github.com/thestk/rtaudio) and [RtMidi](https://github.com/thestk/rtmidi)
for the audio and MIDI devices (`third_party/`, MIT-style licences).

It runs the same on a Mac (to try it now) and on a Raspberry Pi 5 / CM5 (the hardware). Until the front panel exists,
it is driven by commands typed in the terminal; the panel will send the same commands.

## Build and run

Mac (needs the Xcode command line tools and CMake: `xcode-select --install`, `brew install cmake`):

```
cmake -S box -B build-box
cmake --build build-box
./build-box/octavio-box
```

Raspberry Pi OS: `sudo apt install cmake g++ libasound2-dev`, then the same three lines.

Options: `--buffer 128` (samples per block), `--rate 48000`, `--device n` (see `devices`), `--script file`
(commands run at start, for example a saved state).

## Playing it

- Parts start as MINI, PRO70, VACT and DR8 on MIDI channels 1-4. `chan` changes a part's channel; two parts on the
  same channel play together (a layer). A channel no part listens to plays the selected part.
- Every MIDI input and output is opened (looked for again every 2 s), plus virtual ports "Octavio Box" for a DAW. A port
  named after a Bluetooth device BlueZ knows (`bluetoothctl devices`) counts as Bluetooth (BLE MIDI: BlueZ built with
  MIDI support), the others as USB.
- CCs follow the plugin and the app: each engine's controls on CC 2-5, 7-31, 70-95, 102-119, 33-63 (see `params`).
  Any control (and any switch) can be given a CC by MIDI learn: a long press on it on the screen, then move a knob of
  the MIDI controller (on the part's channel). A learnt CC goes before the default one; it is saved in the multi
  (`cc <n> <engine> <control>`), and the screen shows it under the control.
- Tempo and sync (TEMPO & SYNC page, or `sync`): the clock is internal or follows MIDI clock from DIN MIDI, USB or
  Bluetooth; start / stop received or not; MIDI clock (and start / stop) sent to DIN, USB and / or Bluetooth; DIN MIDI
  thru on or off; a multi loads its tempo or not. Kept in `~/octavio/reglages.txt`.
- Type `help` for the commands. Example:

```
p 2              part 2
eng cs8          CS8 on it
params cutoff    its controls whose name has CUTOFF
set cutoff 0.7
fx slot          the FX slots
fxset slot0 7    reverb in slot 1 (0 = off, 1-16 = OVERDRIVE ... SHIMMER)
note 60 .8 500   play a note
bpm 100
play
status           engines, levels, CPU
chan 5           part 2 now listens to MIDI channel 5
savepart pad.txt this part's sound (p 4, then load pad.txt, copies it to part 4)
save live.txt    all 4 parts, as commands (load live.txt or --script live.txt)
```

## The screen

`ui/Screen.h` draws the synth's screen (Raspberry Pi Touch Display 2, 5", used in landscape: 1280 x 720), in a sober
hardware style: the page's title at the top, in the selected part's colour (the 1-4 buttons change the part), with
in its right corner the output's waveform (tap it: a tuner for the selected part), the tempo with its clock source
(tap it: TEMPO & SYNC) and a play / stop button; then the page chosen with the panel's page buttons. The 8 controls of a page sit 2 x 4 like the 8 encoders to
the right of the screen, each with a ring like the encoder's LED ring; a control that just moved lights up in the part's
colour. The pages are described below (front panel).

Touch: tap a part to select it, drag a control up or down to change it (double-tap = default value), tap the play button
to start / stop, on PRESETS (MIXER's tabs) tap a sound or a multi to pick it and tap it again to load it. The screen goes through the same logic as
the front panel (`Panel.h`), so the knobs, the buttons and the touch screen always agree.

It is drawn in software (`ui/Canvas.h`, about 2 ms per picture on a desktop CPU) and shown:
- on the Pi, straight on the display through KMS (no desktop needed): `--rotate 90` or `--rotate 270` turns the
  picture for the portrait panel (try the other one if it is upside down); the touch panel is read from `/dev/input`.
  `config.txt` needs `dtoverlay=vc4-kms-v3d` and `dtoverlay=vc4-kms-dsi-ili9881-5inch`; the user running octavio-box
  needs the `video` and `input` groups;
- on a computer, in a window when SDL2 is installed (`brew install sdl2`, `apt install libsdl2-dev`), `--zoom 0.75`
  for a smaller window. Keys stand in for the panel: 1-4 parts, `e v b f t m q x i p` pages, `[` `]` knob 9, space play.

`--no-screen` runs without it; `shot file.png` saves a picture of it (also in `--render`, for tests), `touch x y dy`
plays a finger, `page`, `knob` and `push` play the front panel from the terminal. With a screen, the synth keeps running
when the terminal closes (as a service). Fonts: DejaVu Sans Condensed (`third_party/fonts`, free licence).

## In a web browser

`box/web/` builds the same program for the browser (`WASI_SDK=/path/to/wasi-sdk box/web/build.sh`): the synth, the
panel's logic and the screen are compiled to WebAssembly (`boxwasm.cpp`) and `octavio-box.html` is one self-contained
page with the front panel around the screen. The sound runs in an AudioWorklet; the page keeps a copy of the synth for
the panel and the screen and posts each change to it. The virtual panel speaks the real panel's USB-MIDI protocol and
lights its rings the way the panel firmware does. Encoders: drag or wheel, click = push. Notes: the computer keyboard,
the keys under the panel or any Web MIDI input. Multis are kept in the browser.

## Offline render (no devices)

`octavio-box --render out.wav < script.txt` runs the commands of a script, where `wait <ms>` renders that much sound,
and writes a WAV. Build it alone with `-DOCTAVIO_NO_DEVICES=ON` (no audio library needed).

## Measured load

On one core of an Intel Xeon at 2.1 GHz (test server): 4 parts (J8, PRO70 and CS8 replaying 6-note chords, DR8
playing), each with 8 effects (reverb, shimmer, granular, delay, phaser, chorus, tape, overdrive) = about 29 % of one
core, 16 MB of memory. To be measured again on a Raspberry Pi 5.

## On the hardware synth (Raspberry Pi 5 + Octavio HAT + front panel)
`/boot/firmware/config.txt`:
```
dtoverlay=hifiberry-dacplusadcpro
dtoverlay=uart2-pi5
```
Then `octavio-box --device <n>` with the HiFiBerry card (see `devices`). On start it also:
- opens DIN MIDI on `/dev/ttyAMA2` at 31250 baud (`--serial` to change). MIDI IN plays like any other input; everything is echoed to MIDI OUT (thru);
- switches the headphone amplifier on (`--phones 0-63`, default 48; `phones` command while running);
- looks for the front panel ("Octavio Facade" USB-MIDI) every 2 s, so it can be plugged in at any time.

Files over USB (`systeme/`): `sudo sh systeme/usb-gadget.sh` once makes the Pi 5's USB-C port a USB device when a
computer is plugged into it (an Ethernet gadget; the box is 10.55.0.1 and `octavio.local`) and starts
`systeme/transfert.py`, a page at http://octavio.local: the multis (download one or all as a zip, upload, delete) and
the SAMPLER's WAV files (drag and drop files or whole folders, download, delete), straight on the SD card; the box
sees them at once. It answers only on the USB link. The screen says when a computer is connected. The computer then
powers the box through that port: it must give 3 A (most laptops' USB-C ports do), or a powered USB-C hub goes
between them. Adjust the user and paths in `systeme/octavio-transfert.service` if the box's user is not `octavio`.

Front panel (Panel.h): PART 1-4 select the part. The page buttons:
- ENGINE (tabs ENGINE, SEQUENCE on the synths, ARPEGGIATOR, CUSTOM): engine (in families: the mono synths, the poly synths, then the drums and the sampler), volume, pan, MIDI
  channel, tempo, ACCORD (the part's tuning, ±12 semitones, 5 cents a click) and VINTAGE (0 = clean; up: the pitch
  wanders a few cents, the filter breathes, the engine's own drift / vintage control is pushed up, and a soft
  saturation warms the part's output). Pushing ACCORD or VINTAGE resets it. Pressed again (or the CUSTOM tab): 8 knobs to fill with any of
  the engine's controls (tap a knob, or push an empty one, then pick in the list with knob 9), per part and engine,
  saved in the multi.
- VOICE and PAGE 2: the plugin's controls, section by section in the plugin's order (`PluginPages.h`, made again from
  the plugins' pages by `tools/plugin_pages.py`), 8 at a time, knob 9 moves along. PAGE 2 is the plugin's own extra
  page when it has one (SPECTRA on the VACT, PATCH MATRIX on the VC3), otherwise the second half of VOICE. The VC3's
  PATCH MATRIX is a pin board on the touch screen, as on the real one: sources in rows, destinations in columns; touch
  a hole and lift for a pin (white or green, chosen at the top), touch a pin to take it out. Knobs: 1 the row, 2 the
  column (push = pin in / out), 3 the pin's colour. On the DR8 it
  is PATTERN: one page per track, its 16 steps on the touch screen (tap: off, on, accent), knobs: level (push = mute),
  tune, swing, accent. On the SAMPLER, the first VOICE knob picks the pad the others edit, and PAGE 2 is PADS: the
  4 x 4 pads of a bank on the touch screen (tap = play and edit that pad), banks A-D, and LOAD A SOUND, a browser of
  the WAV files on the SD card (`~/octavio/samples`) and on USB sticks (`/media`): a file touched goes into the pad and
  plays. The samples are saved in the multi by their path (`sample <slot> <file>`). In the browser simulator, the SD
  card is the files added with « Carte SD » under the panel.
- The DX7 (the box's own engine, `core/synth/Dx7.h`, not in the plugins): 6-operator FM after the Yamaha DX7, 16 voices.
  The 32 algorithms and the operator feedback (the mean of the last two outputs, as the DX7's), the DX7's 4-rate / 4-level
  envelopes in its log domain (attack faster the further from the top, rate scaling by key, the 0.75 dB output level
  steps, a full-scale modulator = 2 cycles of phase), velocity sensitivity and key level scaling per operator, the
  pitch envelope, the DX7's LFO (speed, delay, wave, key sync, pitch and amplitude modulation with their
  sensitivities), portamento and MONO; BRIGHTNESS moves all the modulators' levels at once. The operators run at 2x
  from a sine table, then a 64-tap decimator. Pages: VOICE = ALGORITHM and OP 1-6 (ratio, fine, detune, level with its
  on / off, velocity, rate scaling, key level scaling, amp mod sens), PAGE 2 = LFO and PITCH EG, FILTER = the 6 levels
  with feedback and brightness side by side (FM has no filter: this is its timbre page), ENV = each operator's
  envelope. It starts on an electric piano; factory sounds: E.PIANO, BRASS, BASS, BELLS, STRINGS.
- LINE IN (the box's own engine, `core/synth/LineIn.h`): a part set to LINE IN plays the rear LINE IN jack (stereo,
  the HAT's PCM1863) instead of a synth, through the part's knobs, FX, mixer and outputs (OUT L/R, PHONES). VOICE =
  INPUT (gain in dB, STEREO / MONO / LEFT / RIGHT / SWAP, width, balance, drive, level, phase invert), PAGE 2 = GATE
  (OFF: always heard; NOTE: heard while a key or the sequencer holds a note, through the ADSR on ENV; LEVEL: a noise
  gate with its threshold), FILTER = low pass / high pass / band pass / notch, resonance, envelope amount and a low
  cut. LFOs, S&H, sequencers and the matrix as the other engines. Factory sounds: CLEAN, DUB FILTER, TRANCE GATE,
  TELEPHONE, NOISE GATE. octavio-box opens the output card's inputs (the HAT's ADC; `--input n` another device,
  `--input off` none); `--render` takes `--line-in file.wav`. In the browser: the "Entrée ligne" button (the
  computer's audio input).
- The DFAM (the box's own engine, `core/synth/Dfam.h`, not in the plugins): 8 tracks of a Moog DFAM style voice (2 VCOs
  triangle / square with 1-2 FM and sync, noise, ladder filter low / high pass, the VCO, VCF and VCA decay envelopes),
  laid out like the DR8: VOICE edits the track chosen with knob 1, PAGE 2 is PATTERN with each track's 16 steps and,
  under them, each step's pitch (drawn with a finger; SEQ PITCH sets how far they go). LFOs, matrix and SEQ as the others.
- FILTER and ENV: the engine's filter and envelope controls (taken out of VOICE).
- MOD: one page per LFO (1-4): rate, shape, destination, amount (the LFO's first matrix link, taken when needed), and
  under RATE, LFO or S&H (the RANDOM shape).
- SEQ: one page per step sequencer (1-4): knob 1 destination, knob 2 amount, the 16 steps drawn on the touch screen.
- FX: first CHAIN, the 8 effects in their order with their mix (push = that effect's page; on the screen, touch a box
  and turn knob 9 to change its effect, drag a box sideways to move it in the chain, as in the FX plugin), then one page per effect slot: knob 1 the effect,
  knob 2 its model, then its controls as in the FX plugin (web/fx-web, in `PluginPages.h`). MIXER: knobs 1-4 the parts' volumes (push = mute), 5-8 their pans.
- SIDECHAIN is one of the effects (the 17th, in any slot of the chain, as many times as wanted): it ducks the sound
  at that point of the chain. SOURCE: PART 1-4 (KEY: its sound, a level follower; its notes; or one of its drum tracks /
  pads 1-8), LFO (a smooth pump on the tempo, deepest on the beat) or SEQUENCE (16 steps, STEP 1-16 on the next page);
  RATE 1/1 to 1/16; ATTACK (0-300 ms), DECAY (10-2000 ms), AMOUNT. The box runs every part's engine first, then the
  effects, so the key is the same block's: no delay. In the FX plugin only the LFO and the sequence work.
- TEMPO & SYNC (no button: tap the tempo on the screen; a page button leaves it): tempo, clock (internal, MIDI,
  USB, Bluetooth), start / stop received, tap tempo (push knob 4 or tap the box in rhythm), clock sent, start / stop
  sent, MIDI thru, the multi's tempo. On the screen a tap changes a setting.
  Its second tab, QUANTIZER: the key (C..B), the scale (OFF, major / Ionian, minor / Aeolian, the other modes
  (Dorian, Phrygian, Lydian, Mixolydian, Locrian), harmonic and melodic minor, pentatonics, blues and major blues,
  whole tone, diminished, Phrygian dominant, Lydian dominant, altered, Hungarian minor, double harmonic, Hirajoshi,
  In Sen, bebop dominant, chromatic), the rounding (nearest, down, up) and which parts' ENGINE > SEQUENCE it
  pulls into the key (the SEQUENCE page shows the notes as they will play). Saved with the sync settings
  (`sync qroot 0-11`, `qscale 0-24`, `qmode 0-2`, `qparts 0-15`).
- MOD and SEQ have 8 LFOs and 8 sequencers, in tabs along the bottom; their first page, AMOUNTS, has the 8 amounts on
  the 8 knobs. LFOs and sequencers 1-4 are the engine's; 5-8 are the box's own (Track::bm, the engine's ext offsets),
  with the same destinations.
- ENGINE, after SEQUENCE: ARPEGGIATOR, the part's arpeggiator (on, mode up / down / up-down / random / as played,
  octaves, rate 1/4 to 1/32 and triplets, gate, latch), clocked by the tempo, on the notes played in MIDI.
- ENGINE, third tab on the synths: SEQUENCE, a 16-step note sequencer per part (note, rest / note / tie per step,
  length, rate, gate, transpose), running with play. REC: the notes played go into the steps (on the playhead when
  playing, step by step when stopped). On SEQ, REC makes the AMOUNT knob write the step under the playhead.
- Pages laid out by hand where the plugins' did not suit the panel (Panel.h PAGE_DEFS): VC3 (joystick pad, the matrix
  outputs as buttons), VACT (spectral sliders, a gate per page), EASEL, PRO70, J8, CS8 (ribbon to play, with its
  destination), SAMPLER (pads on VOICE, the waveform on PAGE 2), DR8 and DFAM (a tab per track).
- The J8's arpeggio and the J8's and CS8's chorus are kept off on the box (the arpeggiator above; the J8 chorus is a
  model of the FX chorus), and SEQ 5-6 stay silent.
- KEYS (the 10th page button, a little keyboard on its cap): the 9 other page buttons play notes on the part chosen,
  like a keyboard: the QUANTIZER's key and scale when it has one, else C major. Knob 9 turns the octave down or up
  (shown at the top of the screen: a little keyboard and C4). Pressed again, the buttons go back to their pages.
- PRESETS · <ENGINE> (MIXER pressed again, or its tab on the screen): the engine's 5 factory sounds for the part
  chosen (`sounds/factory.txt`, made into `FactorySounds.h` by `tools/sounds.py`; `sound 1-5` in the terminal). Knob 1
  picks, push (or a second tap, or LOAD) loads: every control of the engine back to its default, then the sound's.
  Synths: bass, lead, pad, keys... DR8 and DFAM: a kit and a pattern each; SAMPLER: the built-in kit, tuned and filtered.
- PRESETS · ALL TRACKS (MIXER pressed a third time): knob 1 picks a multi in `~/octavio`, push loads it, push knob 2 saves a new one under a name typed on the
  screen's keyboard (or with knob 1: turn = the last letter, push = one more letter, push knob 3 = erase, push knob 2 =
  save); on the screen: load, save under a name, save over the one picked.
At the bottom of the screen, tabs go straight to an LFO, a sequencer, an effect or a DR8 track; pressing a page button
again goes to the page's next tab (LFO 1, 2, 3, 4, 1...).
On / off switches and two-way choices (a waveform on, SYNC, SUB OCTAVE -1 / -2...) take no knob: they are buttons
along the bottom of the touch screen, under the knobs of their section (a tap flips one), so the pages are fewer.
Push knob 1-8 = default value; a level that has an on / off switch (the MINI's mixer and glide, a DR8 track's mute)
carries it on its push (double-tap on the screen = default). Push knob 9 = play / stop, volume pot = master volume.
The panel firmware and its protocol are in `facade-fw/`.

## Sound

- Every part's output is DC-blocked; knob moves glide over ~10 ms (no zipper); denormals are flushed on the audio thread.
- The ladder filters (MINI, A26, PRO70, J8 24 dB, VC3, DFAM) are tuned: the resonance lands on the cutoff and rings
  the same at every cutoff (core/synth/LadderTune.h). The DFAM's ladder runs at 2x.
- The EASEL and VACT wavefolders are antialiased (ADAA); the EASEL runs at 8x, the VC3 at 4x.
- The DR8's 808 drums (kick, snare tones, toms, congas, rim) are bridged-T resonators rung by the trigger pulse.
- `box/tools/bench.cpp`: CPU and aliasing per engine (`g++ -std=c++17 -O2 -I. -Icore box/tools/bench.cpp`).
- VINTAGE also gives each oscillator its own slow wander (EngineBase::vOsc) and each poly voice its own tolerances
  (J8: tuning, filter, envelope speed; CS8: tuning; PRO70 has its own VINTAGE, which the box pushes up).
- The ladders (MINI, A26, PRO70, J8 24 dB, VC3, DFAM) are zero-delay-feedback (LadderTune.h zdfLadder): exactly in
  tune at every cutoff, the same resonance everywhere. The oscillators' steps are 4-point (DspBlocks.h blep4),
  ~10 dB less aliasing at the top. The MS-20's EG1 attack is a capacitor's curve like the others'.
- FX: BBD chorus hiss that breathes with the signal (the compander) and antialiased clipping; tape delays with a
  playback head bump. DR8 808: hats and cymbal through their circuit's bands (7.1 kHz; 3.44 + 7.1 kHz), no noise.
- FX: the overdrives and the tape delays' saturations are antialiased (adaa in core/DspBlocks.h); the rack's output
  is clean up to 0.6 then a soft knee (it was a tanh that bent everything); modulated delays read with a cubic
  (chorus, flanger, micro pitch, tape wow); the hall, chamber, plate and room reverbs have an allpass inside each
  line of their network (denser tails, less ringing, same decay times).
- `box/tools/fxbench.cpp` (CPU, aliasing, top end, grain per FX model) and `box/tools/rvbench.cpp` (decay, echo
  density, ringing, width per reverb).
- The simulator's A/B button plays the old sound (synths and FX, before these passes) to compare by ear.
- MOD > LFO n: a SYNC button in the title bar (where REC is on the sequencer pages) puts that LFO on the tempo; its
  rate knob then picks a note value (4 bars to 1/32, with triplets) and its phase follows the box's beat (reset when
  play starts). LFOs 1-4 (the engine's) and 5-8 (the box's); saved with the part (`lfosync <0-7> <0/1>`).
- On / off: DÉFILER held 2 s (front panel v1.1 and its firmware, see cartes/facade/LISEZMOI.md). Running: the panel
  sends note 9, octavio-box saves the session (`~/octavio/.derniere-session`, loaded again at start) and powers off.
  Off: the panel, still fed by the 5 V header, pulses the Pi 5's power button (its J2 pads) and the Pi starts. The
  simulator does the same (dark screen and LEDs, sound suspended).
- The simulator's panel is drawn to scale from the enclosure's numbers (box/enclosure/enclosure.cpp; the screen at the
  Touch Display 2's 110.4 x 62.1 mm active area). Its "Taille" button shows the box at its real size (30.8 x 16.3 cm),
  calibrated once against a bank card (85.6 mm) held on the screen; both choices are remembered.
- The 4 parts render in parallel when the box is busy (box/PartPool.h): the audio thread and 2 helper threads on the
  Pi 5 (`--threads 0-3`), one core left to the screen and MIDI. Adaptive: while the load stays under ~35% of the block
  time the audio thread renders alone and the helpers sleep; past it (or past 80% on one block) they are woken, and
  they go back to sleep after a second under 20%. Parallel costs ~10-15% more CPU in total (waking, sharing), which is
  why it only runs when it buys headroom. The output is identical either way.
