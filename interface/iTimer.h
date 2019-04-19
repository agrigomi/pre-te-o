#ifndef __I_TIMER_H__
#define __I_TIMER_H__

#include "iDriver.h"

#define I_TIMER	"iTimer"

#define TF_SUSPEND	(1<<0)
#define TF_PERIODIC	(1<<1)

#define INVALID_TIMER_HANDLE	0xffffffff

typedef _u32 HTIMER;
typedef void _timer_callback_t(void *p_udata);

class iTimer:public iDriverBase {
public:
	DECLARE_INTERFACE(I_TIMER, iTimer, iDriverBase);
	
	virtual HTIMER create(_u32 resolution, _u8 flags, _timer_callback_t *p_callback, void *p_udata)=0;
	virtual void suspend(HTIMER htimer)=0;
	virtual void resume(HTIMER htimer)=0;
	virtual void remove(HTIMER htimer)=0;
};

#endif

