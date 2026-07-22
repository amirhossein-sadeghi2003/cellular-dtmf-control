# Cellular DTMF Control with SIM5300EA and STM32F407

## Overview

This project investigates a cellular-based control system in which a user calls a SIM card installed in a cellular module and presses a keypad digit to control an LED connected to an STM32F407 microcontroller.

The current stage focuses on evaluating the required hardware, communication interfaces, and available methods for detecting DTMF signals during a voice call.

## Key Findings

The initial design considered the SIM5300E module. However, the available hardware documentation indicates that SIM5300E does not provide an accessible audio interface, while the SIM5300EA variant supports audio input and output.

Therefore, SIM5300EA is the more suitable module for accessing the audio signal of an active voice call.

The public AT command documentation includes commands for generating and transmitting DTMF tones, but no documented command was found for reporting received DTMF digits directly through UART.

Two decoding approaches are therefore being considered:

1. Hardware decoding using a DTMF decoder such as MT8870.
2. Software decoding using STM32F407 ADC sampling and the Goertzel algorithm.

## Proposed System Architecture

<pre>
Mobile Phone
     |
Cellular Voice Call
     |
SIM5300EA
     |
Analog Audio Output
     |
     +---------------------------+
     |                           |
MT8870 DTMF Decoder       STM32 ADC Sampling
     |                    and Goertzel Algorithm
     |                           |
     +------------+--------------+
                  |
              STM32F407
                  |
              LED Control
</pre>

## Hardware Decoding Option

In the hardware-based approach, the analog audio output of SIM5300EA is connected to an external DTMF decoder such as MT8870.

The decoder detects the two DTMF frequencies and provides a digital code that can be read by STM32F407 GPIO pins.

This approach is simpler and more suitable for the initial prototype.

## Software Decoding Option

In the software-based approach, the audio signal is conditioned and sampled using the STM32F407 ADC.

The Goertzel algorithm is then used to measure the energy of the standard DTMF frequencies and determine which keypad digit was pressed.

This approach reduces the number of external components but requires additional analog circuitry, signal sampling, and software processing.

## Current Status

- Initial feasibility study completed
- SIM5300E and SIM5300EA capabilities compared
- Audio interface requirements identified
- Hardware and software DTMF decoding methods evaluated
- STM32F407 selected as the main controller
- Hardware implementation not started yet

## Documentation

- [Persian feasibility report](docs/feasibility-study-fa.pdf)

## Planned Work

- Test basic GPIO and LED control on STM32F407
- Establish UART communication with the cellular module
- Test SIM card registration and incoming calls
- Evaluate DTMF decoding using MT8870
- Evaluate ADC sampling and the Goertzel algorithm
- Connect detected keypad digits to LED control commands

## Repository Structure

<pre>
cellular-dtmf-control/
├── README.md
└── docs/
    └── feasibility-study-fa.pdf
</pre>

## Disclaimer

This repository currently contains a feasibility study and a proposed system architecture.

Hardware implementation, source code, circuit diagrams, and experimental results will be added in later stages.
