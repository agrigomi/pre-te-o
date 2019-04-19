#ifndef __KARG_PARSER__H__
#define __KARG_PARSER__H__

#include "mgtype.h"

#define MAX_EXT_PATH	256

#define HEAP_SIZE	(_str_t)"hs"
#define LOG_SIZE	(_str_t)"ls"
#define EXT_CONFIG	(_str_t)"ext"
#define RAMD_SIZE	(_str_t)"rds"

_u32 int_parameter(_str_t args, _str_t name);
bool str_parameter(_str_t args, _str_t name, _str_t value, _u32 szv);

#endif
