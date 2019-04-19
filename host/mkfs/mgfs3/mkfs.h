#ifndef __MKFS_H__
#define __MKFS_H__


#include <stdio.h>
#include "mgfs3.h"
#include "clargs.h"

#ifdef _GCC_
#define TRACE(fmt,...) printf(fmt,__VA_ARGS__);
#define VTRACE(fmt,...) {\
			if(clargs_option("-V")) \
				printf(fmt, __VA_ARGS__); \
			}
#ifdef DEBUG
#define DBG(fmt,...) printf(fmt,__VA_ARGS__);
#else
#define DBG(fmt,...)
#endif
#endif // _GCC_

// defaults
#define DEFAULT_UNIT_SIZE	2

extern _cl_arg_t _g_options_[];

typedef void _action_proc_t(void);

typedef struct {
	_cstr_t		action;
	_action_proc_t *p_action;
	_cstr_t		description;
}_action_t;

extern _action_t _g_action_[];

_mgfs_context_t *get_context(void);
_fs_context_t *get_fs_context(void);
_u8 *get_device(void);
_u64 get_device_size(void);
_u8 *map_file(_str_t name,_u64 *sz);

void parse_file_map(void);
void install_vbr(void);
void install_fsbr(void);

bool path_parser(_str_t path, _h_file_ hcdir, // in
		 _h_file_ hdir, _str_t *fname // out
		);

void import_file(_str_t src, _str_t dst);
void export_file(_str_t src, _str_t dst);
void create_hard_link(_str_t src, _str_t dst);
void create_soft_link(_str_t src, _str_t dst);
void create_directory(_str_t path, _h_file_ hdstdir, _str_t name);

// fs actions
void _create_fs_(void);
void _read_file_(void);
void _write_file_(void);
void _delete_file_(void);
void _move_file_(void);
void _mkdir_(void);
void _rmdir_(void);
void _soft_link_(void);
void _hard_link_(void);
void _list_(void);
void _cat_(void);
void _status_(void);

#endif

