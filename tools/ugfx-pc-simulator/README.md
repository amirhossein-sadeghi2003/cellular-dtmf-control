# uGFX PC Simulator

A 240×320 PC prototype of the STM32F407 cellular-control user interface, built with uGFX v2.9 and SDL2.

The simulator supports developing and testing the GUI on Ubuntu before connecting it to the physical TFT, five-key keypad, STM32F407, and SIM800C hardware.

## Current Features

* Main menu with seven items:

  * Status
  * Call
  * DTMF
  * Network
  * Audio
  * Diagnostics
  * Settings
* Keyboard-based menu navigation
* Selected-item highlighting
* Implemented prototype pages:

  * Status
  * Call
  * DTMF
  * Network
  * Diagnostics
* Placeholder pages:

  * Audio
  * Settings
* Shared model for:

  * Modem state
  * Network registration
  * Signal strength
  * Call state
  * Caller information
  * Call duration
  * DTMF state and buffer
  * UART readiness
  * SIM readiness
  * Modem reset count
  * AT error count
  * Last diagnostic error
* Interactive DTMF simulation
* Dynamic modem-state simulation
* Dynamic network-registration simulation
* Dynamic call-state simulation
* Dynamic diagnostic fault simulation
* Time-driven active-call duration
* Manual signal-strength adjustment
* DTMF buffer clearing
* SDL2 window matching the target 240×320 TFT resolution

## Architecture

The simulator separates the reusable GUI and data model from the PC-specific SDL keyboard adapter.

### `main.c`

* Initializes uGFX
* Creates the simulator data model
* Reads keyboard events from the Linux SDL backend
* Processes every byte supplied by a keyboard event
* Maps navigation keys to platform-independent `UiKey` values
* Converts supported characters into simulated DTMF events
* Implements PC-only modem, network, call, signal, and diagnostic simulation commands
* Updates the active-call duration using uGFX system ticks
* Injects simulated diagnostic faults when `F` is pressed

### `ui.h`

* Defines platform-independent UI keys and actions
* Exposes the public UI interface
* Connects the UI module to the shared data model
* Has no direct dependency on SDL2 or STM32 HAL

### `ui.c`

* Stores the current page and selected menu item
* Draws the main menu and individual pages
* Implements the Status, Call, DTMF, Network, and Diagnostics pages
* Reads displayed values from `UiModel`
* Handles platform-independent navigation
* Redraws the current page through `uiRefresh()`

### `ui_model.h`

* Defines modem, network, and call states
* Defines the shared `UiModel` structure
* Defines caller, operator, DTMF buffer, and diagnostic error sizes
* Stores UART, SIM, modem reset, and AT error diagnostic values
* Exposes model initialization and DTMF functions

### `ui_model.c`

* Initializes the model to safe default values
* Initializes diagnostic fields
* Validates DTMF characters
* Stores the most recently received DTMF key
* Maintains a bounded 16-character DTMF buffer
* Clears the DTMF state when requested

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

The simulator expects these directories:

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

Create the simulator directory:

```bash
mkdir -p ~/projects/ugfx-pc-demo
```

Copy the required files:

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

Run it:

```bash
./.build/ugfx-pc-demo
```

## Simulator Controls

| Key                    | Action                                    |
| ---------------------- | ----------------------------------------- |
| `Up` / `Down`          | Move between menu items                   |
| `Enter`                | Open the selected page                    |
| `Left` / `Esc`         | Return from a page to the main menu       |
| `Esc` in the main menu | Exit the simulator                        |
| `0–9`, `*`, `#`, `A–D` | Add a simulated DTMF event                |
| `X`                    | Clear the DTMF buffer                     |
| `M`                    | Cycle through modem states                |
| `N`                    | Cycle through network-registration states |
| `R`                    | Cycle through call states                 |
| `F`                    | Cycle through diagnostic fault scenarios  |
| `+` / `-`              | Increase or decrease signal strength      |

Lowercase `a–d` DTMF characters are normalized to uppercase.

### Modem State Cycle

```text
OFFLINE → INITIALIZING → READY → ERROR → OFFLINE
```

### Network State Cycle

```text
NOT REGISTERED → SEARCHING → HOME → ROAMING → DENIED → NOT REGISTERED
```

The simulator also changes the demonstration operator and signal value when the network state changes.

### Call State Cycle

```text
IDLE → RINGING → ANSWERING → ACTIVE → ENDED → IDLE
```

While the call state is `ACTIVE`, the displayed duration increases once per second. The duration stops when the call leaves the active state and resets when a new simulated call begins.

The simulated ringing state includes a demonstration caller number.

### Diagnostic Scenario Cycle

Press `F` to cycle through the available diagnostic scenarios:

```text
AT TIMEOUT → UART ERROR → SIM NOT READY → MODEM RESET → NONE
```

The scenarios update the Diagnostics page as follows:

* `AT TIMEOUT` increments the AT error counter and places the modem in the error state
* `UART ERROR` marks the UART as unavailable and increments the AT error counter
* `SIM NOT READY` marks the SIM as unavailable
* `MODEM RESET` increments the modem reset counter and places the modem in the initializing state
* `NONE` restores the simulated UART, SIM, and modem to their healthy states

The reset and AT error counters remain available after the simulator returns to the healthy state.

### DTMF Example

Open the DTMF page and type:

```text
1234567890*#ABCD
```

The page displays:

* Whether DTMF detection is enabled
* The most recently received DTMF key
* The current DTMF buffer
* Whether the simulator is waiting for or has received a DTMF event

## Diagnostics Page

The Diagnostics page displays:

* UART readiness
* SIM readiness
* Modem reset count
* AT command error count
* Most recently recorded error

The current values are read from the shared `UiModel`, allowing the same page to display simulated data on the PC and real diagnostic data on the STM32F407.

## Initial Demonstration Data

The simulator initializes `UiModel` with these values:

* Modem: ready
* Network registration: home
* Signal strength: 26/31
* Call state: idle
* Auto answer: enabled
* DTMF detection: enabled
* Operator: `DEMO GSM`
* UART: ready
* SIM: ready
* Modem reset count: 0
* AT error count: 0
* Last diagnostic error: `NONE`

The GUI pages read these values from `UiModel`; modem and diagnostic data are not hard-coded inside the drawing functions.

## Hardware Integration Plan

The hardware version will replace simulator-specific components as follows:

* Linux SDL display backend → SPI TFT display driver
* PC keyboard navigation → five-key GPIO keypad
* Simulator commands → real STM32F407 and SIM800C events
* Keyboard-generated DTMF → real SIM800C DTMF events
* Simulated diagnostic faults → real firmware and modem diagnostics

The existing SIM800C parser and STM32F407 firmware can populate the shared model:

* `+CREG` → `network_state`
* `+CSQ` → `signal_rssi`
* Ring and call events → `call_state`
* Caller information → `caller`
* `+DTMF` → `uiModelAddDtmf()`
* UART initialization and failures → `uart_ready`
* `+CPIN: READY` → `sim_ready`
* Modem startup and reset events → `modem_reset_count`
* AT command timeouts and errors → `at_error_count` and `last_error`

After updating the model, the firmware can call `uiRefresh()` to redraw the visible page.

The reusable `ui.c`, `ui.h`, `ui_model.c`, and `ui_model.h` modules are designed to remain largely unchanged when transferred to the STM32F407 project.

## Next Steps

* Add more complete incoming and outgoing call scenarios
* Implement the Audio page
* Implement the Settings page
* Adapt the input layer for the physical five-key keypad
* Port the GUI from SDL2 to the target TFT display
* Connect the shared UI model to the existing STM32F407 and SIM800C firmware
