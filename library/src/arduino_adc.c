#include "arduino_adc.h"

#include <avr/io.h>

void adc_init(void)
{
    ADMUX = _BV(REFS0);
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}

uint16_t analog_read(uint8_t channel)
{
    if (channel > 5U)
    {
        return 0U;
    }

    ADMUX = (ADMUX & 0xF0U) | (channel & 0x0FU);
    ADCSRA |= _BV(ADSC);

    while ((ADCSRA & _BV(ADSC)) != 0U)
    {
    }

    return ADC;
}