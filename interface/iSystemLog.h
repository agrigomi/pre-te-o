#ifndef __iSYSTEM_LOG_H__
#define __iSYSTEM_LOG_H__

#define I_SYSTEM_LOG	"iSystemLog"


#include "iBase.h"
#include "iSyncObject.h"

class iSystemLog:public iBase {
public:
	DECLARE_INTERFACE(I_SYSTEM_LOG, iSystemLog, iBase);
	
	virtual void init(_u32 npages)=0; // allocate 'npages' for system log
	virtual _u32 write(_cstr_t text, HMUTEX hlock=0)=0;
	virtual _u32 fwrite(_cstr_t format,...)=0;
	virtual _u32 fwrite(HMUTEX hlock, _cstr_t format,...)=0;
	virtual void enable_output(void)=0;
	virtual void disable_output(void)=0;
	virtual void disable_lock(void)=0;
	virtual void enable_lock(void)=0;
	virtual HMUTEX lock(HMUTEX hlock=0, _u32 timeout=MUTEX_TIMEOUT_INFINITE, _u8 flags=0)=0;
	virtual HMUTEX try_lock(HMUTEX hlock=0)=0;
	virtual void unlock(HMUTEX hlock)=0;
	virtual iMutex *get_mutex(void)=0;
};

#endif
