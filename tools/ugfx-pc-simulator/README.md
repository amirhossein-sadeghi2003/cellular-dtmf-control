# uGFX PC Simulator

A 240×320 PC prototype of the STM32F407 user interface, built with uGFX v2.9 and SDL2.

## Current Features

* Main menu with seven items:

  * Status
  * Call
  * DTMF
  * Network
  * Audio
  * Diagnostics
  * Settings
* Keyboard navigation:

  * `Up` / `Down`: move between menu items
  * `Enter`: select an item
* Selected-item highlighting
* SDL2 window matching the target TFT resolution

## Requirements

* Ubuntu
* uGFX v2.9
* SDL2 development files
* GCC and Make

```bash
sudo apt install build-essential libsdl2-dev
```

The simulator expects uGFX to be available at:

```text
~/projects/ugfx
```

## Keyboard Backend Patch

The included patch fixes keyboard handling issues in the uGFX v2.9 Linux SDL backend, including incorrect key-buffer bounds, queued `KEYUP` events, and SDL key repetition.

Apply it from the uGFX directory:

```bash
cd ~/projects/ugfx
git apply ~/projects/cellular-dtmf-control/tools/ugfx-pc-simulator/patches/ugfx-v2.9-linux-sdl-keyboard.patch
```

## Build and Run

Copy `main.c`, `gfxconf.h`, and `Makefile` into the PC simulator directory, then build the project:

```bash
cd ~/projects/ugfx-pc-demo
make
./build/demo
```

## Next Steps

* Open real pages with `Enter`
* Return with `Left` or `Esc`
* Implement Status, Call, DTMF, and Network pages
* Add DTMF event simulation
* Connect the GUI to the real SIM800C firmware
