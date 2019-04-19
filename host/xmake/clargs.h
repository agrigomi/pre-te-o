#ifndef __CL_ARGS__
#define __CL_ARGS__

#include "mgtype.h"

#define ARG_TFLAG	1
#define ARG_TINPUT	(1<<1)

typedef struct {
	_u8		type;   // option type
	_cstr_t		opt;	// option
	_cstr_t		sopt;	// short option
	_cstr_t		des;	// description
	_str_t		arg;	// argument
	_u32		sz_arg; // argument size
	bool		active; // option present flag
	_u32		idx;	// argv index
} _cl_arg_t;

bool clargs_parse(_s32 argc, _str_t argv[], _cl_arg_t opt[]);
bool clargs_option(_cstr_t opt, _str_t *p_arg=0, _u32 *sz=0);
void clargs_print(_cl_arg_t opt[]);
_str_t clargs_err_text(void);
_str_t clargs_parameter(_u32 index);
void clargs_reset(void);

#endif

