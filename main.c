#include <stdbool.h>
#include <avr/delay.h>
#include <avr/io.h>

#include "nes.h"
#include "kbd.h"

int main(void)
{
    kbd_init();
    for (;;) {
        uint8_t state = nes_poll();
        bool bit = false;
        for (uint8_t i = 0; i < 8; i++) {
            if ((state & 0x01) != 0) {
                bit = !bit;
            }
        }
        if (bit) {
            PORTD &= ~(1<<5);
        } else {
            PORTD |= (1<<5);
        }
        _delay_ms(1);
    }
    return 0;
}
