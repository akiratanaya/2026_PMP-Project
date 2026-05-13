#ifndef ARDUINO_GPIO_H
#define ARDUINO_GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_LOW  0
#define BOARD_HIGH 1

typedef enum
{
    BOARD_INPUT = 0,
    BOARD_OUTPUT = 1,
    BOARD_INPUT_PULLUP = 2
} board_pin_mode_t;

void pin_mode(uint8_t pin, board_pin_mode_t mode);
void digital_write(uint8_t pin, uint8_t value);
uint8_t digital_read(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif