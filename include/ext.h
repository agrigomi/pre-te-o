#ifndef __EXT_H__
#define __EXT_H__

#include "iRepository.h"

typedef bool _module_exit_t(void);

// extensions startup parameters
typedef struct {
	iRepository	*p_repository; // pointer to repository object
	_ulong		so_index; // storage index (from repository)
	_module_exit_t	*p_cb_exit;
	_str_t		args; // zero terminated string 
}_ext_context_t;

#ifdef _EXT_
#ifdef __cplusplus
__cdecl void _main(_str_t args);
#endif
#endif //_EXT_

#endif

