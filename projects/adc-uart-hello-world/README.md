# ADC + UART Hello World

Reads a voltage divider on RA2, prints the result in millivolts over UART. First working project on the PIC16F15244 Curiosity Nano — basic enough to be a reference, annoying enough to have already hit one documented gotcha.

## Hardware

- PIC16F15244 Curiosity Nano
- Breadboard voltage divider: VCC → 15kΩ → **RA2** → 10kΩ → GND
  - Expected RA2 voltage ≈ VCC × (10k / 25k)

## Peripheral config (MCC Melody)

- **ADC**: Positive Reference = FVR, Result Alignment = right, channel = `ANA2` (RA2)
- **FVR**: enabled, buffer gain = 2x → 2.048V reference
- **EUSART1**: TX on RC0 (board's onboard USB-to-serial bridge), 9600 baud, 8N1

## Build / flash

1. Open this folder as a project in MPLAB X (`File → Open Project`)
2. Select the Curiosity Nano as the Connected Hardware Tool in Project Properties
3. Make and Program Device
4. Open a serial terminal (Tera Term, VS Code Serial Monitor, etc.) on the board's COM port at 9600 8N1

## Output

```
ADC: 1361 mV
ADC: 1359 mV
ADC: 1361 mV
```

## Known issue hit building this

If your terminal shows one repeating character instead of real output, that's not this code — see [`EUSART1_Write()` is non-blocking](../../docs/gotchas/eusart1-write-non-blocking.md).

## Conversion math

```c
millivolts = ((uint32_t)raw * 2048) / 1023;  // 2.048V FVR reference, 10-bit ADC
```
