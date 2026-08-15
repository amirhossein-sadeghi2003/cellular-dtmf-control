# uGFX PC Simulator

A 240×320 PC prototype of the STM32F407 cellular-control user interface, built with uGFX v2.9 and SDL2.

The simulator makes it possible to develop and test the GUI on Ubuntu before connecting it to the physical TFT, five-key keypad, STM32F407, and SIM800C hardware.

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
* Implemented prototype pages:

  * Status
  * Call
  * DTMF
  * Network
* Placeholder pages:

  * Audio
  * Diagnostics
  * Settings
* Shared UI data model for:

  * Modem state
  * Network registration
  * Signal strength
  * Call state
  * Caller information
  * Call duration
  * DTMF state and buffer
* Interactive DTMF simulation:

  * `0–9`
  * `*`
  * `#`
  * `A–D`
* A bounded DTMF buffer that stores the most recent 16 characters
* SDL2 window matching the target 240×320 TFT resolution

## Architecture

The simulator separates the reusable GUI and data model from the PC-specific keyboard input backend.

### `main.c`

* Initializes uGFX
* Creates and initializes the simulator data model
* Reads keyboard events from the Linux SDL backend
* Processes every byte provided by a `GEventKeyboard` event
* Maps PC navigation keys to platform-independent `UiKey` values
* Converts supported keyboard characters into simulated DTMF events
* Passes navigation events to the UI module

### `ui.h`

* Defines platform-independent UI keys and actions
* Exposes the public UI interface
* Connects the UI module to the shared data model
* Has no direct dependency on SDL2 or STM32 HAL

### `ui.c`

* Stores the current page and selected menu item
* Draws the main menu and individual pages
* Reads displayed values from `UiModel`
* Handles platform-independent navigation events
* Redraws the current page through `uiRefresh()`

### `ui_model.h`

* Defines modem, network, and call states
* Defines the shared `UiModel` structure
* Defines caller, operator, and DTMF buffer sizes
* Exposes model initialization and DTMF update functions

### `ui_model.c`

* Initializes the shared model to safe default states
* Validates DTMF characters
* Stores the most recently received DTMF key
* Maintains the bounded DTMF buffer
* Clears DTMF state when requested

### `gfxconf.h`

* Configures the required uGFX display, event, timer, and keyboard modules

### `Makefile`

* Builds the Linux SDL simulator
* Compiles `main.c`, `ui.c`, and `ui_model.c`

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

The included patch fixes keyboard-handling problems in the uGFX v2.9 Linux SDL backend:

* Uses the actual number of key-buffer entries for bounds checking
* Prevents accumulated `KEYUP` events
* Ignores SDL-generated key-repeat events
* Posts key-down notifications only when a key-down event is queued
* Synchronizes keyboard queue inspection and consumption with the shared mutex
* Prevents timing-dependent loss of characters such as `#`

The patch is intended for a clean uGFX v2.9 checkout.

Apply it from the uGFX directory:

```bash
cd ~/projects/ugfx
git apply ~/projects/cellular-dtmf-control/tools/ugfx-pc-simulator/patches/ugfx-v2.9-linux-sdl-keyboard.patch
```

## Simulator Setup

Create the PC simulator directory:

```bash
mkdir -p ~/projects/ugfx-pc-demo
```

Copy the simulator files:

```bash
cp ~/projects/cellular-dtmf-control/tools/ugfx-pc-simulator/{main.c,ui.c,ui.h,ui_model.c,ui_model.h,gfxconf.h,Makefile} \
   ~/projects/ugfx-pc-demo/
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

## DTMF Simulation

Open the DTMF page and type supported DTMF characters on the PC keyboard.

Example:

```text
1234567890*#ABCD
```

The page displays:

* Whether DTMF detection is enabled
* The most recently received DTMF key
* The current DTMF buffer
* Whether the simulator is waiting for or has received a DTMF event

Lowercase `a–d` characters are normalized to uppercase.

## Current GUI Data

The simulator initializes `UiModel` with demonstration values:

* Modem: ready
* Network registration: home
* Signal strength: 26/31
* Call state: idle
* Auto answer: enabled
* DTMF detection: enabled
* Operator: `DEMO GSM`

The UI pages do not contain hard-coded modem data. They display the values stored in the shared model.

## Hardware Integration Plan

The final hardware version will replace the simulator-specific components as follows:

* Linux SDL display backend → SPI TFT display driver
* PC keyboard navigation → five-key GPIO keypad
* Demonstration model values → real STM32F407 and SIM800C state
* Keyboard DTMF simulation → real SIM800C DTMF events

The existing SIM800C parser can update the shared model:

* `+CREG` → `network_state`
* `+CSQ` → `signal_rssi`
* Ring and call events → `call_state`
* Caller information → `caller`
* `+DTMF` → `uiModelAddDtmf()`

After a model update, the firmware can call `uiRefresh()` to redraw the currently visible page.

The reusable `ui.c`, `ui.h`, `ui_model.c`, and `ui_model.h` modules are designed to remain largely unchanged when transferred to the STM32F407 project.

## Next Steps

* Add a command to clear the DTMF buffer
* Add dynamic call-state simulation
* Add dynamic network and signal-strength simulation
* Implement the Audio page
* Implement the Diagnostics page
* Implement the Settings page
* Adapt the input layer for the physical five-key keypad
* Port the GUI from SDL2 to the target TFT display
* Connect the shared UI model to the existing STM32F407 and SIM800C firmware
