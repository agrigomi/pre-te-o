#ifndef __I_BASE__
#define __I_BASE__

#define I_BASE			"iBase"
#define I_REPOSITORY_BASE	"iRepositoryBase"

#include "mgtype.h"
#include "options.h"
#include "compiler.h"

#define DECLARE_INTERFACE(iname, _class_name_, _base_class_name_) \
	_class_name_() {} \
	~_class_name_() {} \
	virtual _cstr_t interface_name(void) 	{ return iname; }\
	virtual _cstr_t _interface_name(void)	{ return _base_class_name_::interface_name(); }

class iRepositoryBase;

#define REPOSITORY	_m_p_rb_
#define ERROR_CODE	_m_err_

#define DEFAULT_RAM_LIMIT	0xffffffffffffffffLLU

typedef struct {
	_str_t	name;
	_u32	size;
	_u16	version;
}_base_info_t;

class iBase {
public:
	iRepositoryBase *REPOSITORY;
	_u32 ERROR_CODE;

	DECLARE_INTERFACE(I_BASE, iBase, iBase);

	virtual void base_info(_base_info_t *p)=0;

	_str_t object_name(void) {	// pointer to NULL terminated string as object name
		_base_info_t i;
		base_info(&i);
		return i.name;
	}
	_u32 object_size(void) {	// object size in bytes
	 	_base_info_t i;
		base_info(&i);
		return i.size;
	}
	_u16 object_version(void) {  // two bytes object version
		_base_info_t i;
		base_info(&i);
		return i.version;
	}

	// after construction initialization step (early init)
	virtual void object_init(void) {}

	// Autoinit object with help of repository (second initialization step)
	virtual bool object_init(iRepositoryBase *p_rb) {
		if(p_rb)
			return true;
		else
			return false;
	}

	_u32 object_error(void) { 
		return ERROR_CODE;	// return the last error code ( 0 = no error )
	}

	virtual iBase *object_clone(void *p) {
		_u8 *dst = (_u8 *)p;
		_u8 *src = (_u8 *)this;
		_u32 sz = object_size();
		
		while(sz--)
			*dst++ = *src++;
		
		return (iBase *)p;
	}

	// destroy the cloned object
	virtual void object_destroy(void) {}
};


// flags
#define RY_CLONEABLE	(1<<0)
#define RY_CLONE	RY_CLONEABLE
#define RY_STANDALONE	(1<<1)
#define RY_ONCE		RY_STANDALONE
#define RY_UNIQUE	(1<<2)

// error codes
#define RY_OK			0
#define RY_ERR_DUPLICATE_ID	1
#define RY_ERR_INVALID_FLAGS	2
#define RY_ERR_NOT_FOUND	3
#define RY_ERR_NO_SPACE		4
#define RY_ERR_ALREADY_EXISTS	5
#define RY_OBJECT_INIT_FAILED	6
#define RY_ERR			0xffff

#define RY_NO_OBJECT_ID		0
#define RY_INVALID_INDEX	0xffffffff

typedef _u8 	_ry_flags_t;
typedef _u16	_ry_id_t;

typedef struct {
	_cstr_t 	_interface_name; // base interface name
	_cstr_t 	interface_name;	 // interface name
	_cstr_t 	object_name;
	_ry_flags_t 	flags;
	_ry_id_t	id;
	_u32		size;
} _object_info_t;

// ogject request flags
#define ORF_NAME	(1<<0)
#define ORF_INTERFACE	(1<<1)
#define ORF_PINTERFACE	(1<<2)
#define ORF_ID		(1<<3)
#define ORF_CMP_OR	(1<<7) // comparsion type OR (AND is by default)

typedef struct {
	_u8 	r_flags;
	_str_t	interface;
	_str_t	name;
	_str_t	pinterface;
	_u16	id;
	_u32	index;
}_object_request_t;

// prototype of static objects registration (SOR) routine
typedef void _register_object_t(_ry_flags_t, iBase *, _ry_id_t id);
extern _register_object_t *__g_p_soreg__;

// used this macro in constructor
#define REGISTER_OBJECT(flags, id) \
	ERROR_CODE = 0; \
	REPOSITORY = 0; \
	if(__g_p_soreg__) \
		__g_p_soreg__(flags, this, id)

class iRepositoryBase {
public:
	DECLARE_INTERFACE(I_REPOSITORY_BASE, iRepositoryBase, iRepositoryBase);	
	
	virtual iBase *get_object_by_interface(_cstr_t interface_name, _ry_flags_t flags, 
						_ulong limit=DEFAULT_RAM_LIMIT)=0;
	virtual iBase *get_object_by_parent_interface(_cstr_t interface_name, _ry_flags_t flags, 
						_ulong limit=DEFAULT_RAM_LIMIT)=0;
	virtual iBase *get_object_by_name(_cstr_t object_name, _ry_flags_t flags, 
						_ulong limit=DEFAULT_RAM_LIMIT)=0;
	virtual iBase *get_object_by_id(_ry_id_t id, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT)=0;
	virtual iBase *get_object(_object_request_t *req, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT)=0;
	virtual bool get_object_info(_object_request_t *req, _object_info_t *p_info)=0;
	virtual void release_object(iBase *p_obj)=0;
	virtual _u32 objects_count(void)=0;
};

#define DECLARE_BASE(_class_name_) \
	void base_info(_base_info_t *p); \
	_class_name_ *object_clone(void *p);

#define DECLARE_EARLY_INIT() 	void object_init(void)
#define DECLARE_INIT()		bool object_init(iRepositoryBase *)
#define DECLARE_DESTROY()	void object_destroy(void)
#define DECLARE_SELFCLONE(_class_name_) _class_name_ *object_clone(void)

	
#define IMPLEMENT_BASE(_class_name_, _name_, _version_) \
	static _class_name_ __g_static_object__##_class_name_; \
	void _class_name_::base_info(_base_info_t *p) { p->name=(_str_t)_name_; \
							p->size=sizeof(_class_name_); p->version=_version_; } \
	_class_name_ *_class_name_::object_clone(void *p) { return (_class_name_ *)iBase::object_clone(p); }

#define IMPLEMENT_EARLY_INIT(_class_name_) \
	void _OPTIMIZE_SIZE_ _class_name_::object_init(void)
	
#define IMPLEMENT_INIT(_class_name_, p_repository_base) \
	bool _OPTIMIZE_SIZE_ _class_name_::object_init(iRepositoryBase *p_repository_base)

#define IMPLEMENT_DESTROY(_class_name_) \
	void _OPTIMIZE_SIZE_ _class_name_::object_destroy(void)

#define IMPLEMENT_SELFCLONE(_class_name_) \
	_class_name_ *_class_name_::object_clone(void) 

#define STORE_REPOSITORY(p_repository_base) \
	REPOSITORY = p_repository_base
	
#define OBJECT_BY_INTERFACE(iname, flags) \
	REPOSITORY->get_object_by_interface(iname, flags)
#define RAM_LIMITED_OBJECT_BY_INTERFACE(iname, flags, limit) \
	REPOSITORY->get_object_by_interface(iname, flags, limit)
	
#define OBJECT_BY_PARENT_INTERFACE(iname, flags) \
	REPOSITORY->get_object_by_parent_interface(iname, flags)
#define RAM_LIMITED_OBJECT_BY_PARENT_INTERFACE(iname, flags, limit) \
	REPOSITORY->get_object_by_parent_interface(iname, flags, limit)

#define OBJECT_BY_NAME(name, flags) \
	REPOSITORY->get_object_by_name(name, flags)
#define RAM_LIMITED_OBJECT_BY_NAME(name, flags, limit) \
	REPOSITORY->get_object_by_name(name, flags, limit)
	
#define OBJECT_BY_ID(id, flags) \
	REPOSITORY->get_object_by_id(id, flags)
#define RAM_LIMITED_OBJECT_BY_ID(id, flags, limit) \
	REPOSITORY->get_object_by_id(id, flags, limit)
	
#define OBJECT_BY_INDEX(idx, flags) \
	REPOSITORY->get_object_by_index(idx, flags)

#define RELEASE_OBJECT(object) \
	if(REPOSITORY) {\
		REPOSITORY->release_object(object); \
		object = 0; \
	}

#define CALL_DESTROY() object_destroy();
	
#endif // __I_BASE__

