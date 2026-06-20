#ifndef DICT_H
#define DICT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DICT_CODE_SIZE 4   
#define DICT_FULL_SIZE 20  


void dict_lookup_name(const char *code, char *out);
void dict_lookup_cat(const char *code, char *out);
void dict_lookup_loc(const char *code, char *out);
void dict_lookup_owner(const char *code, char *out);
void dict_lookup_pic(const char *code, char *out);

#ifdef __cplusplus
}
#endif

#endif /* DICT_H */
