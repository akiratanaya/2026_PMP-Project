#ifndef INVENTORY_H
#define INVENTORY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MAX_NODES  40
#define MAX_SLAVES 8
#define ID_SIZE    4
#define NAME_SIZE  4
#define CAT_SIZE   4
#define LOC_SIZE   3
#define OWNER_SIZE 3
#define PIC_SIZE   3
typedef struct inventory_node
{
    char id[ID_SIZE];
    char name[NAME_SIZE];
    char category[CAT_SIZE];
    uint8_t stock;
    char location[LOC_SIZE];
    uint8_t status; 
    char owner[OWNER_SIZE];
    char pic[PIC_SIZE];
    int8_t next;
} inventory_node_t;
extern inventory_node_t node_pool[MAX_NODES];
extern int16_t free_list_head;
extern int16_t list_head;
extern uint8_t node_count;
extern uint8_t last_op_status;
extern uint8_t slave_addrs[MAX_SLAVES];
extern uint8_t num_slaves;
void inventory_init(void);
void inventory_add(void);
void inventory_delete(void);
void inventory_find(void);
void inventory_update_stock(void);
void inventory_update_status(void);
void inventory_list(void);
void inventory_summary(void);
void inventory_export(void);
void inventory_save_eeprom(void);
void inventory_load_eeprom(void);
void inventory_clear(void);
void find_index_by_id(const char *id, int16_t *result);
void scan_slaves(void);
void set_slave_addr(uint8_t old_addr, uint8_t new_addr, uint8_t *result);
#ifdef __cplusplus
}
#endif
#endif
