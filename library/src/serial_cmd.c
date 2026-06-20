#include <avr/pgmspace.h>
#include "serial_cmd.h"
#include "inventory.h"
#include "arduino_uart.h"
#include "arduino_board.h"
#include "arduino_adc.h"
#include "arduino_i2c.h"
#include <avr/io.h>
#include <string.h>

#define I2C_REMOTE_SLAVE_ADDRESS 0x08
#define I2C_REMOTE_MAX_TRANSFER 16

/* Case insensitive string compare - Refactored to void */
static void str_eq_ci(const char *a, const char *b, uint8_t *result)
{
    *result = 0;
    while ((*a != '\0') && (*b != '\0'))
    {
        char ca = *a;
        char cb = *b;
        if ((ca >= 'a') && (ca <= 'z')) ca = (char)(ca - ('a' - 'A'));
        if ((cb >= 'a') && (cb <= 'z')) cb = (char)(cb - ('a' - 'A'));
        if (ca != cb)
        {
            return;
        }
        a++;
        b++;
    }
    *result = ((*a == '\0') && (*b == '\0')) ? 1 : 0;
}

/* Parse uint16 - Refactored to void */
static void parse_uint16_value(const char *text, uint16_t *value, uint8_t *result)
{
    uint32_t tmp = 0;
    *result = 0;

    if ((text == 0) || (value == 0))
    {
        return;
    }

    while (*text == ' ')
    {
        text++;
    }

    if (*text == '\0')
    {
        return;
    }

    while (*text != '\0')
    {
        if ((*text < '0') || (*text > '9'))
        {
            return;
        }

        tmp = (tmp * 10UL) + (uint32_t)(*text - '0');
        if (tmp > 65535UL)
        {
            return;
        }
        text++;
    }

    *value = (uint16_t)tmp;
    *result = 1;
}

/* Parse uint8 list - Refactored to void */
static void parse_uint8_list(char *text, uint8_t *values, uint8_t max_values, uint8_t *count, uint8_t *result)
{
    uint8_t parsed = 0;
    *result = 0;

    if ((text == 0) || (values == 0) || (count == 0))
    {
        return;
    }

    while ((text != 0) && (*text != '\0') && (parsed < max_values))
    {
        char *comma;
        uint16_t number = 0;

        while (*text == ' ')
        {
            text++;
        }

        comma = strchr(text, ',');
        if (comma != 0)
        {
            *comma = '\0';
        }

        uint8_t parse_ok;
        parse_uint16_value(text, &number, &parse_ok);
        if (!parse_ok || (number > 255))
        {
            return;
        }

        values[parsed++] = (uint8_t)number;

        if (comma == 0)
        {
            break;
        }

        text = comma + 1;
    }

    *count = parsed;
    *result = (parsed > 0) ? 1 : 0;
}

char cmd_input_line[64];
char *cmd_param_str = NULL;
char *cmd_tokens[1];
uint8_t cmd_token_count = 0;

static void cmd_reset(void)
{
    memset(cmd_input_line, 0, sizeof(cmd_input_line));
    cmd_param_str = NULL;
    cmd_token_count = 0;
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
    scan_slaves();
    cmd_reset();
    uart_write_string_P(PSTR("System ready\r\n"));
}

/* UART read line - Refactored to void with global result */
static uint8_t uart_read_line_result = 0;
static void uart_read_line(void)
{
    static uint8_t pos = 0;
    uart_read_line_result = 0;
    /* check RX complete */
    if ((UCSR0A & _BV(RXC0)) == 0) return;
    char c = (char)UDR0;
    if (c == '\r') return;
    if (c == '\n')
    {
        cmd_input_line[pos] = '\0';
        pos = 0;
        uart_read_line_result = 1;
        return;
    }
    if (pos < (sizeof(cmd_input_line) - 1))
    {
        cmd_input_line[pos++] = c;
    }
}

static void parse_command(void)
{
    cmd_param_str = NULL;
    cmd_token_count = 0;
    cmd_tokens[0] = NULL;

    char *p = cmd_input_line;
    while (*p == ' ') p++;
    if (*p == '\0') return;
    /* first token */
    cmd_tokens[0] = p;
    cmd_token_count = 1;
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
    uint8_t eq1, eq2;

    str_eq_ci(cmd, "ADD", &eq1); str_eq_ci(cmd, "A", &eq2);
    if (eq1 || eq2)
    {
        inventory_add();
    }
    else if (str_eq_ci(cmd, "DEL", &eq1), str_eq_ci(cmd, "D", &eq2), (eq1 || eq2))
    {
        inventory_delete();
    }
    else if (str_eq_ci(cmd, "FIND", &eq1), str_eq_ci(cmd, "F", &eq2), (eq1 || eq2))
    {
        inventory_find();
    }
    else if (str_eq_ci(cmd, "STOCK", &eq1), str_eq_ci(cmd, "S", &eq2), (eq1 || eq2))
    {
        inventory_update_stock();
    }
    else if (str_eq_ci(cmd, "STATUS", &eq1), str_eq_ci(cmd, "T", &eq2), (eq1 || eq2))
    {
        inventory_update_status();
    }
    else if (str_eq_ci(cmd, "LIST", &eq1), str_eq_ci(cmd, "L", &eq2), (eq1 || eq2))
    {
        inventory_list();
    }
    else if (str_eq_ci(cmd, "SUMMARY", &eq1), str_eq_ci(cmd, "R", &eq2), (eq1 || eq2))
    {
        inventory_summary();
    }
    else if (str_eq_ci(cmd, "EXPORT", &eq1), str_eq_ci(cmd, "X", &eq2), (eq1 || eq2))
    {
        inventory_export();
    }
    else if (str_eq_ci(cmd, "HELP", &eq1), str_eq_ci(cmd, "H", &eq2), (eq1 || eq2))
    {
        uart_write_string_P(PSTR("Perintah (bisa huruf kecil, bisa alias):\r\n"));
        uart_write_string_P(PSTR("A/ADD id,name,cat,stock,loc,status,owner,pic\r\n"));
        uart_write_string_P(PSTR("D/DEL id\r\nF/FIND id\r\nS/STOCK id,delta\r\n"));
        uart_write_string_P(PSTR("T/STATUS id,status\r\nL/LIST\r\nR/SUMMARY\r\nX/EXPORT\r\n"));
    }
    else if (str_eq_ci(cmd, "SAVE", &eq1), str_eq_ci(cmd, "SV", &eq2), (eq1 || eq2))
    {
        inventory_save_eeprom();
    }
    else if (str_eq_ci(cmd, "LOAD", &eq1), str_eq_ci(cmd, "LD", &eq2), (eq1 || eq2))
    {
        inventory_load_eeprom();
    }
    else if (str_eq_ci(cmd, "CLEAR", &eq1), str_eq_ci(cmd, "C", &eq2), (eq1 || eq2))
    {
        inventory_clear();
    }
    else if (str_eq_ci(cmd, "SCAN", &eq1), (eq1 != 0))
    {
        scan_slaves();
        uart_write_string_P(PSTR("SCAN: found "));
        uart_write_uint16(num_slaves);
        uart_write_string_P(PSTR(" slave(s)\r\n"));
        for (uint8_t i = 0; i < num_slaves; i++)
        {
            uart_write_string_P(PSTR("  Slave["));
            uart_write_uint16(i);
            uart_write_string_P(PSTR("] addr=0x"));
            uart_write_uint16(slave_addrs[i]);
            uart_write_newline();
        }
    }
    else if (str_eq_ci(cmd, "SADDR", &eq1), (eq1 != 0))
    {
        /* SADDR old_hex,new_hex  e.g. SADDR 8,9 */
        if (cmd_param_str == NULL)
        {
            uart_write_string_P(PSTR("SADDR: expected old,new\r\n"));
        }
        else
        {
            char tmp2[16];
            strncpy(tmp2, cmd_param_str, sizeof(tmp2) - 1);
            tmp2[sizeof(tmp2) - 1] = '\0';
            char *comma2 = strchr(tmp2, ',');
            if (comma2 == NULL)
            {
                uart_write_string_P(PSTR("SADDR: expected old,new\r\n"));
            }
            else
            {
                *comma2 = '\0';
                uint16_t old_a = 0, new_a = 0;
                uint8_t ok1, ok2;
                parse_uint16_value(tmp2, &old_a, &ok1);
                parse_uint16_value(comma2 + 1, &new_a, &ok2);
                if (ok1 && ok2)
                {
                    uint8_t result;
                    set_slave_addr((uint8_t)old_a, (uint8_t)new_a, &result);
                    if (result == 0x01)
                    {
                        uart_write_string_P(PSTR("SADDR: OK\r\n"));
                    }
                    else
                    {
                        uart_write_string_P(PSTR("SADDR: FAILED\r\n"));
                    }
                }
                else
                {
                    uart_write_string_P(PSTR("SADDR: invalid address\r\n"));
                }
            }
        }
    }
    else if (str_eq_ci(cmd, "IWRITE", &eq1), str_eq_ci(cmd, "IW", &eq2), (eq1 || eq2))
    {
        char tmp[64];
        uint8_t values[I2C_REMOTE_MAX_TRANSFER];
        uint8_t value_count = 0;
        uint8_t i2c_status = I2C_ERROR;
        uint16_t offset = 0;

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
        
        uint8_t parse_ok;
        parse_uint16_value(tmp, &offset, &parse_ok);
        if (!parse_ok)
        {
            uart_write_string_P(PSTR("IWRITE: invalid offset\r\n"));
            return;
        }

        parse_uint8_list(comma + 1, values, I2C_REMOTE_MAX_TRANSFER, &value_count, &parse_ok);
        if (!parse_ok)
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
    else if (str_eq_ci(cmd, "IREAD", &eq1), str_eq_ci(cmd, "IR", &eq2), (eq1 || eq2))
    {
        char tmp[64];
        uint8_t values[I2C_REMOTE_MAX_TRANSFER];
        uint8_t i2c_status = I2C_ERROR;
        uint16_t offset = 0;
        uint16_t length = 0;
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

        uint8_t parse_ok;
        parse_uint16_value(tmp, &offset, &parse_ok);
        if (!parse_ok)
        {
            uart_write_string_P(PSTR("IREAD: invalid offset\r\n"));
            return;
        }

        parse_uint16_value(comma + 1, &length, &parse_ok);
        if (!parse_ok || (length == 0) || (length > I2C_REMOTE_MAX_TRANSFER))
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
        for (i = 0; i < (uint8_t)length; ++i)
        {
            uart_write_uint16(values[i]);
            if (i + 1 < (uint8_t)length)
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
    uart_read_line();
    if (uart_read_line_result)
    {
        parse_command();
        dispatch_command();
        cmd_reset();
    }
}
