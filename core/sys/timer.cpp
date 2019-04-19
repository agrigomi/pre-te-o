#include "iTimer.h"
#include "iSystemLog.h"
#include "iMemory.h"
#include "iSyncObject.h"
#include "iScheduler.h"
#include "iCPUHost.h"
#include "iRTC.h"
#include "intdef.h"
#include "str.h"

//#define _DEBUG_
#include "debug.h"

#define LOG(_fmt_, ...) \
	if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__) 
	
#define TSTATE_CALL		(1<<0)
#define TSTATE_RUNNING		(1<<1)
#define TSTATE_ACTIVE		(1<<2)

#define INITIAL_TIMER_PACK	32
#define TT_STACK_SIZE		8192

typedef struct {
	_u8	flags;
	_u8	state;
	_u32	countdown;
	_u32	initial;
	_timer_callback_t *p_callback;
	void	*p_udata;
}__attribute__((packed)) _timer_info_t;

class cTimer:public iTimer {
public:
	cTimer() {
		m_p_syslog = 0;
		m_count = 0;
		m_p_timers = 0;
		m_p_blender = 0;
		m_thread_running = false;
		m_thread_state = false;
		m_timer_state = false;
		DRIVER_TYPE = DTYPE_DEV;
		DRIVER_CLASS = DCLASS_SYS;
		str_cpy(DRIVER_IDENT, (_str_t)"System Timer", sizeof(DRIVER_IDENT));
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	//virtual ~cTimer() {}
	
	DECLARE_BASE(cTimer);
	DECLARE_INIT();
	DECLARE_DESTROY();

	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

	HTIMER create(_u32 resolution, _u8 flags, _timer_callback_t *p_callback, void *p_udata);
	void suspend(HTIMER htimer);
	void resume(HTIMER htimer);
	void remove(HTIMER htimer);

protected:
	iSystemLog *m_p_syslog;
	_u32	m_count;
	_timer_info_t	*m_p_timers;
	iBlender *m_p_blender;
	iRTC *m_p_rtc;
	bool m_thread_running;
	_u32 m_thread_id;
	bool m_thread_state;
	bool m_timer_state;
	
	friend void _rtc_callback(void *p_data);
	friend _s32 _timer_thread(void *p_udata);
	void rtc_callback(void);
	void timer_thread(void);
	void alloc_timer_pack(HMUTEX hlock=0);
	void timer_on(bool on);
	void thread_on(bool on);
	void update_state(void);
};


IMPLEMENT_BASE(cTimer, "cTimer", 0x0001);
IMPLEMENT_INIT(cTimer, rbase) {
	bool r = true;
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	INIT_MUTEX();
	alloc_timer_pack();
	REGISTER_DRIVER();
	return r;
}
IMPLEMENT_DESTROY(cTimer) {
	RELEASE_MUTEX();
	iHeap *p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		p_heap->free(m_p_timers, m_count * sizeof(_timer_info_t));
		m_p_timers = 0;
		m_count = 0;
	}
}

_s32 _timer_thread(void *p_udata) {
	cTimer *p_obj = (cTimer *)p_udata;
	p_obj->timer_thread();
	return 0;
}

void _rtc_callback(void *p_data) {
	cTimer *p_obj = (cTimer *)p_data;
	p_obj->rtc_callback();
}

IMPLEMENT_DRIVER_INIT(cTimer, host) {
	bool r = false;
	DRIVER_HOST = host;
	
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host && p_cpu_host->is_running()) {
		m_p_blender = get_blender();
		m_p_rtc = (iRTC *)OBJECT_BY_INTERFACE(I_RTC, RY_STANDALONE);

		if(m_p_rtc && m_p_blender) {
			m_thread_id = m_p_blender->create_systhread(_timer_thread, // thread entry
									TT_STACK_SIZE, // stack size
									this, // thread data
									true // create suspended
								     );
			m_p_rtc->set_callback(_rtc_callback, this);

			DRIVER_HOST = GET_DRIVER_ROOT();
			r = true;
		}
	}
	return r;
}
IMPLEMENT_DRIVER_UNINIT(cTimer) {
	m_thread_running = false;
	thread_on(true);
	if(m_p_rtc)
		m_p_rtc->del_callback(_rtc_callback);
}

void cTimer::timer_on(bool on) {
	bool change = false;
	if(on) {
		if(!m_timer_state)
			change = true;
			
	} else {
		if(m_timer_state)
			change = true;
	}

	if(m_p_rtc && change) {
		m_p_rtc->enable_irq(on);
		m_timer_state = on;
	}
}

void cTimer::thread_on(bool on) {
	if(on) {
		if(!m_thread_state) {
			if(m_p_blender)
				m_p_blender->resume_systhread(m_thread_id);
			m_thread_state = true;
		}
	} else {
		if(m_thread_state) {
			if(m_p_blender)
				m_p_blender->suspend_systhread(m_thread_id);
			m_thread_state = false;
		}
	}
}

void cTimer::alloc_timer_pack(HMUTEX hlock) {
	_timer_info_t *p_current_pack, *p_new_pack;
	
	bool irqs = enable_interrupts(false);
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
				p_heap->free(p_current_pack, m_count * sizeof(_timer_info_t));
			}
			
			m_p_timers = p_new_pack;
			m_count += INITIAL_TIMER_PACK;
		}

		RELEASE_OBJECT(p_heap);
	}
	
	enable_interrupts(irqs);
	unlock(_hlock);
}

void cTimer::rtc_callback(void) {
	HMUTEX hlock = lock();
	bool thread = false;
	for(_u32 i = 0; i < m_count; i++) {
		if((m_p_timers[i].state & TSTATE_ACTIVE) && (m_p_timers[i].state & TSTATE_RUNNING)) {
			if(m_p_timers[i].countdown)
				m_p_timers[i].countdown--;
			if(m_p_timers[i].countdown <= 1)
				thread = true;
		}
	}

	thread_on(thread);
	unlock(hlock);
}

void cTimer::timer_thread(void) {
	m_thread_running = true;
	m_thread_state = true;
	DBG("\r\n---- timer thread running ----\r\n","");

	while(m_thread_running) {
		bool irqs = m_p_blender->enable_interrupts(false);
		HMUTEX hlock = lock();
		for(_u32 i = 0; i < m_count; i++) {
			if((m_p_timers[i].state & TSTATE_ACTIVE) && (m_p_timers[i].state & TSTATE_RUNNING)) {
				if(m_p_timers[i].countdown == 0) {
					_timer_callback_t *p_callback = m_p_timers[i].p_callback;
					void *p_udata = m_p_timers[i].p_udata;
					if(p_callback) {
						m_p_timers[i].state |= TSTATE_CALL;
						unlock(hlock);
						m_p_blender->enable_interrupts(irqs);
						p_callback(p_udata);
						irqs = m_p_blender->enable_interrupts(false);
						hlock = lock();
						m_p_timers[i].state &= ~TSTATE_CALL;
					}
					if(!(m_p_timers[i].flags & TF_PERIODIC)) {
						m_p_timers[i].state &= ~TSTATE_ACTIVE;
						update_state();
					}
					
					m_p_timers[i].countdown = m_p_timers[i].initial;
				}
			}
		}
		
		unlock(hlock);
		m_p_blender->enable_interrupts(irqs);
		m_p_blender->switch_context();
	}
}

void cTimer::update_state(void) {
	bool on  = false;
	
	for(_u32 i = 0; i < m_count; i++) {
		if((m_p_timers[i].state & TSTATE_ACTIVE) && (m_p_timers[i].state & TSTATE_RUNNING)) {
			on = true;
			break;
		}
	}
	
	timer_on(on);
	thread_on(on);
}

HTIMER cTimer::create(_u32 resolution, _u8 flags, _timer_callback_t *p_callback, void *p_udata) {
	HTIMER r = INVALID_TIMER_HANDLE;
	
	bool irqs = m_p_blender->enable_interrupts(false);
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
			break;
		}
	}
	
	if(i == m_count) {
		alloc_timer_pack(hlock);
		goto _try_create_;
	}
	
	update_state();
	
	unlock(hlock);
	m_p_blender->enable_interrupts(irqs);
	return r;
}

void cTimer::suspend(HTIMER htimer) {
	bool irqs = m_p_blender->enable_interrupts(false);
	HMUTEX hlock = lock();

	if(htimer < m_count)
		m_p_timers[htimer].state &= ~TSTATE_RUNNING;
	
	update_state();
	unlock(hlock);
	m_p_blender->enable_interrupts(irqs);
}

void cTimer::resume(HTIMER htimer) {
	bool irqs = m_p_blender->enable_interrupts(false);
	HMUTEX hlock = lock();

	if(htimer < m_count)
		m_p_timers[htimer].state |= TSTATE_RUNNING;
	
	update_state();
	unlock(hlock);
	m_p_blender->enable_interrupts(irqs);
}

void cTimer::remove(HTIMER htimer) {
	bool irqs = m_p_blender->enable_interrupts(false);
	HMUTEX hlock = lock();

	if(htimer < m_count)
		m_p_timers[htimer].state &= ~TSTATE_ACTIVE;
	
	update_state();
	unlock(hlock);
	m_p_blender->enable_interrupts(irqs);
}
