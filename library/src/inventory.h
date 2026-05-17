#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdint.h>


void inventory_init(void);
void inventory_add(void);
void inventory_delete(void);
void inventory_find(void);
void inventory_update_stock(void);
void inventory_update_status(void);
void inventory_list(void);
void inventory_summary(void);
void inventory_save_eeprom(void);
void inventory_load_eeprom(void);
void inventory_clear(void);

#ifdef __cplusplus
}
#endif

#endif
