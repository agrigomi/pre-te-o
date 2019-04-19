#ifndef __I_SCHEDULER__
#define __I_SCHEDULER__

#define I_BLENDER	"iBlender"

#include "iBase.h"
#include "iSyncObject.h"

#define MAX_CPU_STATE	512

// thread state
#define TS_RUNNING		(1<<0)
#define TS_PENDING		(1<<1)
#define TS_SUSPEND		(1<<2)
#define TS_FINISHED		(1<<3)
#define TS_SLEEP		(1<<4)
#define TS_EXCEPTION		(1<<5)
#define TS_BREAKPOINT		(1<<6)
#define TS_DEBUG		(1<<7)
#define TS_MEMORY_EXCEPTION	(1<<8)

#define MIN_TIME_DIV		1
#define MAX_TIME_DIV		10
#define DEFAULT_TIME_DIV	10

class iBlender;
struct _cthread_t;

// thread entry
typedef _s32 _thread_t(void *p_data);

// state callback prototype
typedef void _tstate_t( iBlender *p_blender,
			_cthread_t *p_thread, 
			_u16 nstate, // new state
			void *p_udata
		      );

struct _cthread_t {
	_thread_t	*p_entry;	// thread entry point
	void		*p_data;	// user data
	_tstate_t	*p_tstate_cb;	// state callback
	void		*p_tscb_data;	// state callback user data
	void		*p_stack_base;	// bottom of stack
	_u32		stack_sz;	// stack size  in bytes
	_u32		id;		// thread ID
	_u16		cpu;		// CPU ID
	_u8		dpl;		// thread type (0:system / 3:user)
	_u16		flags;		// thread state
	_u8		exception;	// interrupt number
	_ulong		mf_addr;	// memry fault address
	_u32		resolution;	// automatic sleep with 'resolution' time
	_u64		tm_sched;	// scheduling time
	_u32		tm_sleep;	// sleep countdown (clock time)
	_s32		exit_code;
	_u8		context[MAX_CPU_STATE]; // CPU state buffer
};

class iBlender:public iBase {
public:
	DECLARE_INTERFACE(I_BLENDER, iBlender, iBase);

	virtual void init(_u16 cpu_id)=0;
	virtual void start(void)=0;
	virtual void stop(void)=0;
	virtual bool is_running(void)=0;
	virtual void set_timer(_u32 timer)=0;
	virtual void set_time_div(_u8 div)=0;
	virtual _u8  get_time_div(void)=0;
	virtual bool enable_interrupts(bool enable)=0;

	// IDLE callback
	virtual void idle(void)=0;

	// interrupt callback
	virtual void timer(void *p_cpu_state)=0;
	virtual void switch_context(void *p_cpu_state=0)=0;
	virtual void breakpoint(void *p_cpu_state)=0;
	virtual void debug(void *p_cpu_state)=0;

	// exception callback
	virtual void exception(_u8 exn, void *p_cpu_state)=0;
	virtual void memory_exception(_ulong addr, void *p_cpu_state)=0;

	// system thread management
	virtual _u32 create_systhread(_thread_t *p_entry, _u32 stack_sz, void *p_data, 
					bool suspend=false, _u32 resolution=0)=0;
	virtual _u32 create_usr_systhread(_thread_t *p_entry, void *p_stack, _u32 stack_sz, void *p_data, 
					bool suspend=true, _u32 resolution=0)=0;
	virtual void terminate_systhread(_u32 id)=0;
	virtual void sleep_systhread(_u32 countdown)=0;
	virtual void sleep_systhread(_u32 id, _u32 countdown)=0;
	virtual void suspend_systhread(_u32 id)=0;
	virtual void resume_systhread(_u32 id)=0;
	virtual _cthread_t *get_systhread(_u32 id)=0;
	virtual void set_state_callback(_u32 id, _tstate_t *p_tscb, void *p_cb_data)=0;

	// returns ID of the current thread
	virtual _u32 current_systhread_id(void)=0;

	// sync.
	virtual _u32 wait_event(iEvent *p_event, _u32 mask=EVENT_DEFAULT_MASK, _u32 timeout=EVENT_TIMEOUT_INFINITE)=0;
	virtual HMUTEX acquire_mutex(iMutex *p_mutex, HMUTEX hlock=0, _u32 timeout=MUTEX_TIMEOUT_INFINITE)=0;
};

class iCPU;
class iCPUHost;

iCPUHost *get_cpu_host(void);
_u16 cpu_count(void);
// get current CPU
iCPU *get_cpu(void);
// get CPU(cpu_id)
iCPU *get_cpu(_u16 cpu_id);
iCPU *get_cpu_by_index(_u16 idx);
// get current CPU id
_u16 cpu_id();
// enable/disable IRQ-s
bool enable_interrupts(bool enable);
// get current cpu blender
iBlender *get_blender(void);
// get blender of CPU(cpu_id)
iBlender *get_blender(_u16 cpu_id);
// create system thread on current CPU
_u32 create_systhread(_thread_t *p_entry, _u32 stack_sz, void *p_data, 
			bool suspend=false, _u32 resolution=0);
// create syste thread on CPU(cpu_id)
_u32 create_systhread(_u16 cpu_id, _thread_t *p_entry, _u32 stack_sz, void *p_data, 
			bool suspend=false, _u32 resolution=0);
// sleep current systhread
void sleep_systhread(_u32 tm_ms);
// suspend systhread(thread_id)
void suspend_systhread(_u32 thread_id);
// resume systhread(thread_id)
void resume_systhread(_u32 thread_id);
void switch_context(void);
_u32 wait_event(iEvent *p_event, _u32 mask=EVENT_DEFAULT_MASK, _u32 timeout=EVENT_TIMEOUT_INFINITE);
HMUTEX acquire_mutex(iMutex *p_mutex, HMUTEX hlock=0, _u32 timeout=MUTEX_TIMEOUT_INFINITE);

#endif
