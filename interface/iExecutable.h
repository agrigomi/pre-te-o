#ifndef __I_EXECUTABLE__
#define __I_EXECUTABLE__

#define I_EXECUTABLE	"iExecutable"
#define I_EXT_LOADER	"iExtLoader"

#include "iVFS.h"

#define ELF64_CLASS_NAME	"cELF64"

class iExecutable:public iBase {
public:
	DECLARE_INTERFACE(I_EXECUTABLE, iExecutable, iBase);

	virtual bool init(FSHANDLE hfile  // filesystem entry (source)
			 )=0;
		 
	// identify executable format
	virtual bool identify(void)=0;
	virtual _u32 program_size(void)=0;
	virtual void *entry(void)=0;
	// load at destination address
	virtual bool load(_u8 *dst)=0;
	// load at default virtual address
	virtual bool load(void)=0;
	virtual _u32 exec(void *p_args)=0;
};

#define MAX_EXT_NAME	16

typedef void*	HEXTENSION;
typedef void _cb_ext_info_t(_str_t name, _u32 size, _ulong addr, void *p_udata);

class iExtLoader:public iBase {
public:
	DECLARE_INTERFACE(I_EXT_LOADER, iExtLoader, iBase);

	virtual HEXTENSION load(_str_t ext_path, _str_t name, _u32 sz_name=0)=0;
	virtual void exec(HEXTENSION hext, _str_t args)=0;
	virtual bool remove(HEXTENSION hext)=0;
	virtual void list(_cb_ext_info_t *p_cb, void *p_udata)=0;
};

#endif

