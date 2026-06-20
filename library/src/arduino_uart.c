#include "arduino_uart.h"

#include <avr/io.h>

void uart_init(uint32_t baud_rate)
{
    uint16_t ubrr = (uint16_t)((F_CPU / (16UL * baud_rate)) - 1UL);
    // UBRR= Usart(Universal synchrnouse Ansynchronous Rec-Trans) baud rate register berukuran 8 bit
    // FCPU makro yang berisikan<
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0A = 0;
    UCSR0B = _BV(TXEN0) | _BV(RXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

void uart_write_char(char value)
{
    while ((UCSR0A & _BV(UDRE0)) == 0)
    {
    }

    UDR0 = (uint8_t)value;
}

void uart_write_string(const char *text)
{
    while (*text != '\0')
    {
        uart_write_char(*text);
        text++;
    }
}

#include <avr/pgmspace.h>
void uart_write_string_P(const char *text)
{
    char c;
    while ((c = (char)pgm_read_byte(text)) != '\0')
    {
        uart_write_char(c);
        text++;
    }
}

void uart_write_uint16(uint16_t value)
{
    char buffer[6];
    uint8_t index = 0;

    if (value == 0)
    {
        uart_write_char('0');
        return;
    }

    while (value > 0 && index < sizeof(buffer))
    {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0)
    {
        index--;
        uart_write_char(buffer[index]);
    }
}

void uart_write_newline(void)
{
    uart_write_char('\r');
    uart_write_char('\n');
}