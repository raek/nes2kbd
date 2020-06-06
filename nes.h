#ifndef NES_H
#define NES_H

#include <stdint.h>

#define BUTTON_A      7
#define BUTTON_B      6
#define BUTTON_SELECT 5
#define BUTTON_START  4
#define BUTTON_UP     3
#define BUTTON_DOWN   2
#define BUTTON_LEFT   1
#define BUTTON_RIGHT  0

void nes_init(void);
uint8_t nes_poll(void);

#endif
