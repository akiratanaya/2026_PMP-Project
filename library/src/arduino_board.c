#include "arduino_board.h"

#include <avr/io.h>

#if !defined(BOARD_UNO_R3)
#define BOARD_UNO_R3
#endif

#if defined(BOARD_UNO_R3)

#define BOARD_PIN_COUNT 20

static volatile uint8_t *const ddr_map[BOARD_PIN_COUNT] = {
    &DDRD, &DDRD, &DDRD, &DDRD, &DDRD, &DDRD, &DDRD, &DDRD,
    &DDRB, &DDRB, &DDRB, &DDRB, &DDRB, &DDRB,
    &DDRC, &DDRC, &DDRC, &DDRC, &DDRC, &DDRC
};

static volatile uint8_t *const port_map[BOARD_PIN_COUNT] = {
    &PORTD, &PORTD, &PORTD, &PORTD, &PORTD, &PORTD, &PORTD, &PORTD,
    &PORTB, &PORTB, &PORTB, &PORTB, &PORTB, &PORTB,
    &PORTC, &PORTC, &PORTC, &PORTC, &PORTC, &PORTC
};

static volatile uint8_t *const pin_map[BOARD_PIN_COUNT] = {
    &PIND, &PIND, &PIND, &PIND, &PIND, &PIND, &PIND, &PIND,
    &PINB, &PINB, &PINB, &PINB, &PINB, &PINB,
    &PINC, &PINC, &PINC, &PINC, &PINC, &PINC
};

static const uint8_t bit_map[BOARD_PIN_COUNT] = {
    _BV(PD0), _BV(PD1), _BV(PD2), _BV(PD3), _BV(PD4), _BV(PD5), _BV(PD6), _BV(PD7),
    _BV(PB0), _BV(PB1), _BV(PB2), _BV(PB3), _BV(PB4), _BV(PB5),
    _BV(PC0), _BV(PC1), _BV(PC2), _BV(PC3), _BV(PC4), _BV(PC5)
};

uint8_t board_pin_count(void)
{
    return BOARD_PIN_COUNT;
}

volatile uint8_t *board_ddr(uint8_t pin)
{
    return (pin < BOARD_PIN_COUNT) ? ddr_map[pin] : 0;
}

volatile uint8_t *board_port(uint8_t pin)
{
    return (pin < BOARD_PIN_COUNT) ? port_map[pin] : 0;
}

volatile uint8_t *board_pin(uint8_t pin)
{
    return (pin < BOARD_PIN_COUNT) ? pin_map[pin] : 0;
}

uint8_t board_bit(uint8_t pin)
{
    return (pin < BOARD_PIN_COUNT) ? bit_map[pin] : 0;
}

uint8_t board_pwm_pin_valid(uint8_t pin)
{
    return (pin == 3) || (pin == 5) || (pin == 6) || (pin == 9) || (pin == 10) || (pin == 11);
}

void board_init(void)
{
}

#endif