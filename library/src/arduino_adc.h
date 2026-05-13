#ifndef ARDUINO_ADC_H
#define ARDUINO_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void adc_init(void);
uint16_t analog_read(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif