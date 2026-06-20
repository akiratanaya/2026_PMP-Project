#ifndef SERIAL_CMD_H
#define SERIAL_CMD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void system_init(void);
void system_loop(void);
extern char cmd_input_line[];
extern char *cmd_param_str;
extern char *cmd_tokens[];
extern uint8_t cmd_token_count;
#ifdef __cplusplus
}
#endif
#endif
