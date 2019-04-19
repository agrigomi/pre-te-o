#include "repository.h"
#include "compiler.h"

#ifdef _CORE_
#include "startup_context.h"
_startup_context_t *__g_p_startup_context__=0;
#endif


#ifdef _EXT_
#include "ext.h"
_ext_context_t *__g_p_ext_context__=0;
iRepository *__g_p_repository__=0;
#endif

IMPLEMENT_SO_STORAGE(MAX_STATIC_OBJECTS);

extern _ulong _start_ctors, _end_ctors, _start_dtors, _end_dtors;

extern "C" bool _kmodule_exit_(void) {
	bool r = false;
#ifdef _EXT_
	// try to remove SO storage
	if((r = __g_p_repository__->remove_so_storage(__g_p_ext_context__->so_index))) {
		//- call all the static destructors in the list.
		for(_ulong *destructor(&_start_dtors); destructor < &_end_dtors; ++destructor)
			((void (*) (void)) (*destructor)) ();
	}
#endif
	return r;
}

#ifdef _CORE_
extern "C" void _kmodule_init_(_startup_context_t *p_context) {
	__g_p_startup_context__ = (_startup_context_t *)(p_context->vbase + (_ulong)p_context);
#endif

#ifdef _EXT_
extern "C" void _kmodule_init_(_ext_context_t *p_context) {
	__g_p_ext_context__ = p_context;
	__g_p_repository__ = p_context->p_repository;
#endif

	//- call all the static constructors in the list.
	for(_ulong *constructor(&_start_ctors); constructor < &_end_ctors; ++constructor)
		((void (*) (void)) (*constructor)) ();


#ifdef _CORE_
	ADD_SO_STORAGE();
	// call main entry point
	_kentry(__g_p_startup_context__);
#endif

#ifdef _EXT_
	__g_p_ext_context__->so_index = ADD_SO_STORAGE();
	if(__g_p_ext_context__->so_index != INVALID_SO_STORAGE_INDEX)
		INIT_SO_STORAGE(__g_p_ext_context__->so_index);
	// call module entry
	__g_p_ext_context__->p_cb_exit = _kmodule_exit_;
	_main(p_context->args);
#endif
}

#include "iMemory.h"

static iHeap *__g_p_heap__=0;
static iHeap *get_heap(void) {
	if(!__g_p_heap__)
		__g_p_heap__ = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	return __g_p_heap__;
}
void* operator new(unsigned long s) {
	void *r = 0;
	iHeap *p_heap = get_heap();
	if(p_heap)
		r = p_heap->alloc(s);

	return r;
}

void operator delete(void *p) {
	iHeap *p_heap = get_heap();
	if(p_heap)
		p_heap->free(p);
}

void operator delete(void *p, unsigned long sz) {
	iHeap *p_heap = get_heap();
	if(p_heap)
		p_heap->free(p, sz);
}
