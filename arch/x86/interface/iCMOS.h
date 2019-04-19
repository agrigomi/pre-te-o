#ifndef __I_CMOS_H__
#define __I_CMOS_H__

#include "iSyncObject.h"

#define I_CMOS	"iCMOS"

class iCMOS:public iBase {
public:
	DECLARE_INTERFACE(I_CMOS, iCMOS, iBase);
	virtual HMUTEX lock(HMUTEX hlock=0, _u32 timeout=MUTEX_TIMEOUT_INFINITE)=0;
	virtual void unlock(HMUTEX hlock)=0;
	virtual _u8 read(_u8 reg, bool _lock=true, HMUTEX hlock=0)=0;
	virtual void write(_u8 reg, _u8 value, bool _lock=true, HMUTEX hlock=0)=0;
};

#endif
