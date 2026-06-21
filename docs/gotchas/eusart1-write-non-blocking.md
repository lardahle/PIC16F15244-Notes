# MCC Melody's generated `EUSART1_Write()` is non-blocking — here's what that breaks

**Chip:** PIC16F15244 (likely applies to other PIC16F1xxxx parts using MCC Melody's basic EUSART driver)
**Tools:** MPLAB X IDE, MCC Melody, XC8

## Symptom

You wire up UART, generate the project with MCC, write a basic `printf`-style line over serial, and your terminal shows this:

```
A
A
A
A
A
A
```

One character, repeating, each on its own line. Not garbage/garbled bytes — a single clean ASCII character, consistently, forever.

## Setup that triggers it

A typical "print a formatted line over UART" helper:

```c
void UART_PrintStr(const char *str) {
    while (*str) {
        EUSART1_Write(*str++);
    }
}
```

Called like:

```c
UART_PrintStr("ADC: ");
UART_PrintNum(millivolts);
UART_PrintStr(" mV\r\n");
```

Reasonable code. If you've written bare-metal UART before, this looks correct — you're sending bytes one at a time in a loop. The bug isn't in this code. It's in what MCC generated underneath it.

## Root cause

Open `mcc_generated_files/uart/src/eusart1.c` and look at the generated write function:

```c
void EUSART1_Write(uint8_t txData)
{
    TX1REG = txData;
}
```

That's it. No wait, no flag check. Compare that to what you'd write by hand against the datasheet:

```c
void UART_SendChar(char c) {
    while (!TX1STAbits.TRMT);   // wait until transmit shift register is empty
    TX1REG = c;
}
```

The hardware UART has two stages: `TX1REG` (a one-byte buffer you write to) and the **TSR** (transmit shift register, the thing that actually clocks bits out the pin over time — roughly 1ms per byte at 9600 baud). Writing `TX1REG` only *queues* a byte. If the TSR is still busy shifting out the previous byte, the new byte sits in `TX1REG` until the TSR frees up — and if you write to `TX1REG` *again* before that handoff happens, you silently overwrite whatever was waiting there. No error, no flag, it just vanishes.

MCC's generated `EUSART1_Write()` has no awareness of this. It assumes you'll check readiness yourself using `EUSART1_IsTxReady()` — which exists in the same file, just isn't called for you:

```c
bool EUSART1_IsTxReady(void)
{
    return (bool)(PIR1bits.TX1IF && TX1STAbits.TXEN);
}
```

So here's what actually happens when you loop-call `EUSART1_Write()` for every character in `"ADC: 1234 mV\r\n"`:

1. The CPU runs at 32MHz. Writing a register takes nanoseconds. Transmitting one byte over UART at 9600 baud takes ~1ms — five-plus orders of magnitude slower than the loop that's calling `EUSART1_Write()`.
2. First byte (`'A'`) gets written. `TSR` was idle, so it loads immediately and starts shifting out — that takes the full ~1ms.
3. Every subsequent character in your string (`D`, `C`, `:`, the digits, `\r`, `\n`) gets written to `TX1REG` within that same ~1ms window, each one overwriting the last, because the loop finishes essentially instantly relative to the hardware.
4. By the time the TSR finishes shifting `'A'` out and looks for the next queued byte, `TX1REG` only holds whatever was written *last* — the final character in your string, `'\n'`.
5. `'\n'` loads into the TSR and transmits.

Net result on the wire: `'A'`, then `'\n'`. Repeat every loop iteration. Hence: one character, then a newline, forever.

## Fix

Wait for ready before every write — exactly what you'd do bare-metal, just using the generated readiness check instead of touching `TX1STAbits.TRMT` directly:

```c
void UART_PrintStr(const char *str) {
    while (*str) {
        while (!EUSART1_IsTxReady());
        EUSART1_Write(*str++);
    }
}
```

That's the entire fix. One line.

## Why doesn't MCC just generate this correctly?

Worth noting this isn't universal across all MCC-generated EUSART drivers — some configurations (particularly interrupt-driven/buffered modes on other PIC16/18 parts) generate a ring-buffer-backed write function that handles this internally via `eusart1TxBuffer`/ISR. The basic/polling mode generated here apparently doesn't, and just exposes the raw register write plus a separate readiness check, leaving composition up to you. If you picked a different EUSART config in MCC (interrupt-driven TX), this may not apply — check your generated `eusart1.c` for a TX buffer/ISR before assuming you have the same bug.

## Takeaway

Generated driver code is not automatically "safe by default" just because a config tool wrote it. `EUSART1_Write()` *reads* like a complete send-one-byte function. It isn't — it's a raw register poke with a name that implies more than it does. Check what the generator actually emitted before trusting the abstraction, especially for anything timing-sensitive like UART, SPI, or I2C.
