# Cellular DTMF Control

An embedded control system that uses cellular voice calls and DTMF keypad tones to control outputs through an STM32F407 microcontroller.

The project currently focuses on integrating an **SIM800C cellular module** with an **STM32F407VGT6**, receiving modem messages through UART, and displaying the results on a character LCD.

---

## Project Goal

The final system is intended to operate as follows:

```text
Incoming cellular call
        ↓
SIM800C receives the call
        ↓
Caller presses a DTMF key
        ↓
SIM800C reports the received key through UART
        ↓
STM32F407 processes the message
        ↓
LCD, LED, or relay output is controlled
```

For example, pressing the `1` key during a call may produce a message similar to:

```text
+DTMF: 1
```

The STM32 can then display the received digit or use it to control an output.

---

## Current Project Status

The following stages have been completed:

- Character LCD interface implemented in 4-bit mode
- LCD successfully tested on the STM32F407 board
- USART3 configured on the correct board pins
- UART communication established between STM32F407 and SIM800C
- `AT` command successfully transmitted to the module
- `OK` response successfully received from the module
- UART test result displayed on the LCD
- Initial DTMF feasibility studies completed
- Two-way audio hardware requirements investigated

Verified LCD output:

```text
SIM800C OK
UART3 WORKING
```

The UART test was completed without a SIM card. Network registration, voice calls, and received DTMF reporting have not yet been tested practically.

---

## Hardware

The current hardware platform includes:

- STM32F407VGT6 microcontroller
- Dideban v2.0 Board No. 13
- SIM800C cellular module on a custom interface board
- Character LCD compatible with the HD44780 interface
- GSM antenna
- ST-LINK V2 programmer
- External power supply

Hardware required for the next stage:

- Active SIM card with voice-call capability
- Available GSM network coverage
- A second phone for test calls
- Microphone and audio output hardware if two-way voice communication is required

---

## UART Connection

The SIM800C is connected to `USART3` of the STM32F407 through the PCB.

| Signal | STM32F407 pin | Direction |
|---|---|---|
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

The UART connection is used for commands and status messages such as:

```text
AT
OK
RING
NO CARRIER
+DTMF: 1
```

It does not carry the raw voice signal.

---

## LCD Connection

The LCD is operated in 4-bit mode.

| LCD signal | STM32F407 pin |
|---|---|
| RS | PE7 |
| RW | PE8 |
| EN | PE9 |
| D4 | PE10 |
| D5 | PE11 |
| D6 | PE12 |
| D7 | PE13 |

The LCD driver is divided into the following files:

```text
Core/Inc/lcd.h
Core/Src/lcd.c
Core/Src/main.c
```

---

## Verified UART Test

The current firmware sends the following command to the SIM800C:

```c
const uint8_t atCommand[] = "AT\r\n";
```

The command is transmitted through `USART3`:

```c
HAL_UART_Transmit(
    &huart3,
    (uint8_t *)atCommand,
    sizeof(atCommand) - 1U,
    1000
);
```

The response is received into a buffer:

```c
HAL_UART_Receive(
    &huart3,
    simResponse,
    sizeof(simResponse) - 1U,
    3000
);
```

The firmware searches the received response for `OK`:

```c
if (strstr((char *)simResponse, "OK") != NULL)
{
    LCD_Print("SIM800C OK");
}
```

This test confirmed:

- SIM800C power and startup
- Correct UART baud rate
- Correct USART3 pin assignment
- STM32-to-SIM800C transmission
- SIM800C-to-STM32 reception
- Successful LCD status display

The current receive method is blocking and is suitable only for the initial test. Interrupt- or DMA-based UART reception will be used for continuous modem messages.

---

## Planned DTMF Test

After installing the SIM card, the following sequence will be tested:

```text
1. Check SIM-card status using AT+CPIN?
2. Check network registration using AT+CREG?
3. Check signal strength using AT+CSQ
4. Receive an incoming voice call
5. Detect the RING message
6. Answer the call using ATA
7. Enable received-DTMF reporting
8. Press a key on the calling phone
9. Receive the DTMF message through UART
10. Display the received key on the LCD
11. Use the key to control an LED or relay
```

The final UART implementation should support:

- Continuous reception
- Complete message detection
- Interrupt or DMA reception
- A linear or circular receive buffer
- Prevention of duplicate DTMF key registration

---

## Two-Way Audio

UART communication and voice communication use separate paths.

### Audio from the board to the remote phone

```text
Local voice
    ↓
Microphone and input circuit
    ↓
MICP / MICN
    ↓
SIM800C
    ↓
Cellular network
```

### Audio from the remote phone to the board

```text
Cellular network
    ↓
SIM800C
    ↓
SPKP / SPKN
    ↓
Earpiece or audio amplifier and speaker
```

The audio pins are differential. They must not be connected to ground or external audio components without first checking the module interface circuit.

No clearly labeled `MIC` or `SPK` connector was identified on the current PCB. Therefore, the following pins must be traced on the custom SIM800C interface board before connecting audio hardware:

```text
MICP
MICN
SPKP
SPKN
```

The schematic should be checked first. If the schematic is incomplete, continuity testing can be performed with the board completely powered off.

For normal two-way voice communication, the audio signal does not need to pass through the STM32 ADC or DAC. The STM32 only manages the call through UART.

---

## Repository Structure

```text
cellular-dtmf-control/
├── docs/
│   ├── fa/
│   │   ├── docx/
│   │   │   ├── SIM5300EA-DTMF-STM32F407-report.docx
│   │   │   ├── SIM800C-DTMF-STM32F407-report.docx
│   │   │   └── SIM800C-two-way-audio-hardware-report-fa.docx
│   │   └── pdf/
│   │       ├── feasibility-study-fa.pdf
│   │       ├── SIM5300EA-DTMF-STM32F407-report.pdf
│   │       ├── SIM800C-DTMF-STM32F407-report.pdf
│   │       └── SIM800C-two-way-audio-hardware-report-fa.pdf
│   └── en/
│       ├── docx/
│       └── pdf/
├── firmware/
│   └── stm32f407-lcd-test/
│       ├── Core/
│       ├── Drivers/
│       └── stm32f407-lcd-test.ioc
├── .gitignore
└── README.md
```

The `fa` directory contains Persian reports. English versions will be added gradually to the `en` directory.

---

## Technical Reports

| Report | Persian PDF | Editable Persian Version | English Version |
|---|---|---|---|
| Initial cellular DTMF feasibility study | [PDF](docs/fa/pdf/feasibility-study-fa.pdf) | — | Planned |
| SIM5300EA DTMF study | [PDF](docs/fa/pdf/SIM5300EA-DTMF-STM32F407-report.pdf) | [DOCX](docs/fa/docx/SIM5300EA-DTMF-STM32F407-report.docx) | Planned |
| SIM800C DTMF feasibility study | [PDF](docs/fa/pdf/SIM800C-DTMF-STM32F407-report.pdf) | [DOCX](docs/fa/docx/SIM800C-DTMF-STM32F407-report.docx) | Planned |
| SIM800C two-way audio hardware study | [PDF](docs/fa/pdf/SIM800C-two-way-audio-hardware-report-fa.pdf) | [DOCX](docs/fa/docx/SIM800C-two-way-audio-hardware-report-fa.docx) | Planned |

PDF files are intended for normal viewing. DOCX files are retained as editable report sources.

---

## Development Tools

- STM32CubeIDE
- STM32CubeMX
- STM32CubeF4 firmware package
- STM32CubeProgrammer
- ST-LINK V2
- Git and GitHub

---

## Building the Firmware

1. Clone the repository:

```bash
git clone https://github.com/amirhossein-sadeghi2003/cellular-dtmf-control.git
```

2. Open the CubeMX project:

```text
firmware/stm32f407-lcd-test/stm32f407-lcd-test.ioc
```

3. Generate the project code if required.

4. Open the generated project in STM32CubeIDE.

5. Build the firmware.

6. Connect the STM32 board through SWD.

7. Flash and run the application.

---

## Next Steps

- Install and verify the SIM card
- Confirm network registration
- Measure GSM signal strength
- Test incoming calls
- Answer calls through AT commands
- Enable and verify DTMF reception
- Display received digits on the LCD
- Control an LED or relay using DTMF keys
- Replace blocking UART reception with interrupt or DMA reception
- Trace the SIM800C microphone and speaker pins
- Test two-way audio if required
- Add English versions of the technical reports

---

## Safety and Hardware Notes

- Power off the board before inserting or removing the SIM card.
- Keep the GSM antenna connected during network tests.
- Use a stable supply capable of supporting the SIM800C current requirements.
- Do not power the main board from the ST-LINK `5V` pin unless the board design explicitly supports it.
- Do not connect `SPKP`, `SPKN`, `MICP`, or `MICN` based only on visual assumptions.
- Verify all audio paths through the schematic or continuity testing before soldering.

---

## Author

**Amirhossein Sadeghi**

Computer Engineering student interested in embedded systems, IoT, computer networks, and real-time monitoring.
