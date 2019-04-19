#ifndef __OBJECT_REPOSITORY__
#define __OBJECT_REPOSITORY__

#include "str.h"
#include "iRepository.h"

#define MAX_STATIC_OBJECTS	256
#define MAX_SO_STORAGES		256

extern iRepository	*__g_p_repository__;
extern	_u32 __g_static_objects_counter__;
extern	_u32 __g_so_storage_limit__;
extern	_ulong __g_so_storage_index__;
extern _object_t __g_static_objects__[];

// NOTE: The following macro can be used only once per application context.
//	Normally it's implemented by abi/loader.cpp
#define IMPLEMENT_SO_STORAGE(limit) \
	_object_t __g_static_objects__[limit]; \
	_u32 __g_static_objects_counter__ = 0; \
	_u32 __g_so_storage_limit__ = limit; \
	_ulong __g_so_storage_index__ = INVALID_SO_STORAGE_INDEX; \
	void __register_static_object__(_ry_flags_t flags, iBase *p_obj, _ry_id_t id=RY_NO_OBJECT_ID) { \
		if(__g_static_objects_counter__ < limit) { \
			for(_u32 i = 0; i < __g_static_objects_counter__; i++) { \
				if((str_cmp(__g_static_objects__[i].p_obj->object_name(), \
							p_obj->object_name()) == 0 && \
						(__g_static_objects__[i].flags & flags)) || \
						(__g_static_objects__[i].id == id && id != RY_NO_OBJECT_ID)) \
					return; \
			} \
			__g_static_objects__[__g_static_objects_counter__].flags = flags; \
			__g_static_objects__[__g_static_objects_counter__].id = id; \
			__g_static_objects__[__g_static_objects_counter__].p_obj = p_obj; \
			__g_static_objects_counter__++; \
			p_obj->object_init();\
		}\
	} \
	_register_object_t *__g_p_soreg__ = __register_static_object__;

#define ADD_SO_STORAGE() \
	__g_so_storage_index__ = __g_p_repository__->add_so_storage( \
			__g_static_objects__, __g_static_objects_counter__, __g_so_storage_limit__);
#define INIT_SO_STORAGE(index) \
	__g_p_repository__->init_so_storage(index);	

#endif

