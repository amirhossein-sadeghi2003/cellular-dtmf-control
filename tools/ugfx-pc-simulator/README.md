# uGFX PC Simulator

A 240×320 PC prototype of the STM32F407 cellular-control user interface, built with uGFX v2.9 and SDL2.

The simulator makes it possible to develop and test the GUI on Ubuntu before connecting it to the physical TFT, keypad, STM32F407, and SIM800C hardware.

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
  * `Enter`: open the selected page
  * `Left` / `Esc`: return from a page to the main menu
  * `Esc` in the main menu: exit the simulator
* Selected-item highlighting
* Static prototype pages for:

  * Status
  * Call
  * DTMF
  * Network
* Placeholder pages for:

  * Audio
  * Diagnostics
  * Settings
* Simulated modem, call, DTMF, and network information
* SDL2 window matching the target 240×320 TFT resolution

## Requirements

* Ubuntu
* uGFX v2.9
* SDL2 development files
* GCC
* Make

Install the required Ubuntu packages:

```bash
sudo apt install build-essential libsdl2-dev
```

The simulator expects the following directories:

```text
~/projects/cellular-dtmf-control
~/projects/ugfx
~/projects/ugfx-pc-demo
```

## uGFX Setup

Clone uGFX and check out version v2.9:

```bash
cd ~/projects
git clone https://github.com/ugfx/ugfx.git
cd ~/projects/ugfx
git checkout v2.9
```

## Keyboard Backend Patch

The included patch fixes keyboard-handling problems in the uGFX v2.9 Linux SDL backend, including:

* Incorrect key-buffer bounds
* Accumulated `KEYUP` events
* SDL-generated key repetition

The patch is intended for a clean uGFX v2.9 checkout.

Apply it from the uGFX directory:

```bash
cd ~/projects/ugfx
git apply ~/projects/cellular-dtmf-control/tools/ugfx-pc-simulator/patches/ugfx-v2.9-linux-sdl-keyboard.patch
```

## Simulator Setup

Create the PC simulator directory and copy the required project files:

```bash
mkdir -p ~/projects/ugfx-pc-demo

cp ~/projects/cellular-dtmf-control/tools/ugfx-pc-simulator/main.c \
   ~/projects/ugfx-pc-demo/main.c

cp ~/projects/cellular-dtmf-control/tools/ugfx-pc-simulator/gfxconf.h \
   ~/projects/ugfx-pc-demo/gfxconf.h

cp ~/projects/cellular-dtmf-control/tools/ugfx-pc-simulator/Makefile \
   ~/projects/ugfx-pc-demo/Makefile
```

## Build and Run

Build the simulator:

```bash
cd ~/projects/ugfx-pc-demo
make clean
make -j"$(nproc)"
```

Run the generated executable:

```bash
./.build/ugfx-pc-demo
```

## Current GUI Behavior

The Status, Call, DTMF, and Network pages currently display simulated values. They are intended to validate screen layout and keypad navigation before hardware integration.

The Audio, Diagnostics, and Settings pages are placeholders and will be implemented in later stages.

## Next Steps

* Add interactive DTMF simulation
* Add dynamic call-state simulation
* Add dynamic network and signal-strength simulation
* Implement the Audio page
* Implement the Diagnostics page
* Implement the Settings page
* Adapt the input layer for the physical five-key keypad
* Port the GUI from SDL2 to the target TFT display
* Connect the GUI to the STM32F407 and SIM800C firmware
