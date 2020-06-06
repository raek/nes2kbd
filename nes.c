#include <stdbool.h>
#include <stdint.h>

#include <avr/io.h>

#include "nes.h"

#define SHIFT_PIN 2
#define LATCH_PIN 3
#define DATA_PIN  4

static clock_pin(uint8_t pin)
{
    PORTD &= ~(1<<pin);
    PORTD |= (1<<pin);
}

void nes_init(void)
{
    DDRD  |= ((1<<SHIFT_PIN) | (1<<LATCH_PIN) | (1<<DATA_PIN));
}

uint8_t nes_poll(void)
{
    uint8_t result = 0x00;
    clock_pin(LATCH_PIN);
    for (uint8_t i = 0; i < 8; i++) {
        bool bit = (PIND & (1<<DATA_PIN)) != 0;
        result <<= 1;
        result |= bit ? 0x00 : 0x01;
        clock_pin(SHIFT_PIN);
    }
    return result;
}