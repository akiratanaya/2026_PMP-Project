#include <avr/pgmspace.h>
#include "serial_cmd.h"
#include "inventory.h"
#include "arduino_uart.h"
#include "arduino_board.h"
#include "arduino_adc.h"
#include "arduino_i2c.h"
#include <avr/io.h>
#include <string.h>

#define I2C_REMOTE_SLAVE_ADDRESS 0x08U
#define I2C_REMOTE_MAX_TRANSFER 16U

static uint8_t str_eq_ci(const char *a, const char *b)
{
    while ((*a != '\0') && (*b != '\0'))
    {
        char ca = *a;
        char cb = *b;
        if ((ca >= 'a') && (ca <= 'z')) ca = (char)(ca - ('a' - 'A'));
        if ((cb >= 'a') && (cb <= 'z')) cb = (char)(cb - ('a' - 'A'));
        if (ca != cb)
        {
            return 0U;
        }
        a++;
        b++;
    }
    return ((*a == '\0') && (*b == '\0')) ? 1U : 0U;
}

static uint8_t parse_uint16_value(const char *text, uint16_t *value)
{
    uint32_t result = 0UL;

    if ((text == 0) || (value == 0))
    {
        return 0U;
    }

    while (*text == ' ')
    {
        text++;
    }

    if (*text == '\0')
    {
        return 0U;
    }

    while (*text != '\0')
    {
        if ((*text < '0') || (*text > '9'))
        {
            return 0U;
        }

        result = (result * 10UL) + (uint32_t)(*text - '0');
        if (result > 65535UL)
        {
            return 0U;
        }
        text++;
    }

    *value = (uint16_t)result;
    return 1U;
}

static uint8_t parse_uint8_list(char *text, uint8_t *values, uint8_t max_values, uint8_t *count)
{
    uint8_t parsed = 0U;

    if ((text == 0) || (values == 0) || (count == 0))
    {
        return 0U;
    }

    while ((text != 0) && (*text != '\0') && (parsed < max_values))
    {
        char *comma;
        uint16_t number = 0U;

        while (*text == ' ')
        {
            text++;
        }

        comma = strchr(text, ',');
        if (comma != 0)
        {
            *comma = '\0';
        }

        if (!parse_uint16_value(text, &number) || (number > 255U))
        {
            return 0U;
        }

        values[parsed++] = (uint8_t)number;

        if (comma == 0)
        {
            break;
        }

        text = comma + 1;
    }

    *count = parsed;
    return (parsed > 0U) ? 1U : 0U;
}

char cmd_input_line[64];
char *cmd_param_str = NULL;
char *cmd_tokens[1];
uint8_t cmd_token_count = 0U;

static void cmd_reset(void)
{
    memset(cmd_input_line, 0, sizeof(cmd_input_line));
    cmd_param_str = NULL;
    cmd_token_count = 0U;
    cmd_tokens[0] = NULL;
}

void system_init(void)
{
    board_init();
    adc_init();
    uart_init(9600UL);
    i2c_master_init(100000UL);
    inventory_init();
    inventory_load_eeprom();
    cmd_reset();
    uart_write_string_P(PSTR("System ready\r\n"));
}

static uint8_t uart_read_line(void)
{
    static uint8_t pos = 0U;
    /* check RX complete */
    if ((UCSR0A & _BV(RXC0)) == 0U) return 0U;
    char c = (char)UDR0;
    if (c == '\r') return 0U;
    if (c == '\n')
    {
        cmd_input_line[pos] = '\0';
        pos = 0U;
        return 1U;
    }
    if (pos < (sizeof(cmd_input_line) - 1))
    {
        cmd_input_line[pos++] = c;
    }
    return 0U;
}

static void parse_command(void)
{
    cmd_param_str = NULL;
    cmd_token_count = 0U;
    cmd_tokens[0] = NULL;

    char *p = cmd_input_line;
    while (*p == ' ') p++;
    if (*p == '\0') return;
    /* first token */
    cmd_tokens[0] = p;
    cmd_token_count = 1U;
    while (*p != ' ' && *p != '\0') p++;
    if (*p == ' ')
    {
        *p = '\0';
        p++;
        while (*p == ' ') p++;
        if (*p != '\0')
        {
            cmd_param_str = p;
        }
    }
}

static void dispatch_command(void)
{
    if (cmd_tokens[0] == NULL) return;
    char *cmd = cmd_tokens[0];
    if (str_eq_ci(cmd, "ADD") || str_eq_ci(cmd, "A"))
    {
        inventory_add();
    }
    else if (str_eq_ci(cmd, "DEL") || str_eq_ci(cmd, "D"))
    {
        inventory_delete();
    }
    else if (str_eq_ci(cmd, "FIND") || str_eq_ci(cmd, "F"))
    {
        inventory_find();
    }
    else if (str_eq_ci(cmd, "STOCK") || str_eq_ci(cmd, "S"))
    {
        inventory_update_stock();
    }
    else if (str_eq_ci(cmd, "STATUS") || str_eq_ci(cmd, "T"))
    {
        inventory_update_status();
    }
    else if (str_eq_ci(cmd, "LIST") || str_eq_ci(cmd, "L"))
    {
        inventory_list();
    }
    else if (str_eq_ci(cmd, "SUMMARY") || str_eq_ci(cmd, "R"))
    {
        inventory_summary();
    }
    else if (str_eq_ci(cmd, "HELP") || str_eq_ci(cmd, "H"))
    {
        uart_write_string_P(PSTR("Perintah (bisa huruf kecil, bisa alias):\r\n"));
        uart_write_string_P(PSTR("A/ADD id,name,cat,stock,loc,status,owner,pic\r\n"));
        uart_write_string_P(PSTR("D/DEL id\r\nF/FIND id\r\nS/STOCK id,delta\r\n"));
        uart_write_string_P(PSTR("T/STATUS id,status\r\nL/LIST\r\nR/SUMMARY\r\n"));
    }
    else if (str_eq_ci(cmd, "SAVE") || str_eq_ci(cmd, "SV"))
    {
        inventory_save_eeprom();
    }
    else if (str_eq_ci(cmd, "LOAD") || str_eq_ci(cmd, "LD"))
    {
        inventory_load_eeprom();
    }
    else if (str_eq_ci(cmd, "CLEAR") || str_eq_ci(cmd, "C"))
    {
        inventory_clear();
    }
    else if (str_eq_ci(cmd, "IWRITE") || str_eq_ci(cmd, "IW"))
    {
        char tmp[64];
        uint8_t values[I2C_REMOTE_MAX_TRANSFER];
        uint8_t value_count = 0U;
        uint8_t i2c_status = I2C_ERROR;
        uint16_t offset = 0U;

        if (cmd_param_str == 0)
        {
            uart_write_string_P(PSTR("IWRITE: missing params\r\n"));
            return;
        }

        strncpy(tmp, cmd_param_str, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';

        char *comma = strchr(tmp, ',');
        if (comma == 0)
        {
            uart_write_string_P(PSTR("IWRITE: expected offset,data...\r\n"));
            return;
        }
        *comma = '\0';
        if (!parse_uint16_value(tmp, &offset))
        {
            uart_write_string_P(PSTR("IWRITE: invalid offset\r\n"));
            return;
        }

        if (!parse_uint8_list(comma + 1, values, I2C_REMOTE_MAX_TRANSFER, &value_count))
        {
            uart_write_string_P(PSTR("IWRITE: invalid data\r\n"));
            return;
        }

        i2c_status = i2c_master_mem_write(I2C_REMOTE_SLAVE_ADDRESS, offset, values, value_count);
        if (i2c_status == I2C_OK)
        {
            uart_write_string_P(PSTR("IWRITE: OK\r\n"));
        }
        else
        {
            uart_write_string_P(PSTR("IWRITE: FAILED code="));
            uart_write_uint16(i2c_status);
            uart_write_newline();
        }
    }
    else if (str_eq_ci(cmd, "IREAD") || str_eq_ci(cmd, "IR"))
    {
        char tmp[64];
        uint8_t values[I2C_REMOTE_MAX_TRANSFER];
        uint8_t i2c_status = I2C_ERROR;
        uint16_t offset = 0U;
        uint16_t length = 0U;
        uint8_t i;

        if (cmd_param_str == 0)
        {
            uart_write_string_P(PSTR("IREAD: missing params\r\n"));
            return;
        }

        strncpy(tmp, cmd_param_str, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';

        char *comma = strchr(tmp, ',');
        if (comma == 0)
        {
            uart_write_string_P(PSTR("IREAD: expected offset,length\r\n"));
            return;
        }
        *comma = '\0';

        if (!parse_uint16_value(tmp, &offset))
        {
            uart_write_string_P(PSTR("IREAD: invalid offset\r\n"));
            return;
        }

        if (!parse_uint16_value(comma + 1, &length) || (length == 0U) || (length > I2C_REMOTE_MAX_TRANSFER))
        {
            uart_write_string_P(PSTR("IREAD: invalid length\r\n"));
            return;
        }

        i2c_status = i2c_master_mem_read(I2C_REMOTE_SLAVE_ADDRESS, offset, values, (uint8_t)length);
        if (i2c_status != I2C_OK)
        {
            uart_write_string_P(PSTR("IREAD: FAILED code="));
            uart_write_uint16(i2c_status);
            uart_write_newline();
            return;
        }

        uart_write_string_P(PSTR("IREAD: "));
        for (i = 0U; i < (uint8_t)length; ++i)
        {
            uart_write_uint16(values[i]);
            if (i + 1U < (uint8_t)length)
            {
                uart_write_char(',');
            }
        }
        uart_write_newline();
    }
    else
    {
      
        if (strchr(cmd_input_line, ',') == NULL)
        {
            cmd_param_str = cmd_input_line;
            inventory_find();
        }
        else
        {
            uart_write_string_P(PSTR("Unknown command. Type HELP\r\n"));
        }
    }
}

void system_loop(void)
{
    if (uart_read_line())
    {
        parse_command();
        dispatch_command();
        cmd_reset();
    }
}
