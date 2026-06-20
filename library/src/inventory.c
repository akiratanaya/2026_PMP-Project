#include <avr/pgmspace.h>
#include "inventory.h"
#include "dict.h"

#include <string.h>
#include <stddef.h>
#include <avr/io.h>


extern char cmd_input_line[];
extern char *cmd_param_str;
extern char *cmd_tokens[];
extern uint8_t cmd_token_count;

#include "arduino_uart.h"
#include "arduino_i2c.h"
#include "arduino_time.h"
#include <avr/eeprom.h>

#include <stdint.h>

// EEPROM layout
#define EEPROM_MAGIC 0x494E5631UL 
#define I2C_CHUNK_SIZE 16

/* Multi-Slave registry (Master only) */
#define SLAVE_BASE_ADDR 0x08
#define SLAVE_MAX_ADDR  0x0F   /* supports up to 8 slaves: 0x08..0x0F */
uint8_t slave_addrs[MAX_SLAVES];
uint8_t num_slaves = 0;

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint8_t version;
    uint8_t max_nodes;
    uint8_t node_count;
    int16_t list_head;
    int16_t free_list_head;
    uint16_t crc;
} eeprom_header_t;

/* Global state (non-static so Slave can inspect it) */
inventory_node_t node_pool[MAX_NODES];
int16_t free_list_head = -1;
int16_t list_head = -1;
uint8_t node_count = 0;
uint8_t last_op_status = 0;

/* compute simple checksum (16-bit sum) - Refactored to void */
static void checksum16(const void *data, size_t len, uint16_t *result)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        sum += p[i];
    }
    *result = (uint16_t)(sum & 0xFFFF);
}

/* Write block to specific slave address */
static void i2c_write_block(uint8_t addr, uint16_t offset, const uint8_t *data, uint16_t length, uint8_t *result)
{
    uint16_t pos = 0;
    *result = I2C_OK;
    while (pos < length)
    {
        uint8_t chunk = (uint8_t)((length - pos) > I2C_CHUNK_SIZE ? I2C_CHUNK_SIZE : (length - pos));
        uint8_t status = i2c_master_mem_write(addr, (uint16_t)(offset + pos), &data[pos], chunk);
        if (status != I2C_OK) { *result = status; return; }
        pos = (uint16_t)(pos + chunk);
    }
}

/* Read block from specific slave address */
static void i2c_read_block(uint8_t addr, uint16_t offset, uint8_t *data, uint16_t length, uint8_t *result)
{
    uint16_t pos = 0;
    *result = I2C_OK;
    while (pos < length)
    {
        uint8_t chunk = (uint8_t)((length - pos) > I2C_CHUNK_SIZE ? I2C_CHUNK_SIZE : (length - pos));
        uint8_t status = i2c_master_mem_read(addr, (uint16_t)(offset + pos), &data[pos], chunk);
        if (status != I2C_OK) { *result = status; return; }
        pos = (uint16_t)(pos + chunk);
    }
}

#ifdef ROLE_MASTER
/* Scan I2C bus and populate slave_addrs[] */
void scan_slaves(void)
{
    uint8_t dummy;
    num_slaves = 0;
    for (uint8_t addr = SLAVE_BASE_ADDR; addr <= SLAVE_MAX_ADDR; ++addr)
    {
        uint8_t status = i2c_master_mem_read(addr, 0, &dummy, 1);
        if (status == I2C_OK && num_slaves < MAX_SLAVES)
        {
            slave_addrs[num_slaves++] = addr;
        }
    }
}

/* Send command to a specific slave address */
static void send_slave_cmd(uint8_t slave_addr, uint8_t cmd, const char *param, uint8_t *status_out)
{
    uint8_t i2c_status;

    if (param != NULL)
    {
        uint8_t len = (uint8_t)strlen(param);
        if (len > 60) len = 60;
        i2c_write_block(slave_addr, 2, (const uint8_t *)param, (uint16_t)(len + 1), &i2c_status);
        if (i2c_status != I2C_OK) { *status_out = 0xFF; return; }
    }

    uint8_t busy = 0xFF;
    i2c_status = i2c_master_mem_write(slave_addr, 1, &busy, 1);
    if (i2c_status != I2C_OK) { *status_out = 0xFF; return; }

    i2c_status = i2c_master_mem_write(slave_addr, 0, &cmd, 1);
    if (i2c_status != I2C_OK) { *status_out = 0xFF; return; }

    delay_ms(20);

    uint8_t slave_cmd = 0xFF;
    uint16_t timeout = 2000;
    while (timeout > 0)
    {
        i2c_status = i2c_master_mem_read(slave_addr, 0, &slave_cmd, 1);
        if (i2c_status == I2C_OK && slave_cmd == 0x00) { break; }
        delay_ms(5);
        timeout--;
    }
    if (timeout == 0) { *status_out = 0xFE; return; }

    i2c_status = i2c_master_mem_read(slave_addr, 1, status_out, 1);
    if (i2c_status != I2C_OK) { *status_out = 0xFF; }
}

/* Set new I2C address on a slave (SETADDR command 0x09) */
void set_slave_addr(uint8_t old_addr, uint8_t new_addr, uint8_t *result)
{
    uint8_t payload = new_addr;
    i2c_master_mem_write(old_addr, 2, &payload, 1);
    send_slave_cmd(old_addr, 0x09, NULL, result);
    if (*result == 0x01)
    {
        /* Update registry */
        for (uint8_t i = 0; i < num_slaves; i++)
        {
            if (slave_addrs[i] == old_addr)
            {
                slave_addrs[i] = new_addr;
                break;
            }
        }
    }
}
#else
void scan_slaves(void) {}
void set_slave_addr(uint8_t old_addr, uint8_t new_addr, uint8_t *result) { (void)old_addr; (void)new_addr; *result = 0; }
#endif

void inventory_init(void)
{
    for (int16_t i = 0; i < MAX_NODES; ++i)
    {
        node_pool[i].next = (int8_t)(i + 1);
    }
    node_pool[MAX_NODES - 1].next = -1;
    free_list_head = 0;
    list_head = -1;
    node_count = 0;
}

void inventory_clear(void)
{
    memset(node_pool, 0, sizeof(node_pool));
    for (int16_t i = 0; i < MAX_NODES; ++i)
    {
        node_pool[i].next = (int8_t)(i + 1);
    }
    node_pool[MAX_NODES - 1].next = -1;
    free_list_head = 0;
    list_head = -1;
    node_count = 0;
    inventory_save_eeprom();
    
#ifdef ROLE_MASTER
    uart_write_string_P(PSTR("CLEAR: Master memory wiped\r\n"));
    for (uint8_t s = 0; s < num_slaves; s++)
    {
        uint8_t slave_status;
        send_slave_cmd(slave_addrs[s], 0x08, NULL, &slave_status);
        if (slave_status == 0x01)
        {
            uart_write_string_P(PSTR("CLEAR: Slave wiped\r\n"));
        }
    }
#endif
}

/* Allocate node helper - Refactored to void */
static void alloc_node(int16_t *result)
{
    if (free_list_head == -1)
    {
        *result = -1;
        return;
    }
    int16_t idx = free_list_head;
    free_list_head = node_pool[idx].next;
    node_pool[idx].next = -1;
    node_count++;
    *result = idx;
}

static void free_node(int16_t idx)
{
    node_pool[idx].next = free_list_head;
    free_list_head = idx;
    if (node_count > 0) node_count--;
}

/* Helper: find index by ID, returns -1 if not found */
void find_index_by_id(const char *id, int16_t *result)
{
    int16_t cur = list_head;
    while (cur != -1)
    {
        if (strncmp(node_pool[cur].id, id, ID_SIZE) == 0)
        {
            *result = cur;
            return;
        }
        cur = node_pool[cur].next;
    }
    *result = -1;
}

void inventory_add(void)
{
    /* Expected param string: id,name,category,stock,location,status,owner,pic */
    if (cmd_param_str == NULL)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("ADD: missing parameters\r\n"));
#endif
        last_op_status = 0x04;
        return;
    }

    char tmp[128];
    strncpy(tmp, cmd_param_str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *fields[8] = {0};
    uint8_t f = 0;
    char *p = tmp;
    while (p != NULL && f < 8)
    {
        char *comma = strchr(p, ',');
        if (comma != NULL)
        {
            *comma = '\0';
        }
        fields[f++] = p;
        if (comma == NULL) break;
        p = comma + 1;
    }

    if (f < 8)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("ADD: insufficient fields\r\n"));
#endif
        last_op_status = 0x04;
        return;
    }

    /* check duplicate ID locally */
    int16_t dup_idx;
    find_index_by_id(fields[0], &dup_idx);
    if (dup_idx != -1)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("ADD: duplicate ID\r\n"));
#endif
        last_op_status = 0x02;
        return;
    }

#ifdef ROLE_MASTER
    /* Check duplicate ID across ALL slaves */
    {
        uint8_t slave_status;
        for (uint8_t s = 0; s < num_slaves; s++)
        {
            send_slave_cmd(slave_addrs[s], 0x03, fields[0], &slave_status);
            if (slave_status == 0x01)
            {
                uart_write_string_P(PSTR("ADD: duplicate ID (Slave)\r\n"));
                last_op_status = 0x02;
                return;
            }
        }
    }
#endif

    /* Check free space and add */
    if (free_list_head != -1)
    {
        int16_t idx;
        alloc_node(&idx);
        if (idx == -1)
        {
#ifdef ROLE_MASTER
            uart_write_string_P(PSTR("ADD: allocation failed\r\n"));
#endif
            last_op_status = 0x03;
            return;
        }
        memset(&node_pool[idx], 0, sizeof(inventory_node_t));

        strncpy(node_pool[idx].id, fields[0], ID_SIZE - 1);
        node_pool[idx].id[ID_SIZE - 1] = '\0';
        strncpy(node_pool[idx].name, fields[1], NAME_SIZE - 1);
        node_pool[idx].name[NAME_SIZE - 1] = '\0';
        strncpy(node_pool[idx].category, fields[2], CAT_SIZE - 1);
        node_pool[idx].category[CAT_SIZE - 1] = '\0';
        uint16_t parsed_stock = 0;
        for (char *q = fields[3]; *q != '\0'; ++q)
        {
            if ((*q >= '0') && (*q <= '9'))
            {
                parsed_stock = (uint16_t)(parsed_stock * 10 + (uint16_t)(*q - '0'));
            }
        }
        node_pool[idx].stock = (uint8_t)(parsed_stock > 255 ? 255 : parsed_stock);
        strncpy(node_pool[idx].location, fields[4], LOC_SIZE - 1);
        node_pool[idx].location[LOC_SIZE - 1] = '\0';
        
        uint8_t status = 0;
        if (strncmp(fields[5], "available", 9) == 0) status = 0;
        else if (strncmp(fields[5], "borrowed", 8) == 0) status = 1;
        else if (strncmp(fields[5], "broken", 6) == 0) status = 2;
        else if (strncmp(fields[5], "empty", 5) == 0) status = 3;
        node_pool[idx].status = status;
        strncpy(node_pool[idx].owner, fields[6], OWNER_SIZE - 1);
        node_pool[idx].owner[OWNER_SIZE - 1] = '\0';
        strncpy(node_pool[idx].pic, fields[7], PIC_SIZE - 1);
        node_pool[idx].pic[PIC_SIZE - 1] = '\0';

        node_pool[idx].next = list_head;
        list_head = idx;

        last_op_status = 0x01; // SUCCESS
#ifdef ROLE_MASTER
        if (node_count >= MAX_NODES - 2)
        {
            uart_write_string_P(PSTR("ADD: OK (Warning: memory almost full)\r\n"));
        }
        else
        {
            uart_write_string_P(PSTR("ADD: OK\r\n"));
        }
#endif
        inventory_save_eeprom();
    }
    else
    {
#ifdef ROLE_MASTER
        /* Master is full — cascade to first Slave with space */
        uart_write_string_P(PSTR("ADD: Master full. Forwarding to Slave...\r\n"));
        uint8_t slave_status = 0;
        uint8_t forwarded = 0;
        for (uint8_t s = 0; s < num_slaves; s++)
        {
            send_slave_cmd(slave_addrs[s], 0x01, cmd_param_str, &slave_status);
            if (slave_status == 0x01)
            {
                uart_write_string_P(PSTR("ADD: OK (Slave)\r\n"));
                uart_write_string_P(PSTR("SAVE: source=SLAVE(I2C) OK\r\n"));
                last_op_status = 0x01;
                forwarded = 1;
                break;
            }
            /* 0x03 = full, try next slave */
        }
        if (!forwarded)
        {
            uart_write_string_P(PSTR("ADD: all nodes full\r\n"));
            last_op_status = 0x03;
        }
#else
        last_op_status = 0x03; // Full on Slave
#endif
    }
}

void inventory_delete(void)
{
    if (cmd_param_str == NULL)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("DEL: missing ID\r\n"));
#endif
        last_op_status = 0x02;
        return;
    }

    int16_t prev = -1;
    int16_t cur = list_head;
    while (cur != -1)
    {
        if (strncmp(node_pool[cur].id, cmd_param_str, ID_SIZE) == 0)
        {
            break;
        }
        prev = cur;
        cur = node_pool[cur].next;
    }

    if (cur != -1)
    {
        if (prev == -1)
        {
            list_head = node_pool[cur].next;
        }
        else
        {
            node_pool[prev].next = node_pool[cur].next;
        }

        free_node(cur);
        last_op_status = 0x01;
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("DEL: OK\r\n"));
#endif
        inventory_save_eeprom();
    }
    else
    {
#ifdef ROLE_MASTER
        /* Try deleting on each slave in order */
        uint8_t slave_status;
        uint8_t deleted = 0;
        for (uint8_t s = 0; s < num_slaves; s++)
        {
            send_slave_cmd(slave_addrs[s], 0x02, cmd_param_str, &slave_status);
            if (slave_status == 0x01)
            {
                uart_write_string_P(PSTR("DEL: OK (Slave)\r\n"));
                last_op_status = 0x01;
                deleted = 1;
                break;
            }
        }
        if (!deleted)
        {
            uart_write_string_P(PSTR("DEL: ID not found\r\n"));
            last_op_status = 0x02;
        }
#else
        last_op_status = 0x02;
#endif
    }
}

#ifdef ROLE_MASTER
static void print_find_row(const char *label_P, const char *val, uint8_t val_width)
{
    uart_write_string_P(label_P);
    uint8_t l = 0;
    if (val != NULL)
    {
        while (val[l] != '\0' && l < val_width) { uart_write_char(val[l]); l++; }
    }
    while (l < val_width) { uart_write_char(' '); l++; }
    uart_write_string_P(PSTR(" |\r\n"));
}

static void print_find_result(const inventory_node_t *node, const char *src)
{
    char buf[DICT_FULL_SIZE];

    uart_write_string_P(PSTR("+----------+----------------------+\r\n"));
    uart_write_string_P(PSTR("| FIND     | "));
    uint8_t l = 0;
    while (src[l] != '\0' && l < 20) { uart_write_char(src[l]); l++; }
    while (l < 20) { uart_write_char(' '); l++; }
    uart_write_string_P(PSTR(" |\r\n+----------+----------------------+\r\n"));

    print_find_row(PSTR("| ID       | "), node->id, 20);

    dict_lookup_name(node->name, buf);
    print_find_row(PSTR("| Name     | "), buf, 20);

    dict_lookup_cat(node->category, buf);
    print_find_row(PSTR("| Category | "), buf, 20);

    /* Stock — right-aligned */
    uart_write_string_P(PSTR("| Stock    | "));
    {
        char sbuf[6]; uint8_t idx = 0; uint16_t v = node->stock;
        if (v == 0) { sbuf[idx++] = '0'; }
        else { while (v > 0 && idx < 6) { sbuf[idx++] = (char)('0' + (v % 10)); v /= 10; } }
        uint8_t pad = (idx < 20) ? (20 - idx) : 0;
        while (pad--) { uart_write_char(' '); }
        while (idx > 0) { idx--; uart_write_char(sbuf[idx]); }
    }
    uart_write_string_P(PSTR(" |\r\n"));

    dict_lookup_loc(node->location, buf);
    print_find_row(PSTR("| Location | "), buf, 20);

    /* Status from PROGMEM */
    uart_write_string_P(PSTR("| Status   | "));
    {
        const char *st;
        if      (node->status == 0) st = PSTR("available");
        else if (node->status == 1) st = PSTR("borrowed");
        else if (node->status == 2) st = PSTR("broken");
        else                          st = PSTR("empty");
        uint8_t sl = 0; char c;
        while ((c = (char)pgm_read_byte(st + sl)) != '\0') { uart_write_char(c); sl++; }
        while (sl < 20) { uart_write_char(' '); sl++; }
    }
    uart_write_string_P(PSTR(" |\r\n"));

    dict_lookup_owner(node->owner, buf);
    print_find_row(PSTR("| Owner    | "), buf, 20);

    dict_lookup_pic(node->pic, buf);
    print_find_row(PSTR("| PIC      | "), buf, 20);
    uart_write_string_P(PSTR("+----------+----------------------+\r\n"));
}
#endif

void inventory_find(void)
{
    if (cmd_param_str == NULL)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("FIND: missing ID\r\n"));
#endif
        last_op_status = 0x02;
        return;
    }

    int16_t idx;
    find_index_by_id(cmd_param_str, &idx);
    if (idx != -1)
    {
#ifdef ROLE_MASTER
        char src[7] = "Master";
        print_find_result(&node_pool[idx], src);
#endif
        last_op_status = 0x01;
    }
    else
    {
#ifdef ROLE_MASTER
        uint8_t slave_status;
        uint8_t found_slave = 0;
        for (uint8_t s = 0; s < num_slaves; s++)
        {
            send_slave_cmd(slave_addrs[s], 0x03, cmd_param_str, &slave_status);
            if (slave_status == 0x01)
            {
                inventory_node_t node;
                uint8_t i2c_st;
                i2c_read_block(slave_addrs[s], 2, (uint8_t *)&node, sizeof(inventory_node_t), &i2c_st);
                if (i2c_st == I2C_OK)
                {
                    char src[8];
                    src[0]='S'; src[1]='l'; src[2]='a'; src[3]='v';
                    src[4]='e'; src[5]=(char)('0'+s); src[6]='\0';
                    print_find_result(&node, src);
                    last_op_status = 0x01;
                    found_slave = 1;
                }
                break;
            }
        }
        if (!found_slave)
        {
            uart_write_string_P(PSTR("FIND: ID not found\r\n"));
            last_op_status = 0x02;
        }
#else
        last_op_status = 0x02;
#endif
    }
}

void inventory_update_stock(void)
{
    if (cmd_param_str == NULL)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("STOCK: missing params\r\n"));
#endif
        last_op_status = 0x02;
        return;
    }

    char tmp[64];
    strncpy(tmp, cmd_param_str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *comma = strchr(tmp, ',');
    if (comma == NULL)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("STOCK: expected id,delta\r\n"));
#endif
        last_op_status = 0x02;
        return;
    }
    *comma = '\0';
    char *id = tmp;
    char *num = comma + 1;

    int16_t idx;
    find_index_by_id(id, &idx);
    if (idx != -1)
    {
        /* parse number signed */
        int32_t val = 0;
        uint8_t sign = 0;
        char *q = num;
        if (*q == '+') { sign = 0; q++; }
        else if (*q == '-') { sign = 1; q++; }
        while (*q != '\0')
        {
            if ((*q >= '0') && (*q <= '9'))
            {
                val = val * 10 + (int32_t)(*q - '0');
            }
            q++;
        }
        if (sign) val = -val;

        if (val < 0 && (uint16_t)(-val) > node_pool[idx].stock)
        {
#ifdef ROLE_MASTER
            uart_write_string_P(PSTR("STOCK: insufficient stock\r\n"));
#endif
            last_op_status = 0x03;
            return;
        }

        int32_t newstock = (int32_t)node_pool[idx].stock + val;
        if (newstock < 0) newstock = 0;
        node_pool[idx].stock = (uint8_t)(newstock > 255 ? 255 : newstock);
        last_op_status = 0x01;
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("STOCK: OK\r\n"));
#endif
        inventory_save_eeprom();
    }
    else
    {
#ifdef ROLE_MASTER
        uint8_t slave_status;
        uint8_t updated = 0;
        for (uint8_t s = 0; s < num_slaves; s++)
        {
            send_slave_cmd(slave_addrs[s], 0x04, cmd_param_str, &slave_status);
            if (slave_status == 0x01)
            {
                uart_write_string_P(PSTR("STOCK: OK (Slave)\r\n"));
                last_op_status = 0x01;
                updated = 1;
                break;
            }
            else if (slave_status == 0x03)
            {
                uart_write_string_P(PSTR("STOCK: insufficient stock\r\n"));
                last_op_status = 0x03;
                updated = 1;
                break;
            }
        }
        if (!updated)
        {
            uart_write_string_P(PSTR("STOCK: ID not found\r\n"));
            last_op_status = 0x02;
        }
#else
        last_op_status = 0x02;
#endif
    }
}

void inventory_update_status(void)
{
    if (cmd_param_str == NULL)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("STATUS: missing params\r\n"));
#endif
        last_op_status = 0x02;
        return;
    }

    char tmp[64];
    strncpy(tmp, cmd_param_str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *comma = strchr(tmp, ',');
    if (comma == NULL)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("STATUS: expected id,status\r\n"));
#endif
        last_op_status = 0x02;
        return;
    }
    *comma = '\0';
    char *id = tmp;
    char *st = comma + 1;

    int16_t idx;
    find_index_by_id(id, &idx);
    if (idx != -1)
    {
        if (strncmp(st, "available", 9) == 0) node_pool[idx].status = 0;
        else if (strncmp(st, "borrowed", 8) == 0) node_pool[idx].status = 1;
        else if (strncmp(st, "broken", 6) == 0) node_pool[idx].status = 2;
        else if (strncmp(st, "empty", 5) == 0) node_pool[idx].status = 3;
        else
        {
#ifdef ROLE_MASTER
            uart_write_string_P(PSTR("STATUS: unknown status\r\n"));
#endif
            last_op_status = 0x03;
            return;
        }

        last_op_status = 0x01;
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("STATUS: OK\r\n"));
#endif
        inventory_save_eeprom();
    }
    else
    {
#ifdef ROLE_MASTER
        uint8_t slave_status;
        uint8_t updated = 0;
        for (uint8_t s = 0; s < num_slaves; s++)
        {
            send_slave_cmd(slave_addrs[s], 0x05, cmd_param_str, &slave_status);
            if (slave_status == 0x01)
            {
                uart_write_string_P(PSTR("STATUS: OK (Slave)\r\n"));
                last_op_status = 0x01;
                updated = 1;
                break;
            }
            else if (slave_status == 0x03)
            {
                uart_write_string_P(PSTR("STATUS: unknown status\r\n"));
                last_op_status = 0x03;
                updated = 1;
                break;
            }
        }
        if (!updated)
        {
            uart_write_string_P(PSTR("STATUS: ID not found\r\n"));
            last_op_status = 0x02;
        }
#else
        last_op_status = 0x02;
#endif
    }
}

#ifdef ROLE_MASTER
static void uart_write_padded(const char *s, uint8_t width)
{
    uint8_t len = 0;
    if (s != NULL)
    {
        len = (uint8_t)strlen(s);
        uart_write_string(s);
    }
    while (len < width)
    {
        uart_write_char(' ');
        len++;
    }
}

static void uart_write_padded_uint16(uint16_t val, uint8_t width)
{
    char buffer[6];
    uint8_t index = 0;
    if (val == 0)
    {
        buffer[index++] = '0';
    }
    else
    {
        while (val > 0 && index < sizeof(buffer))
        {
            buffer[index++] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    while (index < width)
    {
        uart_write_char(' ');
        width--;
    }
    while (index > 0)
    {
        index--;
        uart_write_char(buffer[index]);
    }
}

static void print_node_row(const inventory_node_t *node, const char *source_sram, const char *source_progmem)
{
    /* Temporary buffer in SRAM — only alive during this print call */
    char buf[DICT_FULL_SIZE];
    char c;

    /* ID (raw code, no expansion needed) */
    uart_write_string_P(PSTR("| "));
    uart_write_padded(node->id, 3);

    /* Name — expand code to full name via Flash dictionary */
    uart_write_string_P(PSTR(" | "));
    dict_lookup_name(node->name, buf);
    uart_write_padded(buf, 15);

    /* Category — expand via Flash dictionary */
    uart_write_string_P(PSTR(" | "));
    dict_lookup_cat(node->category, buf);
    uart_write_padded(buf, 13);

    /* Stock (numeric, no expansion) */
    uart_write_string_P(PSTR(" | "));
    uart_write_padded_uint16(node->stock, 5);

    /* Location — expand via Flash dictionary */
    uart_write_string_P(PSTR(" | "));
    dict_lookup_loc(node->location, buf);
    uart_write_padded(buf, 15);

    /* Status (PROGMEM string, no dictionary needed) */
    uart_write_string_P(PSTR(" | "));
    const char *status_str;
    if      (node->status == 0) status_str = PSTR("available");
    else if (node->status == 1) status_str = PSTR("borrowed");
    else if (node->status == 2) status_str = PSTR("broken");
    else                         status_str = PSTR("empty");
    uint8_t len = 0;
    while ((c = (char)pgm_read_byte(status_str + len)) != '\0')
    {
        uart_write_char(c);
        len++;
    }
    while (len < 9) { uart_write_char(' '); len++; }

    /* Owner — expand via Flash dictionary */
    uart_write_string_P(PSTR(" | "));
    dict_lookup_owner(node->owner, buf);
    uart_write_padded(buf, 16);

    /* PIC — expand via Flash dictionary */
    uart_write_string_P(PSTR(" | "));
    dict_lookup_pic(node->pic, buf);
    uart_write_padded(buf, 12);

    /* Source column */
    uart_write_string_P(PSTR(" | "));
    if (source_progmem != NULL)
    {
        uint8_t slen = 0;
        while ((c = (char)pgm_read_byte(source_progmem + slen)) != '\0')
        {
            uart_write_char(c);
            slen++;
        }
        while (slen < 6) { uart_write_char(' '); slen++; }
    }
    else
    {
        uart_write_padded(source_sram, 6);
    }
    uart_write_string_P(PSTR(" |\r\n"));
}

static void print_table_header(void)
{
    uart_write_string_P(PSTR("+-----+-----------------+---------------+-------+-----------------+-----------+------------------+--------------+--------+\r\n"));
    uart_write_string_P(PSTR("| ID  | Name            | Category      | Stock | Location        | Status    | Owner            | PIC          | Source |\r\n"));
    uart_write_string_P(PSTR("+-----+-----------------+---------------+-------+-----------------+-----------+------------------+--------------+--------+\r\n"));
}

static void print_table_footer(void)
{
    uart_write_string_P(PSTR("+-----+-----------------+---------------+-------+-----------------+-----------+------------------+--------------+--------+\r\n"));
}
#endif

void inventory_list(void)
{
    int16_t cur = list_head;
#ifdef ROLE_MASTER
    uint16_t total_printed = 0;
    
    print_table_header();
    
    while (cur != -1)
    {
        print_node_row(&node_pool[cur], NULL, PSTR("Master"));
        cur = node_pool[cur].next;
        total_printed++;
    }

    /* List all slaves in order */
    for (uint8_t s = 0; s < num_slaves; s++)
    {
        uint8_t list_idx = 0;
        while (list_idx < MAX_NODES)
        {
            char param[4];
            param[0] = (char)('0' + (list_idx / 10));
            param[1] = (char)('0' + (list_idx % 10));
            param[2] = '\0';

            uint8_t slave_status;
            send_slave_cmd(slave_addrs[s], 0x06, param, &slave_status);
            if (slave_status != 0x01) break;

            inventory_node_t node;
            uint8_t i2c_st;
            i2c_read_block(slave_addrs[s], 2, (uint8_t *)&node, sizeof(inventory_node_t), &i2c_st);
            if (i2c_st == I2C_OK)
            {
                char src_str[8];
                src_str[0] = 'S';
                src_str[1] = 'l';
                src_str[2] = 'a';
                src_str[3] = 'v';
                src_str[4] = 'e';
                src_str[5] = (char)('0' + s);
                src_str[6] = '\0';
                
                print_node_row(&node, src_str, NULL);
                total_printed++;
            }
            list_idx++;
        }
    }
    
    print_table_footer();
    
    if (total_printed == 0)
    {
        uart_write_string_P(PSTR("No items found in database.\r\n"));
    }
    else
    {
        uart_write_string_P(PSTR("Total items: "));
        uart_write_uint16(total_printed);
        uart_write_newline();
    }
#else
    while (cur != -1) { cur = node_pool[cur].next; }
#endif
}

void inventory_summary(void)
{
#ifdef ROLE_MASTER
    uint16_t total_nodes    = node_count;
    uint16_t total_capacity = (uint16_t)MAX_NODES;

    uart_write_string_P(PSTR("+--------+------+------+\r\n"));
    uart_write_string_P(PSTR("| Source | Used |  Cap |\r\n"));
    uart_write_string_P(PSTR("+--------+------+------+\r\n"));

    /* Master row */
    uart_write_string_P(PSTR("| Master | "));
    uart_write_padded_uint16(node_count, 4);
    uart_write_string_P(PSTR(" | "));
    uart_write_padded_uint16((uint16_t)MAX_NODES, 4);
    uart_write_string_P(PSTR(" |\r\n"));

    /* Slave rows */
    for (uint8_t s = 0; s < num_slaves; s++)
    {
        uint8_t slave_status;
        send_slave_cmd(slave_addrs[s], 0x07, NULL, &slave_status);
        uart_write_string_P(PSTR("| Slave"));
        uart_write_char((char)('0' + s));
        uart_write_string_P(PSTR(" | "));
        if (slave_status == 0x01)
        {
            uint8_t sn = 0, sc = 0;
            i2c_master_mem_read(slave_addrs[s], 2, &sn, 1);
            i2c_master_mem_read(slave_addrs[s], 3, &sc, 1);
            total_nodes    = (uint16_t)(total_nodes    + sn);
            total_capacity = (uint16_t)(total_capacity + sc);
            uart_write_padded_uint16(sn, 4);
            uart_write_string_P(PSTR(" | "));
            uart_write_padded_uint16(sc, 4);
            uart_write_string_P(PSTR(" |\r\n"));
        }
        else
        {
            uart_write_string_P(PSTR("OFFL | OFFL |\r\n"));
        }
    }

    uart_write_string_P(PSTR("+--------+------+------+\r\n"));
    uart_write_string_P(PSTR("| TOTAL  | "));
    uart_write_padded_uint16(total_nodes, 4);
    uart_write_string_P(PSTR(" | "));
    uart_write_padded_uint16(total_capacity, 4);
    uart_write_string_P(PSTR(" |\r\n"));
    uart_write_string_P(PSTR("+--------+------+------+\r\n"));

    if (num_slaves == 0)
        uart_write_string_P(PSTR("(No slave nodes)\r\n"));
#endif
}

#ifdef ROLE_MASTER
static void print_raw_csv_row(const inventory_node_t *node)
{
    uart_write_string(node->id); uart_write_char(',');
    uart_write_string(node->name); uart_write_char(',');
    uart_write_string(node->category); uart_write_char(',');
    
    char sbuf[6]; uint8_t idx = 0; uint16_t v = node->stock;
    if (v == 0) { sbuf[idx++] = '0'; }
    else { while (v > 0 && idx < 6) { sbuf[idx++] = (char)('0' + (v % 10)); v /= 10; } }
    while (idx > 0) { idx--; uart_write_char(sbuf[idx]); }
    
    uart_write_char(',');
    uart_write_string(node->location); uart_write_char(',');
    
    const char *st;
    if      (node->status == 0) st = PSTR("available");
    else if (node->status == 1) st = PSTR("borrowed");
    else if (node->status == 2) st = PSTR("broken");
    else                        st = PSTR("empty");
    uint8_t sl = 0; char c;
    while ((c = (char)pgm_read_byte(st + sl)) != '\0') { uart_write_char(c); sl++; }
    
    uart_write_char(',');
    uart_write_string(node->owner); uart_write_char(',');
    uart_write_string(node->pic); uart_write_newline();
}
#endif

void inventory_export(void)
{
#ifdef ROLE_MASTER
    int16_t cur = list_head;
    while (cur != -1)
    {
        print_raw_csv_row(&node_pool[cur]);
        cur = node_pool[cur].next;
    }
    
    for (uint8_t s = 0; s < num_slaves; s++)
    {
        uint8_t list_idx = 0;
        while (list_idx < MAX_NODES)
        {
            char param[4];
            param[0] = (char)('0' + (list_idx / 10));
            param[1] = (char)('0' + (list_idx % 10));
            param[2] = '\0';
            
            uint8_t slave_status;
            send_slave_cmd(slave_addrs[s], 0x06, param, &slave_status);
            if (slave_status != 0x01) break;
            
            inventory_node_t node;
            uint8_t i2c_st;
            i2c_read_block(slave_addrs[s], 2, (uint8_t *)&node, sizeof(inventory_node_t), &i2c_st);
            if (i2c_st == I2C_OK) {
                print_raw_csv_row(&node);
            }
            list_idx++;
        }
    }
    uart_write_string_P(PSTR("EXPORT_DONE\r\n"));
#endif
}

void inventory_save_eeprom(void)
{
    eeprom_header_t hdr;
    hdr.magic = EEPROM_MAGIC;
    hdr.version = 1;
    hdr.max_nodes = (uint8_t)MAX_NODES;
    hdr.node_count = node_count;
    hdr.list_head = list_head;
    hdr.free_list_head = free_list_head;
    hdr.crc = 0;

    uint16_t crc_nodes;
    checksum16(node_pool, sizeof(node_pool), &crc_nodes);
    uint16_t crc_hdr;
    checksum16(&hdr, offsetof(eeprom_header_t, crc), &crc_hdr);
    hdr.crc = (uint16_t)((crc_nodes + crc_hdr) & 0xFFFF);

    /* Write local EEPROM */
    eeprom_update_block((const void *)&hdr, (void *)0, sizeof(eeprom_header_t));
    eeprom_update_block((const void *)node_pool, (void *)(sizeof(eeprom_header_t)), sizeof(node_pool));
#ifdef ROLE_MASTER
    uart_write_string_P(PSTR("SAVE: source=MASTER(EEPROM) OK\r\n"));
#endif
}

void inventory_load_eeprom(void)
{
    eeprom_header_t hdr;
    eeprom_read_block((void *)&hdr, (const void *)0, sizeof(eeprom_header_t));
    if (hdr.magic != EEPROM_MAGIC)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("LOAD: no saved data\r\n"));
#endif
        return;
    }
    if (hdr.version != 1 || hdr.max_nodes != (uint8_t)MAX_NODES)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("LOAD: incompatible format\r\n"));
#endif
        return;
    }

    /* read nodes */
    eeprom_read_block((void *)node_pool, (const void *)(sizeof(eeprom_header_t)), sizeof(node_pool));

    uint16_t crc_nodes;
    checksum16(node_pool, sizeof(node_pool), &crc_nodes);
    uint16_t crc_hdr;
    checksum16(&hdr, offsetof(eeprom_header_t, crc), &crc_hdr);
    uint16_t expected = (uint16_t)((crc_nodes + crc_hdr) & 0xFFFF);
    if (expected != hdr.crc)
    {
#ifdef ROLE_MASTER
        uart_write_string_P(PSTR("LOAD: CRC mismatch\r\n"));
#endif
        return;
    }

    node_count = hdr.node_count;
    list_head = hdr.list_head;
    free_list_head = hdr.free_list_head;
#ifdef ROLE_MASTER
    uart_write_string_P(PSTR("LOAD: source=MASTER(EEPROM)\r\n"));
#endif
}
