#include "iTimer.h"
#include "iSystemLog.h"
#include "pthread.h"
#include "stdlib.h"
#include "unistd.h"
#include "iMemory.h"

#define INITIAL_TIMER_PACK	32

typedef struct {
	_u8	flags;
	_u8	state;
	_u32	countdown;
	_u32	initial;
	_timer_callback_t *p_callback;
	pthread_t thread;
	void	*p_udata;
}__attribute__((packed)) _timer_info_t;

class cTimer:public iTimer {
public:
	cTimer() {
		DRIVER_TYPE = DTYPE_DEV;
		DRIVER_CLASS = DCLASS_SYS;
		m_count = 0;
		m_p_timers = 0;
		str_cpy(DRIVER_IDENT, (_str_t)"System Timer", sizeof(DRIVER_IDENT));
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	virtual ~cTimer() {}
	
	DECLARE_BASE(cTimer);
	DECLARE_INIT();
	DECLARE_DESTROY();

	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

	HTIMER create(_u32 resolution, _u8 flags, _timer_callback_t *p_callback, void *p_udata);
	void suspend(HTIMER htimer);
	void resume(HTIMER htimer);
	void remove(HTIMER htimer);
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {}

protected:
	_u32	m_count;
	_timer_info_t	*m_p_timers;
	void alloc_timer_pack(HMUTEX hlock=0);
};

#define TSTATE_CALL		(1<<0)
#define TSTATE_RUNNING		(1<<1)
#define TSTATE_ACTIVE		(1<<2)

IMPLEMENT_BASE(cTimer, "cTimer", 0x0001);
IMPLEMENT_INIT(cTimer, rbase) {
	bool r = true;
	REPOSITORY = rbase;
	INIT_MUTEX();
	alloc_timer_pack();
	REGISTER_DRIVER();
	return r;
}
IMPLEMENT_DESTROY(cTimer) {
	RELEASE_MUTEX();
}

IMPLEMENT_DRIVER_INIT(cTimer, host) {
	bool r = false;
	DRIVER_HOST = host;
	
	DRIVER_HOST = GET_DRIVER_ROOT();
	r = true;

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cTimer) {
	//???
}

void cTimer::alloc_timer_pack(HMUTEX hlock) {
	_timer_info_t *p_current_pack, *p_new_pack;
	
	HMUTEX _hlock = lock(hlock);
	
	p_current_pack = m_p_timers;
	iHeap *p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		_u32 new_pack_size = (m_count + INITIAL_TIMER_PACK) * sizeof(_timer_info_t);
		p_new_pack = (_timer_info_t *)p_heap->alloc(new_pack_size);
		if(p_new_pack) {
			mem_set((_u8 *)p_new_pack, 0, new_pack_size);
			if(p_current_pack) {
				mem_cpy((_u8 *)p_new_pack, (_u8 *)p_current_pack, m_count * sizeof(_timer_info_t));
				p_heap->free(p_current_pack);
			}
			
			m_p_timers = p_new_pack;
			m_count += INITIAL_TIMER_PACK;
		}
	}
	
	unlock(_hlock);
}
void *_thread_proc_(void *p_udata) {
	_timer_info_t *p_ti = (_timer_info_t *)p_udata;
	p_ti->state = TSTATE_RUNNING;
	while(p_ti->state & TSTATE_RUNNING) {
		usleep(1000*p_ti->countdown);
		p_ti->p_callback(p_ti->p_udata);
	}
	return 0;
}

HTIMER cTimer::create(_u32 resolution, _u8 flags, _timer_callback_t *p_callback, void *p_udata) {
	HTIMER r = INVALID_TIMER_HANDLE;
	HMUTEX hlock = lock();
	
	_u32 i;
_try_create_:	
	for(i = 0; i < m_count; i++) {
		if(!(m_p_timers[i].state & TSTATE_ACTIVE)) {
			m_p_timers[i].state |= TSTATE_ACTIVE;
			m_p_timers[i]. flags = flags;
			if(!(flags & TF_SUSPEND))
				m_p_timers[i].state |= TSTATE_RUNNING;
			m_p_timers[i].initial = m_p_timers[i].countdown = resolution;
			m_p_timers[i].p_callback = p_callback;
			m_p_timers[i].p_udata = p_udata;
			r = i;
			pthread_create(&(m_p_timers[i].thread), 0, _thread_proc_, &m_p_timers[i]);
			break;
		}
	}
	
	if(i == m_count) {
		alloc_timer_pack(hlock);
		goto _try_create_;
	}
	
	unlock(hlock);
	return r;
}
void cTimer::suspend(HTIMER htimer) {

}
void cTimer::resume(HTIMER htimer) {

}
void cTimer::remove(HTIMER htimer) {

}

