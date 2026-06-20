#ifndef ARDUINO_UART_H
#define ARDUINO_UART_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void uart_init(uint32_t baud_rate);
void uart_write_char(char value);
void uart_write_string(const char *text);
void uart_write_string_P(const char *text);
void uart_write_uint16(uint16_t value);
void uart_write_newline(void);
#ifdef __cplusplus
}
#endif
#endif
