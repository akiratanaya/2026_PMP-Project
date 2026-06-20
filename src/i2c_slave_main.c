#include "arduino_board.h"
#include "arduino_gpio.h"
#include "arduino_time.h"
#include "arduino_i2c.h"
#include "inventory.h"
#include <avr/eeprom.h>
#include <string.h>

#define I2C_SLAVE_MEMORY_SIZE 64
#define SLAVE_ADDR_EEPROM_BYTE ((uint8_t *)1023)
#define SLAVE_ADDR_MIN 0x08
#define SLAVE_ADDR_MAX 0x0F
#define SLAVE_ADDR_DEFAULT 0x08

static uint8_t slave_memory[I2C_SLAVE_MEMORY_SIZE];

void uart_write_string(const char *s) { (void)s; }
void uart_write_string_P(const char *s) { (void)s; }
void uart_write_char(char c) { (void)c; }
void uart_write_uint16(uint16_t val) { (void)val; }
void uart_write_newline(void) {}
char cmd_input_line[64];
char *cmd_param_str = NULL;
char *cmd_tokens[1];
uint8_t cmd_token_count = 0;
static void get_slave_i2c_address(uint8_t *result)
{
    uint8_t addr = eeprom_read_byte(SLAVE_ADDR_EEPROM_BYTE);
    if (addr < SLAVE_ADDR_MIN || addr > SLAVE_ADDR_MAX)
    {
        *result = SLAVE_ADDR_DEFAULT;
        return;
    }
    *result = addr;
}
int main(void)
{
    board_init();
    inventory_init();
    inventory_load_eeprom();
    pin_mode(13, BOARD_OUTPUT);
    digital_write(13, BOARD_LOW);
    memset(slave_memory, 0, sizeof(slave_memory));
    uint8_t my_addr;
    get_slave_i2c_address(&my_addr);
    i2c_slave_init(my_addr, slave_memory, I2C_SLAVE_MEMORY_SIZE);
    while (1)
    {
        if (i2c_slave_take_dirty_flag() != 0)
        {
            uint8_t cmd = slave_memory[0];
            if (cmd != 0)
            {
                digital_write(13, BOARD_HIGH);
                last_op_status = 0;
                if (cmd == 0x01) 
                {
                    cmd_param_str = (char *)&slave_memory[2];
                    inventory_add();
                    slave_memory[1] = last_op_status;
                }
                else if (cmd == 0x02) 
                {
                    cmd_param_str = (char *)&slave_memory[2];
                    inventory_delete();
                    slave_memory[1] = last_op_status;
                }
                else if (cmd == 0x03) 
                {
                    int16_t idx;
                    find_index_by_id((char *)&slave_memory[2], &idx);
                    if (idx == -1)
                    {
                        slave_memory[1] = 0x02;
                    }
                    else
                    {
                        memcpy(&slave_memory[2], &node_pool[idx], sizeof(inventory_node_t));
                        slave_memory[1] = 0x01;
                    }
                }
                else if (cmd == 0x04) 
                {
                    cmd_param_str = (char *)&slave_memory[2];
                    inventory_update_stock();
                    slave_memory[1] = last_op_status;
                }
                else if (cmd == 0x05) 
                {
                    cmd_param_str = (char *)&slave_memory[2];
                    inventory_update_status();
                    slave_memory[1] = last_op_status;
                }
                else if (cmd == 0x06) 
                {
                    uint8_t target_idx = 0;
                    char *p = (char *)&slave_memory[2];
                    while (*p >= '0' && *p <= '9') {
                        target_idx = (uint8_t)(target_idx * 10 + (uint8_t)(*p - '0'));
                        p++;
                    }
                    int16_t cur = list_head;
                    uint8_t count = 0;
                    while (cur != -1 && count < target_idx)
                    {
                        cur = node_pool[cur].next;
                        count++;
                    }
                    if (cur == -1)
                    {
                        slave_memory[1] = 0x02;
                    }
                    else
                    {
                        memcpy(&slave_memory[2], &node_pool[cur], sizeof(inventory_node_t));
                        slave_memory[1] = 0x01;
                    }
                }
                else if (cmd == 0x07) 
                {
                    slave_memory[2] = node_count;
                    slave_memory[3] = MAX_NODES;
                    slave_memory[1] = 0x01;
                }
                else if (cmd == 0x08) 
                {
                    inventory_clear();
                    slave_memory[1] = 0x01;
                }
                else if (cmd == 0x09) 
                {
                    uint8_t new_addr = slave_memory[2];
                    if (new_addr >= SLAVE_ADDR_MIN && new_addr <= SLAVE_ADDR_MAX)
                    {
                        eeprom_update_byte(SLAVE_ADDR_EEPROM_BYTE, new_addr);
                        slave_memory[1] = 0x01;
                        slave_memory[0] = 0x00;
                        digital_write(13, BOARD_LOW);
                        i2c_slave_init(new_addr, slave_memory, I2C_SLAVE_MEMORY_SIZE);
                        continue;
                    }
                    else
                    {
                        slave_memory[1] = 0x03; 
                    }
                }
                slave_memory[0] = 0x00;
                digital_write(13, BOARD_LOW);
            }
        }
        delay_ms(5);
    }
    return 0;
}
