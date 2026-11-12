# 00 – Blink with GIO

**Topic:** GIO outputs and inputs
**Board:** LAUNCHXL2-570LC43 (TMS570LC4357ZWT)

## What it shows
The smallest complete program: HALCoGen-generated start-up code, the GIO driver,
one LED blinking, and a second LED that follows a user button. It also proves
the toolchain, the XDS110 connection, and flashing.

## Tools
- HALCoGen 04.07.01
- Code Composer Studio 21.0.1
- TI Arm Compiler 20.2.7.LTS

## HALCoGen configuration
- Device TMS570LC4357ZWT, TI tools.
- Driver Enable: GIO only.
- Application code lives in the `USER CODE` sections of
  `halcogen/source/HL_sys_main.c`, so regenerating does not overwrite it.

## Pins
| Function | Pin |
|----------|-----|
| LED A (blinks) | GIOB6 |
| LED B (follows button) | GIOB7 |
| User button B | GIOB5 (active low) |

## Behavior
LED A toggles roughly once per second. LED B is on while the button is held.

## Notes
- Timing uses a busy-wait loop and is approximate. `01-rti-blinky` replaces it
  with a timer interrupt.
- To build, import `ccs/` into CCS (*Import projects at their original
  locations*). See `docs/setup.md`.
