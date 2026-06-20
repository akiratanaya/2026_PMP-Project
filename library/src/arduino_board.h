#ifndef ARDUINO_BOARD_H
#define ARDUINO_BOARD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void board_init(void);
uint8_t board_pin_count(void);
volatile uint8_t *board_ddr(uint8_t pin);
volatile uint8_t *board_port(uint8_t pin);
volatile uint8_t *board_pin(uint8_t pin);
uint8_t board_bit(uint8_t pin);
uint8_t board_pwm_pin_valid(uint8_t pin);
#ifdef __cplusplus
}
#endif
#endif
