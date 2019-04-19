#ifndef __I_SYNC_OBJECT_H__
#define __I_SYNC_OBJECT_H__

#define I_MUTEX		"iMutex"
#define I_EVENT		"iEvent"
#define I_BROADCAST	"iBroadcast"

#include "iBase.h"

#define MUTEX_TIMEOUT_INFINITE	0xffffffff

#define MF_TIMEOUT_RESET		(1<<0)
#define MF_TIMEOUT_RESET_WITH_HANDLE	(1<<1)

typedef _u64	HMUTEX;

class iMutex:public iBase {
public:
	DECLARE_INTERFACE(I_MUTEX, iMutex, iBase);
	
	virtual void init(void)=0;
	virtual HMUTEX try_lock(HMUTEX hm=0)=0; 
	virtual HMUTEX lock(HMUTEX hm=0, 
			    _u32 timeout=MUTEX_TIMEOUT_INFINITE, 
			    _u8 flags=0)=0; 
	virtual void unlock(HMUTEX hm)=0;
	virtual void reset(void)=0;
};

#define EVENT_TIMEOUT_INFINITE	0xffffffff
#define EVENT_TIMEOUT		0
#define EVENT_DEFAULT_MASK	0xffffffff

typedef void _t_event_wait(void);

class iEvent:public iBase {
public:
	DECLARE_INTERFACE(I_EVENT, iEvent, iBase);

	virtual void init(_t_event_wait *p_cb_wait=0)=0;
	virtual _u32 wait(_u32 mask=EVENT_DEFAULT_MASK,
			  _u32 timeout=EVENT_TIMEOUT_INFINITE
			 )=0;
	virtual _u32 check(_u32 mask=EVENT_DEFAULT_MASK)=0;
	virtual void set(_u32 mask)=0;
};

#define MSG_DRIVER_INIT		101
#define MSG_DRIVER_READY	102
#define MSG_OBJECT_DESTROY	103
#define MSG_DRIVER_UNINIT	104
#define MSG_DRIVER_REMOVE	105

typedef struct {
	_u32	msg;
	_ulong	data;
}_msg_t;

typedef void _msg_handler_t(_msg_t*, void*);

class iBroadcast:public iBase {
public:
	DECLARE_INTERFACE(I_BROADCAST, iBroadcast, iBase);

	virtual void add_handler(_msg_handler_t *p_hcb, void *p_data)=0;
	virtual void remove_handler(_msg_handler_t *p_hcb)=0;
	virtual void send_msg(_msg_t *p_msg)=0;
};

#endif

