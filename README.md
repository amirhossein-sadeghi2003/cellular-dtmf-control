# Cellular DTMF Control

An embedded cellular control system based on an **STM32F407VGT6** microcontroller and a **SIM800C GSM module**.

The project receives incoming voice calls, answers them automatically, detects DTMF keypad tones, shows modem/call state on a graphical HMI, and can map DTMF commands to application actions. It also supports prerecorded voice playback into an active cellular call.

---

## System Overview

```text
Incoming cellular call
        ↓
SIM800C reports RING
        ↓
STM32 sends ATA
        ↓
Call becomes active
        ↓
STM32 enables DTMF detection
        ↓
Caller presses a DTMF key
        ↓
SIM800C reports +DTMF
        ↓
STM32 processes the key
        ↓
Graphical HMI / application action
```

The currently validated demonstration command is:

```text
DTMF 1
   ↓
AT+DTAM=1
   ↓
AT+CMEDPLAY=1,C:\voice.wav,0,100
   ↓
SIM800C plays the prerecorded Persian prompt
   ↓
Remote caller hears the audio
```

---

## Current Project Status

The cellular call, DTMF, filesystem, HMI, and prerecorded-audio path have all been validated incrementally on real hardware.

The primary integrated firmware is now:

```text
firmware/stm32f407-hmi-test
```

The character-LCD project is retained as a known-good diagnostic environment:

```text
firmware/stm32f407-lcd-test
```

Verified functionality includes:

- STM32F407 USART3 communication with SIM800C
- SIM-card readiness using `AT+CPIN?`
- GSM registration using `AT+CREG?`
- Signal-strength monitoring using `AT+CSQ`
- Incoming-call detection using `RING`
- Automatic call answering using `ATA`
- DTMF reporting using `AT+DDET`
- Multiple consecutive incoming calls
- Interrupt-driven UART reception
- Graphical ILI9341 HMI
- Live modem, network, signal, and call-state display
- SIM800C local-filesystem access
- File creation, deletion, writing, listing, and size verification
- WAV transfer from STM32 to SIM800C
- Media playback using `AT+CMEDPLAY`
- Audio playback into an active cellular call
- DTMF-triggered prerecorded Persian voice playback
- Repeated DTMF `1` playback during real calls
- Startup verification of `C:\voice.wav`
- Automatic voice-file recovery logic in the HMI firmware

The current embedded voice prompt is stored in STM32 Flash as WAV data with this format:

```text
PCM
Mono
8000 Hz
16-bit
101642 bytes
```

If `C:\voice.wav` already exists with the expected size, the HMI proceeds directly to the normal ready state. If the file is missing or has the wrong size, the firmware contains a recovery path that can delete, recreate, upload the WAV in chunks, and verify the final file size.

---

## Hardware Prototype

### STM32F407 + SIM800C HW-537

![Hardware overview](docs/images/hardware/hardware-overview.png)

The current prototype uses an STM32F407VGT6 development board connected to a SIM800C-based **HW-537** carrier board.

### SIM800C HW-537 Module

![SIM800C HW-537 module](docs/images/hardware/sim800c-hw537-module-redacted.jpg)

Device-specific identifiers in the public image have been redacted.

### Incoming Call and DTMF Test

![DTMF call test](docs/images/hardware/dtmf-call-test.png)

The character LCD was used extensively during low-level modem and filesystem testing. The integrated firmware now uses the graphical HMI.

---

## Hardware

Current hardware includes:

- STM32F407VGT6 microcontroller
- Dideban v2.0 development board
- SIM800C GSM/GPRS module on an HW-537 carrier board
- ILI9341 graphical display for the main HMI
- HD44780-compatible character LCD for diagnostic firmware
- GSM antenna
- ST-LINK V2 programmer
- External power source
- Active SIM card with GSM voice-call support
- Separate phone for incoming-call and DTMF testing

---

## SIM800C Power-Supply Debugging

Power integrity was one of the main hardware issues encountered during development.

With the original carrier-board power path, the SIM800C VBAT rail showed severe voltage sag during startup and GSM activity. Measurements showed the rail dropping below approximately 3 V in problematic conditions.

To isolate the carrier-board supply path, the module **VCC path was disconnected** and power was applied **directly to the SIM800C VBAT rail**.

For additional voltage drop from the available 5 V source, a **1N4002 series diode** was inserted. With one diode installed, the measured VBAT voltage was approximately:

```text
4.48 V
```

In this configuration the modem was able to start, register on the network, receive calls, detect DTMF, and play stored audio.

A second experiment used **two 1N4002 diodes in series**. This reduced the idle voltage to roughly 4.0 V, but the modem did not operate reliably and could shut down or fail to register. The likely cause was additional voltage drop under the SIM800C high-current GSM bursts.

The prototype was therefore returned to the **single-diode configuration**, where the measured operating voltage was about **4.48 V** and normal operation resumed.

> **Important:** 4.48 V is documented here as an experimental prototype measurement, not as the recommended final SIM800C supply voltage. It is above the SIM800C specified maximum VBAT voltage. A final hardware revision should use a properly regulated supply near 4.0–4.1 V with sufficient burst-current capability, short low-resistance power paths, and adequate low-ESR bulk/local decoupling.

---

## UART Connection

The SIM800C communicates with `USART3` of the STM32F407.

| Signal | STM32F407 pin | Direction |
| --- | --- | --- |
| USART3_TX | PD8 | STM32 → SIM800C |
| USART3_RX | PD9 | SIM800C → STM32 |

UART configuration:

```text
Baud rate:            115200
Word length:          8 bits
Parity:               None
Stop bits:            1
Hardware flow control: Disabled
Mode:                 Transmit and Receive
```

Typical modem messages include:

```text
OK
RING
NO CARRIER
+CLCC: ...
+DTMF: 5
```

UART carries AT commands, responses, unsolicited result codes, and file-upload data. During normal media playback, the SIM800C media subsystem generates the audio for the cellular call rather than continuously streaming raw voice audio through USART3.

---

## UART Receive Architecture

Cellular modem messages arrive asynchronously, so the integrated firmware uses interrupt-driven UART reception and a receive buffer.

```text
USART3 RX interrupt
        ↓
Received byte
        ↓
RX buffer
        ↓
Main-loop state machine
        ↓
AT response / URC parsing
        ↓
HMI and application state update
```

This allows unsolicited modem messages such as `RING`, `NO CARRIER`, and `+DTMF` to be captured while the application continues running.

---

## Validated Call Flow

The real-hardware call path is:

```text
AT communication
        ↓
CPIN check
        ↓
CREG network registration
        ↓
CSQ signal check
        ↓
Wait for RING
        ↓
Send ATA
        ↓
Enable DTMF detection
        ↓
Receive +DTMF notifications
```

During an active call, pressing a keypad digit can produce:

```text
+DTMF: 1
```

The firmware validates the received symbol and can map it to a device action.

---

## Recorded Voice Playback

The project can play a prerecorded Persian prompt to the remote caller without requiring a microphone.

The workflow is:

```text
Persian voice prompt
        ↓
Converted to WAV PCM
8000 Hz / mono / 16-bit
        ↓
Embedded in STM32 Flash
        ↓
Uploaded to SIM800C local filesystem when required
        ↓
Stored as C:\voice.wav
        ↓
Incoming call becomes active
        ↓
Caller presses DTMF 1
        ↓
AT+DTAM=1
        ↓
AT+CMEDPLAY=1,C:\voice.wav,0,100
        ↓
Remote caller hears the prompt
```

The current high-quality Persian test prompt was generated using TTS and converted to SIM800-compatible WAV format with FFmpeg.

The file is uploaded in chunks of up to 10240 bytes. The first write starts from the beginning of the file, and subsequent writes append data until the full WAV has been transferred. The firmware then verifies the final file size using `AT+FSFLSIZE`.

---

## SIM800C Filesystem

Filesystem features validated on real hardware include:

```text
AT+FSDRIVE=0
AT+FSMEM
AT+FSLS
AT+FSCREATE
AT+FSDEL
AT+FSWRITE
AT+FSFLSIZE
```

The local storage was successfully used for `voice.wav` and for small test files during development.

The integrated HMI firmware checks the expected voice-file size during startup. This removes the normal runtime dependency on the character-LCD diagnostic firmware.

---

## Modem Commands Used

The project currently uses or has validated commands including:

```text
AT
ATE0
AT+CPIN?
AT+CREG?
AT+CSQ
ATA
AT+CLCC
AT+DDET=1
AT+DTAM=1
AT+FSDRIVE=0
AT+FSMEM
AT+FSLS
AT+FSCREATE
AT+FSDEL
AT+FSWRITE
AT+FSFLSIZE
AT+CMEDPLAY=?
AT+CMEDPLAY=1,C:\voice.wav,0,100
```

These commands cover modem communication, SIM readiness, network registration, signal monitoring, incoming calls, DTMF reporting, filesystem management, file upload, size verification, audio routing, and prerecorded media playback.

---

## Graphical HMI

The integrated firmware uses an ILI9341-based graphical interface.

The HMI displays operational information such as:

- Modem state
- SIM readiness
- Network registration state
- Signal level
- Call state
- DTMF activity
- Diagnostic/error information

The graphical firmware is maintained in:

```text
firmware/stm32f407-hmi-test
```

The desktop UI development environment is maintained in:

```text
tools/ugfx-pc-simulator
```

---

## Character-LCD Diagnostic Firmware

The character-LCD project remains useful for isolated hardware debugging:

```text
firmware/stm32f407-lcd-test
```

It was used to validate the modem step by step before integrating the same behavior into the graphical HMI.

Major stages validated with this firmware include:

```text
AT communication
SIM readiness
network registration
signal strength
incoming calls
DTMF detection
filesystem access
file creation
chunked file upload
file-size verification
CMEDPLAY support
prerecorded audio during a real call
```

---

## Repository Structure

```text
cellular-dtmf-control/
├── docs/
│   ├── en/
│   │   ├── docx/
│   │   └── pdf/
│   ├── fa/
│   │   ├── docx/
│   │   └── pdf/
│   └── images/
│       └── hardware/
├── firmware/
│   ├── stm32f407-lcd-test/
│   │   └── diagnostic / hardware-isolation firmware
│   └── stm32f407-hmi-test/
│       └── integrated graphical firmware
├── tools/
│   └── ugfx-pc-simulator/
│       └── desktop HMI development environment
└── README.md
```

---

## Development Tools

- STM32CubeIDE
- STM32CubeMX
- STM32CubeF4 firmware package
- STM32CubeProgrammer
- ST-LINK V2
- FFmpeg
- Git
- GitHub

---

## Building the Firmware

Clone the repository:

```bash
git clone https://github.com/amirhossein-sadeghi2003/cellular-dtmf-control.git
cd cellular-dtmf-control
```

For the integrated graphical firmware, open:

```text
firmware/stm32f407-hmi-test
```

in STM32CubeIDE and build the required configuration.

For isolated modem/filesystem diagnostics, use:

```text
firmware/stm32f407-lcd-test
```

Connect the STM32F407 board through SWD using ST-LINK, build the selected project, and flash it from STM32CubeIDE.

The graphical firmware embeds the current voice WAV in Flash through:

```text
Core/Inc/sim800_voice_data.h
```

so its firmware image is intentionally larger than the earlier DTMF-only builds.

---

## Validation Notes

Development was performed incrementally on real hardware.

The following stages were individually verified:

```text
AT communication
CPIN readiness
CREG network registration
CSQ signal strength
incoming RING
automatic ATA
active voice call
DTMF reception
repeated incoming calls
SIM800 filesystem access
file creation
chunked file writing
file-size verification
WAV playback
remote caller hearing the playback
DTMF-triggered voice playback
graphical HMI integration
startup voice-file size check
```

The HMI has also been tested with repeated DTMF `1` commands and successfully plays the stored Persian voice prompt during an active call.

---

## Future Work

Planned development includes:

- Deliberately deleting or corrupting `voice.wav` and validating the full automatic recovery path end to end
- Mapping DTMF keys to real physical outputs
- Relay / LED control
- Full IVR-style command menu
- Multiple prerecorded voice prompts
- Device-status announcement
- Configurable multi-digit DTMF commands
- Caller authorization
- Caller-number validation
- Persistent configuration
- Improved modem-reset and UART-error recovery
- Dedicated regulated SIM800C power stage
- Long-duration stability testing
- Optional microphone and speaker hardware

A possible future IVR mapping is:

```text
1 → output ON
2 → output OFF
3 → report device status
0 → repeat menu
# → finish / hang up
```

---

## Safety Note

SIM800-class cellular modules can draw large short current pulses during GSM transmission.

Use a properly regulated supply, adequate local decoupling, short low-resistance power paths, and a common ground between the STM32 and modem interface.

The single-1N4002 / 4.48 V configuration described above is an experimental prototype result only. It should not be treated as the final production power solution.

Power should be disconnected before modifying modem wiring or SIM-card hardware.

---

## Author

**Amirhossein Sadeghi**

Computer Engineering student interested in embedded systems, IoT, computer networks, and real-time monitoring.
