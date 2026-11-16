# Octavio Facade firmware (RP2040)

The firmware of the front-panel board: a USB-MIDI device that reads the 9 encoders, the 14 buttons and the volume pot,
and drives the 110 WS2812B LEDs. The protocol is in [PROTOCOLE.md](PROTOCOLE.md).

## Flash it
Hold BOOT (or short the BOOT pads) while plugging in USB: the board shows up as a drive named RPI-RP2.
Copy `octavio_facade.uf2` to it. It restarts as "Octavio Facade".

## Build it
Needs the Pico SDK 2.x and an `arm-none-eabi-gcc`:
```
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S box/facade-fw -B build-fw -DCMAKE_BUILD_TYPE=Release
cmake --build build-fw          # -> build-fw/octavio_facade.uf2
```

## Settings in main.c
- `ENC_DIR`: -1 if the knobs turn the wrong way.
- `LED_LIMIT_MA`: LED current cap (1200 mA by default, under the 2 A fuse on the main board).
- The USB IDs are the pid.codes test IDs (1209:0001): fine for your own unit.
