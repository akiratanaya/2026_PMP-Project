#include "arduino_time.h"
#include <util/delay.h>
void delay_ms(uint16_t ms)
{
    while (ms > 0)
    {
        _delay_ms(1.0);
        ms--;
    }
}
