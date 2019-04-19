#ifndef __I_LOCAL_APIC_H__
#define __I_LOCAL_APIC_H__

#include "iDriver.h"
#include "arch_ifclist.h"

#define APIC_TIMER_ONESHOT	0
#define APIC_TIMER_PERIODIC	1

class iLocalApic:public iDriverBase {
public:

	DECLARE_INTERFACE(I_LOCAL_APIC, iLocalApic, iDriverBase);
	
	virtual void init(void)=0; // last step of initialization
	virtual void enable(void)=0;
	virtual void disable(void)=0;
	virtual void *get_base(void)=0; // retrn APIC base address
	virtual _u8 get_id(void)=0;
	virtual void set_id(_u8 id)=0;
	virtual _u8 get_version(void)=0;
	virtual void set_sri(_u8 vector)=0; // set spurious interrupt vector
	virtual void set_timer(_u8 vector, _u32 countdown, _u8 mode=APIC_TIMER_PERIODIC)=0;
	virtual _u32 get_timer_count(void)=0;
	virtual void end_of_interrupt(void)=0;
	virtual bool send_init_ipi(_u16 cpu_id, _u32 startup_vector)=0; // setup another CPU
	virtual bool send_ipi(_u16 cpu_id, _u8 vector)=0;
};

#endif

