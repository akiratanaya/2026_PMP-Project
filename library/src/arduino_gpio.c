#include "arduino_gpio.h"
#include "arduino_board.h"
void pin_mode(uint8_t pin, board_pin_mode_t mode)
{
    volatile uint8_t *ddr = board_ddr(pin);
    volatile uint8_t *port = board_port(pin);
    uint8_t bit = board_bit(pin);
    if (ddr == 0 || port == 0 || bit == 0)
    {
        return;
    }
    if (mode == BOARD_OUTPUT)
    {
        *ddr |= bit;
        *port &= (uint8_t)~bit;
    }
    else if (mode == BOARD_INPUT_PULLUP)
    {
        *ddr &= (uint8_t)~bit;
        *port |= bit;
    }
    else
    {
        *ddr &= (uint8_t)~bit;
        *port &= (uint8_t)~bit;
    }
}
void digital_write(uint8_t pin, uint8_t value)
{
    volatile uint8_t *port = board_port(pin);
    uint8_t bit = board_bit(pin);
    if (port == 0 || bit == 0)
    {
        return;
    }
    if (value != 0)
    {
        *port |= bit;
    }
    else
    {
        *port &= (uint8_t)~bit;
    }
}
uint8_t digital_read(uint8_t pin)
{
    volatile uint8_t *pin_reg = board_pin(pin);
    uint8_t bit = board_bit(pin);
    if (pin_reg == 0 || bit == 0)
    {
        return 0;
    }
    return ((*pin_reg & bit) != 0) ? 1 : 0;
}
