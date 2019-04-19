#include "iScheduler.h"
#include "iSystemLog.h"
#include "iCPUHost.h"
#include "iDataStorage.h"
#include "iMemory.h"
#include "str.h"

//#define _DEBUG_
#include "debug.h"

#define LOG(_fmt_, ...) \
	if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__) 

#define IDLE_WORK		2000
#define DEFAULT_BOOT_COUNTDOWN	0x00040000
#define DEFAULT_APP_COUNTDOWN	0x00400000

class cBlender:public iBlender {
public:
	cBlender() {
		m_p_syslog = 0;
		m_p_cpu_host = 0;
		m_q_systhread = 0;
		m_p_cpu = 0;
		m_p_pma = 0;
		m_idle_id = 0;
		m_thread_id = 1;
		m_is_init = false;
		m_is_running = false;
		m_timer = DEFAULT_APP_COUNTDOWN;
		m_time_div = DEFAULT_TIME_DIV;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	
	//virtual ~cBlender() {}
	
	DECLARE_BASE(cBlender);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	void init(_u16 cpu_id);
	void start(void);
	void stop(void);
	bool is_running(void) { return m_is_running; }
	void set_timer(_u32 timer);
	void set_time_div(_u8 div);
	_u8  get_time_div(void) { return m_time_div; }
	bool enable_interrupts(bool enable);
	void idle(void);
	void switch_context(void *p_cpu_state=0);
	void timer(void *p_cpu_state);
	void exception(_u8 enx, void *p_cpu_state);
	void breakpoint(void *p_cpu_state);
	void debug(void *p_cpu_state);
	void memory_exception(_ulong addr, void *p_cpu_state);
	_u32 create_systhread(_thread_t *p_entry, _u32 stack_sz, void *p_data, 
				bool suspend=false, _u32 resolution=0);
	_u32 create_usr_systhread(_thread_t *p_entry, void *p_stack, _u32 stack_sz, void *p_data, 
				bool suspend=true, _u32 resolution=0);
	void terminate_systhread(_u32 id);
	void sleep_systhread(_u32 countdown);
	void sleep_systhread(_u32 id, _u32 countdown);
	void suspend_systhread(_u32 id);
	void resume_systhread(_u32 id);
	_cthread_t *get_systhread(_u32 id);
	void set_state_callback(_u32 id, _tstate_t *p_tscb, void *p_cb_data);
	_u32 current_systhread_id(void);
	_u32 wait_event(iEvent *p_event, _u32 mask=EVENT_DEFAULT_MASK, _u32 timeout=EVENT_TIMEOUT_INFINITE);
	HMUTEX acquire_mutex(iMutex *p_mutex, HMUTEX hlock=0, _u32 timeout=MUTEX_TIMEOUT_INFINITE);

protected:
	bool	m_is_running;
	bool 	m_is_init;
	_u16	m_idle_work;
	_u32	m_idle_id;
	_u32	m_thread_id;
	_u64	m_timestamp;
	iSystemLog *m_p_syslog;
	iCPUHost *m_p_cpu_host;
	iQueue *m_q_systhread;
	iCPU 	*m_p_cpu;
	iPMA	*m_p_pma;
	_u32	m_timer;
	_u8	m_time_div;

	void create_idle(void);
	_cthread_t *get_thread(_u32 id);
	_u32 _adjust_timer(void);
	void set_thread_state(_cthread_t *p_ct, _u16 flags);
};
		
IMPLEMENT_BASE(cBlender, "cBlender", 0x001);

IMPLEMENT_INIT(cBlender, rb) {
	bool r = false;
	REPOSITORY = rb;
	
	INIT_DEBUG();

	m_p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_q_systhread = (iQueue *)OBJECT_BY_INTERFACE(I_QUEUE, RY_CLONE);
	m_p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);

	if(m_p_cpu_host && m_q_systhread && m_p_pma) {
		m_q_systhread->init(sizeof(_cthread_t));
		r = true;
	}

	return r;
}

IMPLEMENT_DESTROY(cBlender) {
	RELEASE_OBJECT(m_q_systhread);
	RELEASE_OBJECT(m_p_syslog);
	RELEASE_OBJECT(m_p_pma);
}

void cBlender::init(_u16 cpu_id) {
	if(!m_is_init) {
		if((m_p_cpu = m_p_cpu_host->get_cpu(cpu_id))) {
			// fix blender start time
			DBG("init blender CPU(%d)\r\n", m_p_cpu->cpu_id());
			m_timestamp = m_p_cpu->timestamp();
			
			// NOTE: the first thread in queue must be IDLE (THIS CONTEXT)
			create_idle();
			
			// disable reinitialization
			m_is_init = true;
		}
	}
}

void cBlender::set_timer(_u32 timer) {
	m_timer = timer;
	DBG("BLENDER: CPU(%d) sheduling timer (0x%x)\r\n", 
			m_p_cpu->cpu_id(), m_timer * m_time_div);
	m_p_cpu->set_timer(m_timer * m_time_div);
}

void cBlender::start(void) {
	if(m_is_init) {
		// set the default context switch timing
		if(!m_is_running) {
			DBG("run blender CPU(%d)\r\n", m_p_cpu->cpu_id());
			if(m_p_cpu->is_boot())
				set_timer(DEFAULT_BOOT_COUNTDOWN);
			else
				set_timer(DEFAULT_APP_COUNTDOWN);
			m_is_running = true;
		}
	}
}

void cBlender::stop(void) {
	if(m_is_init) {
		if(m_is_running) {
			m_p_cpu->set_timer(0);
			m_is_running = false;
		}
	}
}
	
void cBlender::set_thread_state(_cthread_t *p_ct, _u16 flags) {
	p_ct->flags |= flags;
	if(p_ct->p_tstate_cb)
		p_ct->p_tstate_cb(this, p_ct, flags, p_ct->p_tscb_data);
}

void cBlender::set_time_div(_u8 div) {
	if(div >= MIN_TIME_DIV && div <= MAX_TIME_DIV) {
		m_time_div = div;
		if(m_p_cpu && m_is_running)
			set_timer(m_timer);
	}
}

bool cBlender::enable_interrupts(bool enable) {
	if(m_p_cpu)
		return m_p_cpu->enable_interrupts(enable);
	return false;
}

static _s32 starter_proc(void *p_data) {
	_cthread_t *p_tinfo = (_cthread_t *)p_data;

	iCPU *p_cpu = get_cpu();
	if(p_cpu) {
		p_tinfo->flags &= ~TS_PENDING;
		p_tinfo->flags |= TS_RUNNING;
		if(p_tinfo->p_tstate_cb)
			p_tinfo->p_tstate_cb(p_cpu->get_blender(), p_tinfo, TS_RUNNING, p_tinfo->p_tscb_data);
		
		// run thread
		p_tinfo->exit_code = p_tinfo->p_entry(p_tinfo->p_data);

		p_tinfo->flags &= ~TS_RUNNING;
		p_tinfo->flags |= TS_PENDING|TS_FINISHED;
		if(p_tinfo->p_tstate_cb)
			p_tinfo->p_tstate_cb(p_cpu->get_blender(), p_tinfo, TS_FINISHED, p_tinfo->p_tscb_data);
		DBG("BLENDER: exit systhread CPU(%d), ID=%d\r\n", p_tinfo->cpu, p_tinfo->id);
		while(1)
			p_cpu->switch_context();
	}
	
	return 0;
}

_u32 cBlender::create_systhread(_thread_t *p_entry, _u32 stack_sz, void *p_data, bool suspend, _u32 resolution) {
	_u32 r = 0;
	
	if(m_is_init) {
		_cthread_t thread;
		
		mem_set((_u8 *)&thread, 0, sizeof(_cthread_t));

		thread.p_entry = p_entry;
		thread.p_data = p_data;
		thread.dpl = DPL_SYS;
		thread.flags = TS_PENDING|((suspend)?TS_SUSPEND:0);
		thread.cpu = m_p_cpu->cpu_id();
		thread.id = m_thread_id;
		thread.resolution = resolution;
		
		if(m_p_cpu) { // alloc stack frame
			bool ints = m_p_cpu->enable_interrupts(false);
			HMUTEX hm = m_q_systhread->lock();

			_u32 stack_pages = ((stack_sz) ? stack_sz : 1) / m_p_pma->get_page_size();
			stack_pages += (((stack_sz) ? stack_sz : 1) % m_p_pma->get_page_size()) ? 1 : 0;
			thread.p_stack_base = m_p_pma->alloc(stack_pages);
			if(thread.p_stack_base) { // create CPU context
				thread.stack_sz = stack_pages * m_p_pma->get_page_size();
				_cthread_t *p_thread = (_cthread_t *)m_q_systhread->add(&thread, hm);
				m_p_cpu->create_context(thread.p_stack_base, thread.stack_sz, starter_proc, 
							DPL_SYS, p_thread, p_thread->context);
				r = m_thread_id;
				m_thread_id++;
			} else
				LOG("BLENDER: Failed to allocate stack frame for new thread\r\n","");
			
			m_q_systhread->unlock(hm);
			m_p_cpu->enable_interrupts(ints);
		}
	}
	
	return r;
}

_u32 cBlender::create_usr_systhread(_thread_t *p_entry, void *p_stack, _u32 stack_sz, void *p_data, 
					bool suspend, _u32 resolution) {
	_u32 r = 0;
	
	if(m_is_init) {
		_cthread_t thread;

		mem_set((_u8 *)&thread, 0, sizeof(_cthread_t));

		thread.p_entry = p_entry;
		thread.p_data = p_data;
		thread.dpl = DPL_USR;
		thread.flags = TS_PENDING|((suspend)?TS_SUSPEND:0);
		thread.cpu = m_p_cpu->cpu_id();
		thread.id = m_thread_id;
		thread.p_stack_base = p_stack;
		thread.stack_sz = stack_sz;
		thread.resolution = resolution;
		
		if(m_p_cpu) { 
			bool ints = m_p_cpu->enable_interrupts(false);
			HMUTEX hm = m_q_systhread->lock();

			_cthread_t *p_thread = (_cthread_t *)m_q_systhread->add(&thread, hm);
			m_p_cpu->create_context(thread.p_stack_base, thread.stack_sz, p_entry, 
						DPL_USR, p_data, p_thread->context);
			r = m_thread_id;
			m_thread_id++;
			
			m_q_systhread->unlock(hm);
			m_p_cpu->enable_interrupts(ints);
		}
	}
	
	return r;
}

void cBlender::create_idle(void) {
	_cthread_t thread;

	mem_set((_u8 *)&thread, 0, sizeof(_cthread_t));
	
	thread.dpl = DPL_SYS;
	thread.cpu = m_p_cpu->cpu_id();
	m_idle_id = thread.id = m_thread_id;
	m_idle_work = IDLE_WORK;
	m_thread_id++;
	m_q_systhread->add(&thread);
	DBG("BLENDER: create idle CPU(%d)\r\n", m_p_cpu->cpu_id());
}

void cBlender::idle(void) {
	if(m_is_init) {
		if(m_idle_work)
			m_idle_work--;
		else {
			bool irqs = m_p_cpu->enable_interrupts(false);
			HMUTEX hm = m_q_systhread->lock();

			_u32 i = 0;
			_u32 tcount = m_q_systhread->cnt(hm);
			_cthread_t *p = (_cthread_t *)m_q_systhread->first(hm);
			if(p) {
				do {
					if((p->flags & (TS_FINISHED|TS_EXCEPTION))) { // remove thread
						DBG("BLENDER: Remove thread (%d) CPU(%d)\r\n", p->id, p->cpu);
						//// release stack frame ////
						_u32 stack_pages = p->stack_sz / m_p_pma->get_page_size();
						stack_pages += (p->stack_sz % m_p_pma->get_page_size()) ? 1 : 0;
						m_p_pma->free(p->p_stack_base, stack_pages);
						////////////////////////////
						
						// remove from queue
						m_q_systhread->del(hm);
						tcount = m_q_systhread->cnt(hm);

						if((p = (_cthread_t *)m_q_systhread->current(hm)))
							continue;
						else
							break;
					} else {
						i++;
						if(!(p = (_cthread_t *)m_q_systhread->next(hm)))
							break;
					}
				}while(i < tcount);
			}

			m_idle_work = IDLE_WORK;
			m_q_systhread->unlock(hm);
			m_p_cpu->enable_interrupts(irqs);
		}
	}
}

void cBlender::switch_context(void *p_cpu_state) {
	if(m_is_init) {
		if(p_cpu_state) {
			HMUTEX hm = m_q_systhread->lock();
			
			///////////////////////////////////////////////////////////////
			// NOTE: KEEP THE CURRENT THREAD IN THE TOP OF QUEUE ALWAYS !!!
			///////////////////////////////////////////////////////////////
			_cthread_t *p_tc, *_p_tc;
			p_tc = _p_tc = (_cthread_t *)m_q_systhread->first(hm);
			if(p_tc) {
				do { // roll queue untill find the next available thread
					m_q_systhread->roll(hm);
					p_tc = (_cthread_t *)m_q_systhread->first(hm);
				}while(p_tc->flags & (	TS_FINISHED|
							TS_SLEEP|
							TS_SUSPEND|
							TS_BREAKPOINT|
							TS_DEBUG|
							TS_MEMORY_EXCEPTION|
							TS_EXCEPTION
						     )
				      );

				if(p_tc != _p_tc) {
					// save current context
					m_p_cpu->copy_cpu_state(_p_tc->context, p_cpu_state);
					_p_tc->flags &= ~TS_RUNNING;
					set_thread_state(_p_tc, TS_PENDING);

					if(_p_tc->resolution) {
						_p_tc->flags |= TS_SLEEP;
						_p_tc->tm_sleep += (_p_tc->resolution / m_time_div);
					}
					
					// context switch
					p_tc->flags &= ~TS_PENDING;
					set_thread_state(p_tc, TS_RUNNING);
					m_p_cpu->copy_cpu_state(p_cpu_state, p_tc->context);
				}
			}
			
			m_q_systhread->unlock(hm);
		} else
			m_p_cpu->switch_context();
	}
}

void cBlender::timer(void *p_cpu_state) {
	if(m_is_init) {
		HMUTEX hm = m_q_systhread->lock();

		////// SLEEP TIME CONTROL //////
		_u32 i = 0;
		_cthread_t *p_tc = (_cthread_t *)m_q_systhread->first(hm);
		if(p_tc) {
			p_tc->tm_sched++;
			_u32 tcount = m_q_systhread->cnt(hm);
			
			do {
				if(p_tc->flags & TS_SLEEP) {
					if(!p_tc->tm_sleep) // wake up
						p_tc->flags &= ~TS_SLEEP;
					else
						p_tc->tm_sleep--;
				}
				i++;
				if(!(p_tc = (_cthread_t *)m_q_systhread->next(hm)))
					break;
			}while(i < tcount);
		}
		/////////////////////////////////
		
		m_q_systhread->unlock(hm);
		switch_context(p_cpu_state);
	}
}

void cBlender::exception(_u8 exn, void *p_cpu_state) {
	if(m_is_init) {
		HMUTEX hm = m_q_systhread->lock();
		
		// the current thread (top of queue) causes a exception
		_cthread_t *p_tc = (_cthread_t *)m_q_systhread->first(hm);
		if(p_tc) {
			p_tc->exception = exn;
			set_thread_state(p_tc, TS_EXCEPTION);
			LOG("BLENDER: exception at CPU(%d):#%d\r\n", m_p_cpu->cpu_id(), exn);
			LOG("------ CURRENT CPU STATE ------------\r\n","");
			m_p_cpu->dump(p_cpu_state);
			//LOG("------ CURRENT THREAD STATE ---------\r\n","");
			//m_p_cpu->dump(p_tc->context);
		}
		
		m_q_systhread->unlock(hm);
		switch_context(p_cpu_state);
	}
}

void cBlender::breakpoint(void *p_cpu_state) {
	if(m_is_init) {
		HMUTEX hm = m_q_systhread->lock();
		
		// the current thread (top of queue) causes a exception
		_cthread_t *p_tc = (_cthread_t *)m_q_systhread->first(hm);
		if(p_tc)
			set_thread_state(p_tc, TS_BREAKPOINT);
		
		m_q_systhread->unlock(hm);
		switch_context(p_cpu_state);
	}
}

void cBlender::debug(void *p_cpu_state) {
	if(m_is_init) {
		HMUTEX hm = m_q_systhread->lock();
		
		// the current thread (top of queue) causes a exception
		_cthread_t *p_tc = (_cthread_t *)m_q_systhread->first(hm);
		if(p_tc)
			set_thread_state(p_tc, TS_DEBUG);
		
		m_q_systhread->unlock(hm);
		switch_context(p_cpu_state);
	}
}

void cBlender::memory_exception(_ulong addr, void *p_cpu_state) {
	if(m_is_init) {
		HMUTEX hm = m_q_systhread->lock();
		
		// the current thread (top of queue) causes a exception
		_cthread_t *p_tc = (_cthread_t *)m_q_systhread->first(hm);
		if(p_tc) {
			p_tc->mf_addr = addr;
			set_thread_state(p_tc, TS_MEMORY_EXCEPTION);
			DBG("BLENDER: memory exception at CPU(%d):0x%h\r\n", m_p_cpu->cpu_id(), addr);
		}
		
		m_q_systhread->unlock(hm);
		switch_context(p_cpu_state);
	}
}

void cBlender::terminate_systhread(_u32 id) {
	_cthread_t *pt = get_thread(id);
	if(pt)
		set_thread_state(pt, TS_FINISHED);
}

void cBlender::sleep_systhread(_u32 countdown) {
	bool irqs = m_p_cpu->enable_interrupts(false);
	HMUTEX hm = m_q_systhread->lock();
	_cthread_t *p_tc = (_cthread_t *)m_q_systhread->first(hm);
	if(p_tc) {
		p_tc->tm_sleep = countdown / m_time_div;
		set_thread_state(p_tc, TS_SLEEP);
	}
	m_q_systhread->unlock(hm);
	m_p_cpu->enable_interrupts(irqs);
	m_p_cpu->switch_context();
}

_cthread_t *cBlender::get_thread(_u32 id) {
	_cthread_t *r = 0;
	
	if(m_is_init) {
		if(id != m_idle_id) {
			bool irqs = m_p_cpu->enable_interrupts(false);
			HMUTEX hmq = m_q_systhread->lock();
			_u32 i = 0;
			_u32 c = m_q_systhread->cnt(hmq);
			_cthread_t *p = (_cthread_t *)m_q_systhread->first(hmq);
			if(p) {
				do {
					if(p->id == id) {
						r = p;
						break;
					}
					p = (_cthread_t *)m_q_systhread->next(hmq);
					i++;
				}while(i < c && p);
			}

			m_q_systhread->unlock(hmq);
			m_p_cpu->enable_interrupts(irqs);
		}
	}
	
	return r;
}

_cthread_t *cBlender::get_systhread(_u32 id) {
	return get_thread(id);
}

void cBlender::sleep_systhread(_u32 id, _u32 countdown) {
	_cthread_t *pt = get_thread(id);
	if(pt) {
		pt->tm_sleep = countdown / m_time_div;
		set_thread_state(pt, TS_SLEEP);
	}
}

void cBlender::suspend_systhread(_u32 id) {
	_cthread_t *pt = get_thread(id);
	if(pt) 
		set_thread_state(pt, TS_SUSPEND);
}

void cBlender::resume_systhread(_u32 id) {
	_cthread_t *pt = get_thread(id);
	if(pt)
		pt->flags &= ~TS_SUSPEND;
}

_u32 cBlender::current_systhread_id(void) {
	_u32 r = 0;
	
	_cthread_t *p = (_cthread_t *)m_q_systhread->first();
	if(p)
		r = p->id;
	
	return r;
}

void cBlender::set_state_callback(_u32 id, _tstate_t *p_tscb, void *p_cb_data) {
	_cthread_t *pt = get_thread(id);
	if(pt) {
		pt->p_tscb_data = p_cb_data;
		pt->p_tstate_cb = p_tscb;
	}
}

_u32 cBlender::wait_event(iEvent *p_event, _u32 mask, _u32 timeout) {
	_u32 r = 0;
	_u32 tmo = timeout;

	if(m_p_cpu && p_event) {
		do {
			bool irqs = m_p_cpu->enable_interrupts(false);
			r = p_event->check(mask);
			m_p_cpu->enable_interrupts(irqs);
			if(tmo != EVENT_TIMEOUT_INFINITE)
				tmo--;
			if(!r && irqs)
				m_p_cpu->switch_context();
		} while(!r && tmo);
	}

	return r;
}

HMUTEX cBlender::acquire_mutex(iMutex *p_mutex, HMUTEX hlock, _u32 timeout) {
	HMUTEX r = 0;
	_u32 tmo = timeout;

	if(m_p_cpu && p_mutex) {
		do {
			bool irqs = m_p_cpu->enable_interrupts(false);
			r = p_mutex->try_lock(hlock);
			m_p_cpu->enable_interrupts(irqs);
			if(tmo != MUTEX_TIMEOUT_INFINITE)
				tmo--;
			if(!r && irqs)
				m_p_cpu->switch_context();
		}while(!r && tmo);
	}

	return r;
}

//////////////////////////////////////////////////////////////////////////////////////////

