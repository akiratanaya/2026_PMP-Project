#ifndef ARDUINO_PWM_H
#define ARDUINO_PWM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void pwm_write(uint8_t pin, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif