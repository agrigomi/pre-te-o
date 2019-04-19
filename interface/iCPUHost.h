#ifndef __I_CPU__
#define __I_CPU__

#include "iDriver.h"
#include "iMemory.h"
#include "iScheduler.h"

#define I_CPU_HOST	"iCPUHost"
#define I_CPU		"iCPU"

// privilage level
#define DPL_SYS	0
#define DPL_USR	3

// CPU creation flags
#define CCPUF_ENABLED	(1<<0)
#define CCPUF_BOOT	(1<<1)
#define CCPUF_CORE	(1<<2)

typedef struct {
	_u32 	signature;
	_u16	id;
	_u8	flags;	
}_cpu_init_info_t;

class iCPU:public iBase {
public:
	DECLARE_INTERFACE(I_CPU, iCPU, iBase);

	virtual bool is_boot(void)=0; // return true if object is a boot processor
	virtual _u16 cpu_id(void)=0; // return CPU number
	virtual _u16 ccpu_id(void)=0; // return CPU number for current context
	virtual iVMM *get_vmm(void)=0; // return pointer to VMM manager
	virtual iBlender *get_blender(void)=0;
	virtual void set_blender(iBlender *p_blender)=0;
	virtual bool is_init(void)=0; // return true if CPU is completely initialized
	// send SMP wakeup to another CPU (used by CPUHost for SMP initialization)
	virtual bool send_init_ipi(_u16 cpu_id, _u32 startup_vector)=0;
	virtual _u32 context_size(void)=0; // return the size of CPU state buffer
	virtual void halt(void)=0;
	// create new executable context in 'p_context' buffer
	virtual void create_context(void *p_stack, _u32 stack_sz, _thread_t *p_entry, 
					_u8 dpl, void *p_data,void *p_context)=0;

	// generate context switch interrupt
	virtual void switch_context(void)=0;
	
	virtual void dump(void *state)=0;
	
	///// context dependent calls ////
	
	virtual void set_timer(_u32 countdown)=0;
	virtual _u32 get_timer(void)=0;
	virtual _u64 timestamp(void)=0; // read CPU timestamp counter
	virtual bool enable_interrupts(bool enable)=0; // return prev. irq state
	virtual void enable_cache(bool enable)=0;
	virtual void idle(void)=0; // IDLE loop, can start scheduler (if available)
	virtual void copy_cpu_state(void *dst, void *src)=0;
	// get fpu state in 'p_target_store' buffer if available
	// else store fpu state in to internal buffer
	virtual void save_fpu_state(void *p_target_store=0)=0;
	// set fpu state from 'p_fpu_state' buffer if available
	// else set fpu state from internal buffer
	virtual void restore_fpu_state(void *p_fpu_state=0)=0;

	//// internal callback functions ////

	virtual void init(_cpu_init_info_t *p_cpu_info)=0; // should be called by CPUHost in CPU enumeration step
	virtual void start(void)=0; // should be called by CPUHost in SMP init step
	// send interrupt to another context or receive interrupt from current context
	virtual void interrupt(_u8 in, void *p_cpu_state=0)=0;
	// called by CPUHost after end of interrupt service routine
	virtual void end_of_interrupt(void)=0;
	///////////////////////////////////////
};

// IRQ flags
#define IRQF_DST_LOGICAL	(1<<0)	// IRQ should be received by more than one CPU
#define IRQF_POLARITY_LO	(1<<1)  // input pin polarity
#define IRQF_MASKED		(1<<2)  // mask interrupt signal (disable)
#define IRQF_TRIGGER_LEVEL	(1<<3)  
#define IRQF_LO_PRIORITY	(1<<4)

// protorype of interrupt callback
typedef void _int_callback_t(iCPU *p_cpu, void *p_cpu_state, void *p_data);


class iCPUHost:public iDriverBase {
public:
	DECLARE_INTERFACE(I_CPU_HOST, iCPUHost, iDriverBase);
	
	virtual iCPU *create_cpu(_u32 signature, _u16 id, _u8 flags)=0; //create instance of physical or virtual CPU
	virtual iCPU *get_cpu(_u16 id)=0; // return interface to spec. CPU (id)
	virtual iCPU *get_cpu_by_index(_u16 idx)=0;
	virtual iCPU *get_cpu(void)=0; // return interface to current CPU
	virtual iCPU *get_boot_cpu(void)=0; // return interface to BOOT CPU
	virtual _u16 get_cpu_count(void)=0;
	virtual _int_callback_t *get_isr(_u8 num)=0; // get pointer to interrupt routine
	virtual void *get_isr_data(_u8 num)=0;
	virtual void set_isr(_u8 num, _int_callback_t *p_isr, void *p_data=0)=0; // set interrupt handler
	virtual bool set_irq(_u8 irq, _u16 dst, _u8 vector, _u32 flags=0)=0; // set irq handler
	virtual void start(void)=0; // start CPU initialization
	virtual bool is_running(void)=0;
};


#endif

