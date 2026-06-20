#include "arduino_time.h"

#include <util/delay.h>

void delay_ms(uint16_t ms)
{
    while (ms > 0)
    // 0u disini gunanya untuk memastikan bahwa 0 adalah unsigned integer
    {
        _delay_ms(1.0);
        // Ini fungsi bawaan dari library <util/delay.h>
        ms--;
    }
}