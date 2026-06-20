#include "arduino_pwm.h"
#include <avr/io.h>
#include "arduino_board.h"
static void pwm_setup(uint8_t pin)
{
    if (pin == 5 || pin == 6)
    {
        TCCR0A |= _BV(WGM00) | _BV(WGM01);
        TCCR0B |= _BV(CS01) | _BV(CS00);
        if (pin == 5)
        {
            TCCR0A |= _BV(COM0B1);
        }
        else
        {
            TCCR0A |= _BV(COM0A1);
        }
        return;
    }
    if (pin == 9 || pin == 10)
    {
        TCCR1A |= _BV(WGM10);
        TCCR1B |= _BV(WGM12) | _BV(CS11);
        if (pin == 9)
        {
            TCCR1A |= _BV(COM1A1);
        }
        else
        {
            TCCR1A |= _BV(COM1B1);
        }
        return;
    }
    if (pin == 3 || pin == 11)
    {
        TCCR2A |= _BV(WGM20) | _BV(WGM21);
        TCCR2B |= _BV(CS21) | _BV(CS20);
        if (pin == 3)
        {
            TCCR2A |= _BV(COM2B1);
        }
        else
        {
            TCCR2A |= _BV(COM2A1);
        }
    }
}
void pwm_write(uint8_t pin, uint8_t value)
{
    if (!board_pwm_pin_valid(pin))
    {
        return;
    }
    if (pin == 5)
    {
        DDRD |= _BV(DDD5);
        pwm_setup(pin);
        OCR0B = value;
        return;
    }
    if (pin == 6)
    {
        DDRD |= _BV(DDD6);
        pwm_setup(pin);
        OCR0A = value;
        return;
    }
    if (pin == 9)
    {
        DDRB |= _BV(DDB1);
        pwm_setup(pin);
        OCR1A = value;
        return;
    }
    if (pin == 10)
    {
        DDRB |= _BV(DDB2);
        pwm_setup(pin);
        OCR1B = value;
        return;
    }
    if (pin == 3)
    {
        DDRD |= _BV(DDD3);
        pwm_setup(pin);
        OCR2B = value;
        return;
    }
    DDRB |= _BV(DDB3);
    pwm_setup(pin);
    OCR2A = value;
}
