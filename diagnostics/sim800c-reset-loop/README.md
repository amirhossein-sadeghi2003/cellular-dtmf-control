# SIM800C Repeated Boot Diagnostic

## Purpose

This diagnostic program checks whether the SIM800C repeatedly boots during normal UART communication.

## Test behavior

- Waits 15 seconds for the initial modem boot
- Resets the displayed event counter to zero
- Sends only the read-only command `AT+CPIN?`
- Sends no software reset command
- Does not use `AT+CFUN=1,1`
- Does not control the PWRKEY pin
- Monitors the SIM800C UART output
- Increments the LCD counter whenever the exact startup message `RDY` is received

## Observed result

The LCD counter increased from 0 to 4 without resetting or reprogramming the STM32.

This confirms repeated SIM800C boot events during the test. The test does not determine the underlying cause. Possible causes still include power instability, unintended PWRKEY activity, or a hardware fault.
