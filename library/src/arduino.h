#ifndef ARDUINO_H
#define ARDUINO_H

#include "arduino_board.h"
#include "arduino_gpio.h"
#include "arduino_adc.h"
#include "arduino_pwm.h"
#include "arduino_uart.h"
#include "arduino_i2c.h"
#include "arduino_time.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_LOW  0
#define BOARD_HIGH 1

void board_init(void);

#ifdef __cplusplus
}
#endif

#endif