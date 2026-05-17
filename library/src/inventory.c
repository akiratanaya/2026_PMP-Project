#include <avr/pgmspace.h>
#include "inventory.h"

#include <string.h>
#include <stddef.h>
#include <avr/io.h>

/* Access command input and tokens provided by serial_cmd.c */
extern char cmd_input_line[];
extern char *cmd_param_str;
extern char *cmd_tokens[];
extern uint8_t cmd_token_count;

#include "arduino_uart.h"
#include "arduino_i2c.h"
#include <avr/eeprom.h>

#include <stdint.h>

/* EEPROM layout */
#define EEPROM_MAGIC 0x494E5631UL /* 'INV1' */

#define SLAVE_I2C_ADDR 0x08U
#define SLAVE_NV_MAGIC 0x534C5631UL /* 'SLV1' */
#define SLAVE_NV_HEADER_SIZE 8U
#define SLAVE_NV_PAYLOAD_OFFSET SLAVE_NV_HEADER_SIZE
#define I2C_CHUNK_SIZE 16U

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

/* compute simple checksum (16-bit sum) */
static uint16_t checksum16(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0U;
    for (size_t i = 0; i < len; ++i)
    {
        sum += p[i];
    }
    return (uint16_t)(sum & 0xFFFFU);
}

/* Configuration: conservative sizes for Uno SRAM */
#define MAX_NODES 5
#define ID_SIZE 10
#define NAME_SIZE 20
#define CAT_SIZE 10
#define LOC_SIZE 10
#define OWNER_SIZE 10
#define PIC_SIZE 10

typedef struct inventory_node
{
    char id[ID_SIZE];
    char name[NAME_SIZE];
    char category[CAT_SIZE];
    uint16_t stock;
    char location[LOC_SIZE];
    uint8_t status; /* 0=available,1=borrowed,2=broken,3=empty */
    char owner[OWNER_SIZE];
    char pic[PIC_SIZE];
    int16_t next; /* index of next node, -1 = null */
} inventory_node_t;

static inventory_node_t node_pool[MAX_NODES];
static int16_t free_list_head = -1;
static int16_t list_head = -1;
static uint8_t node_count = 0U;

static uint8_t i2c_write_block(uint16_t offset, const uint8_t *data, uint16_t length)
{
    uint16_t pos = 0U;

    while (pos < length)
    {
        uint8_t chunk = (uint8_t)((length - pos) > I2C_CHUNK_SIZE ? I2C_CHUNK_SIZE : (length - pos));
        uint8_t status = i2c_master_mem_write(SLAVE_I2C_ADDR, (uint16_t)(offset + pos), &data[pos], chunk);
        if (status != I2C_OK)
        {
            return status;
        }
        pos = (uint16_t)(pos + chunk);
    }

    return I2C_OK;
}

static uint8_t i2c_read_block(uint16_t offset, uint8_t *data, uint16_t length)
{
    uint16_t pos = 0U;

    while (pos < length)
    {
        uint8_t chunk = (uint8_t)((length - pos) > I2C_CHUNK_SIZE ? I2C_CHUNK_SIZE : (length - pos));
        uint8_t status = i2c_master_mem_read(SLAVE_I2C_ADDR, (uint16_t)(offset + pos), &data[pos], chunk);
        if (status != I2C_OK)
        {
            return status;
        }
        pos = (uint16_t)(pos + chunk);
    }

    return I2C_OK;
}

static uint8_t save_to_slave_first(const eeprom_header_t *hdr)
{
    uint8_t header_buf[SLAVE_NV_HEADER_SIZE];
    uint16_t payload_size = (uint16_t)(sizeof(eeprom_header_t) + sizeof(node_pool));
    /* Compute CRC excluding hdr.crc field to avoid including garbage value */
    uint16_t payload_crc = (uint16_t)((checksum16(hdr, offsetof(eeprom_header_t, crc)) + checksum16(node_pool, sizeof(node_pool))) & 0xFFFFU);
    uint8_t status;

    header_buf[0] = (uint8_t)(SLAVE_NV_MAGIC >> 24);
    header_buf[1] = (uint8_t)(SLAVE_NV_MAGIC >> 16);
    header_buf[2] = (uint8_t)(SLAVE_NV_MAGIC >> 8);
    header_buf[3] = (uint8_t)(SLAVE_NV_MAGIC);
    header_buf[4] = (uint8_t)(payload_size >> 8);
    header_buf[5] = (uint8_t)(payload_size & 0x00FFU);
    header_buf[6] = (uint8_t)(payload_crc >> 8);
    header_buf[7] = (uint8_t)(payload_crc & 0x00FFU);

    status = i2c_write_block(0U, header_buf, SLAVE_NV_HEADER_SIZE);
    if (status != I2C_OK)
    {
        return status;
    }

    status = i2c_write_block(SLAVE_NV_PAYLOAD_OFFSET, (const uint8_t *)hdr, sizeof(eeprom_header_t));
    if (status != I2C_OK)
    {
        return status;
    }

    return i2c_write_block((uint16_t)(SLAVE_NV_PAYLOAD_OFFSET + sizeof(eeprom_header_t)), (const uint8_t *)node_pool, sizeof(node_pool));
}

static uint8_t load_from_slave(void)
{
    uint8_t header_buf[SLAVE_NV_HEADER_SIZE];
    eeprom_header_t hdr;
    uint32_t magic;
    uint16_t payload_size;
    uint16_t payload_crc;
    uint16_t computed_crc;

    if (i2c_read_block(0U, header_buf, SLAVE_NV_HEADER_SIZE) != I2C_OK)
    {
        return 0U;
    }

    magic = ((uint32_t)header_buf[0] << 24) | ((uint32_t)header_buf[1] << 16) | ((uint32_t)header_buf[2] << 8) | (uint32_t)header_buf[3];
    payload_size = (uint16_t)(((uint16_t)header_buf[4] << 8) | header_buf[5]);
    payload_crc = (uint16_t)(((uint16_t)header_buf[6] << 8) | header_buf[7]);

    if (magic != SLAVE_NV_MAGIC)
    {
        return 0U;
    }

    if (payload_size != (uint16_t)(sizeof(eeprom_header_t) + sizeof(node_pool)))
    {
        return 0U;
    }

    if (i2c_read_block(SLAVE_NV_PAYLOAD_OFFSET, (uint8_t *)&hdr, sizeof(eeprom_header_t)) != I2C_OK)
    {
        return 0U;
    }

    if (i2c_read_block((uint16_t)(SLAVE_NV_PAYLOAD_OFFSET + sizeof(eeprom_header_t)), (uint8_t *)node_pool, sizeof(node_pool)) != I2C_OK)
    {
        return 0U;
    }

    /* Compute CRC excluding hdr.crc field for consistent comparison */
    computed_crc = (uint16_t)((checksum16(&hdr, offsetof(eeprom_header_t, crc)) + checksum16(node_pool, sizeof(node_pool))) & 0xFFFFU);
    if (computed_crc != payload_crc)
    {
        return 0U;
    }

    if (hdr.magic != EEPROM_MAGIC || hdr.version != 1U || hdr.max_nodes != (uint8_t)MAX_NODES)
    {
        return 0U;
    }

    /* Verify internal hdr.crc field as well */
    if ((uint16_t)((checksum16(node_pool, sizeof(node_pool)) + checksum16(&hdr, offsetof(eeprom_header_t, crc))) & 0xFFFFU) != hdr.crc)
    {
        return 0U;
    }

    node_count = hdr.node_count;
    list_head = hdr.list_head;
    free_list_head = hdr.free_list_head;
    return 1U;
}

void inventory_init(void)
{
    for (int16_t i = 0; i < MAX_NODES; ++i)
    {
        node_pool[i].next = (int16_t)(i + 1);
    }
    node_pool[MAX_NODES - 1].next = -1;
    free_list_head = 0;
    list_head = -1;
    node_count = 0U;
    uart_write_string_P(PSTR("Inventory initialized\r\n"));
}

void inventory_clear(void)
{
    memset(node_pool, 0, sizeof(node_pool));
    for (int16_t i = 0; i < MAX_NODES; ++i)
    {
        node_pool[i].next = (int16_t)(i + 1);
    }
    node_pool[MAX_NODES - 1].next = -1;
    free_list_head = 0;
    list_head = -1;
    node_count = 0U;
    uart_write_string_P(PSTR("CLEAR: Memory wiped\r\n"));
    inventory_save_eeprom();
}

static int16_t alloc_node(void)
{
    if (free_list_head == -1)
    {
        return -1;
    }
    int16_t idx = free_list_head;
    free_list_head = node_pool[idx].next;
    node_pool[idx].next = -1;
    node_count++;
    return idx;
}

static void free_node(int16_t idx)
{
    node_pool[idx].next = free_list_head;
    free_list_head = idx;
    if (node_count > 0U) node_count--;
}

/* Helper: find index by ID, returns -1 if not found */
static int16_t find_index_by_id(const char *id)
{
    int16_t cur = list_head;
    while (cur != -1)
    {
        if (strncmp(node_pool[cur].id, id, ID_SIZE) == 0)
        {
            return cur;
        }
        cur = node_pool[cur].next;
    }
    return -1;
}

void inventory_add(void)
{
    /* Expected param string: id,name,category,stock,location,status,owner,pic */
    if (cmd_param_str == NULL)
    {
        uart_write_string_P(PSTR("ADD: missing parameters\r\n"));
        return;
    }

    if (free_list_head == -1)
    {
        uart_write_string_P(PSTR("ADD: memory full\r\n"));
        return;
    }

    char tmp[128];
    strncpy(tmp, cmd_param_str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    /* DEBUG: show what we got */
    uart_write_string_P(PSTR("DEBUG: param_str="));
    uart_write_string(tmp);
    uart_write_string_P(PSTR("\r\n"));

    char *fields[8] = {0};
    uint8_t f = 0U;
    char *p = tmp;
    while (p != NULL && f < 8U)
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

    if (f < 8U)
    {
        uart_write_string_P(PSTR("ADD: insufficient fields (got "));
        uart_write_uint16(f);
        uart_write_string_P(PSTR(")\r\n"));
        for (uint8_t i = 0U; i < f; i++)
        {
            uart_write_string_P(PSTR("  field["));
            uart_write_uint16(i);
            uart_write_string_P(PSTR("]='"));
            uart_write_string(fields[i]);
            uart_write_string_P(PSTR("'\r\n"));
        }
        return;
    }

    /* check duplicate ID */
    if (find_index_by_id(fields[0]) != -1)
    {
        uart_write_string_P(PSTR("ADD: duplicate ID\r\n"));
        return;
    }

    int16_t idx = alloc_node();
    if (idx == -1)
    {
        uart_write_string_P(PSTR("ADD: allocation failed\r\n"));
        return;
    }

    strncpy(node_pool[idx].id, fields[0], ID_SIZE - 1);
    node_pool[idx].id[ID_SIZE - 1] = '\0';
    strncpy(node_pool[idx].name, fields[1], NAME_SIZE - 1);
    node_pool[idx].name[NAME_SIZE - 1] = '\0';
    strncpy(node_pool[idx].category, fields[2], CAT_SIZE - 1);
    node_pool[idx].category[CAT_SIZE - 1] = '\0';
    uint16_t stock = 0U;
    for (char *q = fields[3]; *q != '\0'; ++q)
    {
        if ((*q >= '0') && (*q <= '9'))
        {
            stock = (uint16_t)(stock * 10U + (uint16_t)(*q - '0'));
        }
    }
    node_pool[idx].stock = stock;
    strncpy(node_pool[idx].location, fields[4], LOC_SIZE - 1);
    node_pool[idx].location[LOC_SIZE - 1] = '\0';
    /* status mapping */
    uint8_t status = 0U;
    if (strncmp(fields[5], "available", 9) == 0) status = 0U;
    else if (strncmp(fields[5], "borrowed", 8) == 0) status = 1U;
    else if (strncmp(fields[5], "broken", 6) == 0) status = 2U;
    else if (strncmp(fields[5], "empty", 5) == 0) status = 3U;
    node_pool[idx].status = status;
    strncpy(node_pool[idx].owner, fields[6], OWNER_SIZE - 1);
    node_pool[idx].owner[OWNER_SIZE - 1] = '\0';
    strncpy(node_pool[idx].pic, fields[7], PIC_SIZE - 1);
    node_pool[idx].pic[PIC_SIZE - 1] = '\0';

    /* insert at head */
    node_pool[idx].next = list_head;
    list_head = idx;

    uart_write_string_P(PSTR("ADD: OK\r\n"));
    inventory_save_eeprom();
}

void inventory_delete(void)
{
    if (cmd_param_str == NULL)
    {
        uart_write_string_P(PSTR("DEL: missing ID\r\n"));
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

    if (cur == -1)
    {
        uart_write_string_P(PSTR("DEL: ID not found\r\n"));
        return;
    }

    if (prev == -1)
    {
        /* removing head */
        list_head = node_pool[cur].next;
    }
    else
    {
        node_pool[prev].next = node_pool[cur].next;
    }

    free_node(cur);
    uart_write_string_P(PSTR("DEL: OK\r\n"));
    inventory_save_eeprom();
}

void inventory_find(void)
{
    if (cmd_param_str == NULL)
    {
        uart_write_string_P(PSTR("FIND: missing ID\r\n"));
        return;
    }

    int16_t idx = find_index_by_id(cmd_param_str);
    if (idx == -1)
    {
        uart_write_string_P(PSTR("FIND: ID not found\r\n"));
        return;
    }

    uart_write_string_P(PSTR("ID:")); uart_write_string(node_pool[idx].id); uart_write_newline();
    uart_write_string_P(PSTR("Name:")); uart_write_string(node_pool[idx].name); uart_write_newline();
    uart_write_string_P(PSTR("Cat:")); uart_write_string(node_pool[idx].category); uart_write_newline();
    uart_write_string_P(PSTR("Stock:")); uart_write_uint16(node_pool[idx].stock); uart_write_newline();
    uart_write_string_P(PSTR("Location:")); uart_write_string(node_pool[idx].location); uart_write_newline();
    uart_write_string_P(PSTR("Status:"));
    if (node_pool[idx].status == 0U) uart_write_string_P(PSTR("available"));
    else if (node_pool[idx].status == 1U) uart_write_string_P(PSTR("borrowed"));
    else if (node_pool[idx].status == 2U) uart_write_string_P(PSTR("broken"));
    else uart_write_string_P(PSTR("empty"));
    uart_write_newline();
    uart_write_string_P(PSTR("Owner:")); uart_write_string(node_pool[idx].owner); uart_write_newline();
    uart_write_string_P(PSTR("PIC:")); uart_write_string(node_pool[idx].pic); uart_write_newline();
}

void inventory_update_stock(void)
{
    /* params: id,delta OR id,newstock (if second has '+' or '-') */
    if (cmd_param_str == NULL)
    {
        uart_write_string_P(PSTR("STOCK: missing params\r\n"));
        return;
    }

    char tmp[64];
    strncpy(tmp, cmd_param_str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *comma = strchr(tmp, ',');
    if (comma == NULL)
    {
        uart_write_string_P(PSTR("STOCK: expected id,delta\r\n"));
        return;
    }
    *comma = '\0';
    char *id = tmp;
    char *num = comma + 1;

    int16_t idx = find_index_by_id(id);
    if (idx == -1)
    {
        uart_write_string_P(PSTR("STOCK: ID not found\r\n"));
        return;
    }

    /* parse number signed */
    int32_t val = 0;
    uint8_t sign = 0U;
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
        uart_write_string_P(PSTR("STOCK: insufficient stock\r\n"));
        return;
    }

    int32_t newstock = (int32_t)node_pool[idx].stock + val;
    if (newstock < 0) newstock = 0;
    node_pool[idx].stock = (uint16_t)newstock;
    uart_write_string_P(PSTR("STOCK: OK\r\n"));
    inventory_save_eeprom();
}

void inventory_update_status(void)
{
    /* params: id,status */
    if (cmd_param_str == NULL)
    {
        uart_write_string_P(PSTR("STATUS: missing params\r\n"));
        return;
    }

    char tmp[64];
    strncpy(tmp, cmd_param_str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *comma = strchr(tmp, ',');
    if (comma == NULL)
    {
        uart_write_string_P(PSTR("STATUS: expected id,status\r\n"));
        return;
    }
    *comma = '\0';
    char *id = tmp;
    char *st = comma + 1;

    int16_t idx = find_index_by_id(id);
    if (idx == -1)
    {
        uart_write_string_P(PSTR("STATUS: ID not found\r\n"));
        return;
    }

    if (strncmp(st, "available", 9) == 0) node_pool[idx].status = 0U;
    else if (strncmp(st, "borrowed", 8) == 0) node_pool[idx].status = 1U;
    else if (strncmp(st, "broken", 6) == 0) node_pool[idx].status = 2U;
    else if (strncmp(st, "empty", 5) == 0) node_pool[idx].status = 3U;
    else
    {
        uart_write_string_P(PSTR("STATUS: unknown status\r\n"));
        return;
    }

    uart_write_string_P(PSTR("STATUS: OK\r\n"));
    inventory_save_eeprom();
}

void inventory_list(void)
{
    int16_t cur = list_head;
    if (cur == -1)
    {
        uart_write_string_P(PSTR("LIST: empty\r\n"));
        return;
    }
    while (cur != -1)
    {
        uart_write_string_P(PSTR("--\r\n"));
        uart_write_string_P(PSTR("ID:")); uart_write_string(node_pool[cur].id); uart_write_newline();
        uart_write_string_P(PSTR("Name:")); uart_write_string(node_pool[cur].name); uart_write_newline();
        uart_write_string_P(PSTR("Stock:")); uart_write_uint16(node_pool[cur].stock); uart_write_newline();
        cur = node_pool[cur].next;
    }
}

void inventory_summary(void)
{
    uart_write_string_P(PSTR("SUMMARY:\r\n"));
    uart_write_string_P(PSTR("Nodes:")); uart_write_uint16(node_count); uart_write_newline();
    uart_write_string_P(PSTR("Capacity:")); uart_write_uint16(MAX_NODES); uart_write_newline();
}

void inventory_save_eeprom(void)
{
    uint8_t slave_status;
    eeprom_header_t hdr;
    hdr.magic = EEPROM_MAGIC;
    hdr.version = 1;
    hdr.max_nodes = (uint8_t)MAX_NODES;
    hdr.node_count = node_count;
    hdr.list_head = list_head;
    hdr.free_list_head = free_list_head;
    /* compute crc over node_pool and header without crc (crc=0)
       we compute over header fields up to free_list_head then nodes */
    hdr.crc = 0U;

    /* temp buffer for nodes only checksum */
    uint16_t crc_nodes = checksum16(node_pool, sizeof(node_pool));
    /* sum header fields */
    uint16_t crc_hdr = checksum16(&hdr, offsetof(eeprom_header_t, crc));
    hdr.crc = (uint16_t)((crc_nodes + crc_hdr) & 0xFFFFU);

    slave_status = save_to_slave_first(&hdr);
    if (slave_status != I2C_OK)
    {
        uart_write_string_P(PSTR("SAVE: SLAVE FAIL code="));
        uart_write_uint16(slave_status);
        uart_write_newline();
        uart_write_string_P(PSTR("SAVE: MASTER SKIPPED\r\n"));
        return;
    }

    uart_write_string_P(PSTR("SAVE: source=SLAVE(NV) OK\r\n"));

    /* write local backup after slave save succeeds */
    eeprom_update_block((const void *)&hdr, (void *)0, sizeof(eeprom_header_t));
    eeprom_update_block((const void *)node_pool, (void *)(sizeof(eeprom_header_t)), sizeof(node_pool));
    uart_write_string_P(PSTR("SAVE: source=MASTER(EEPROM) OK\r\n"));
}

void inventory_load_eeprom(void)
{
    if (load_from_slave() != 0U)
    {
        uart_write_string_P(PSTR("LOAD: source=SLAVE(NV)\r\n"));
        return;
    }

    eeprom_header_t hdr;
    eeprom_read_block((void *)&hdr, (const void *)0, sizeof(eeprom_header_t));
    if (hdr.magic != EEPROM_MAGIC)
    {
        uart_write_string_P(PSTR("LOAD: no saved data\r\n"));
        return;
    }
    if (hdr.version != 1 || hdr.max_nodes != (uint8_t)MAX_NODES)
    {
        uart_write_string_P(PSTR("LOAD: incompatible format\r\n"));
        return;
    }

    /* read nodes */
    eeprom_read_block((void *)node_pool, (const void *)(sizeof(eeprom_header_t)), sizeof(node_pool));

    uint16_t crc_nodes = checksum16(node_pool, sizeof(node_pool));
    uint16_t crc_hdr = checksum16(&hdr, offsetof(eeprom_header_t, crc));
    uint16_t expected = (uint16_t)((crc_nodes + crc_hdr) & 0xFFFFU);
    if (expected != hdr.crc)
    {
        uart_write_string_P(PSTR("LOAD: CRC mismatch\r\n"));
        return;
    }

    node_count = hdr.node_count;
    list_head = hdr.list_head;
    free_list_head = hdr.free_list_head;
    uart_write_string_P(PSTR("LOAD: source=MASTER(EEPROM)\r\n"));
}
