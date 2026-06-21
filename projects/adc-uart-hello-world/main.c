#include "mcc_generated_files/system/system.h"

#define NUM_SAMPLES 8

void UART_PrintStr(const char *str) {
    while (*str) {
        while (!EUSART1_IsTxReady());
        EUSART1_Write(*str++);
    }
}

void UART_PrintNum(uint32_t num) {
    char buf[12];
    uint8_t i = 0;

    if (num == 0) {
        while (!EUSART1_IsTxReady());
        EUSART1_Write('0');
        return;
    }

    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    while (i > 0) {
        while (!EUSART1_IsTxReady());
        EUSART1_Write(buf[--i]);
    }
}

void main(void) {
    SYSTEM_Initialize();

    while (!FVR_IsOutputReady());  // wait for FVR to stabilize

    uint32_t accumulator = 0;
    uint16_t raw = 0;
    uint32_t millivolts = 0;

    while (1) {
        accumulator = 0;

        for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
            accumulator += ADC_ChannelSelectAndConvert(ADC_CHANNEL_ANA2);
        }

        raw = accumulator / NUM_SAMPLES;
        millivolts = ((uint32_t)raw * 2048) / 1023;

        UART_PrintStr("ADC: ");
        UART_PrintNum(millivolts);
        UART_PrintStr(" mV\r\n");

        __delay_ms(1000);
    }
}