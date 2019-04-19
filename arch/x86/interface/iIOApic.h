#ifndef __I_IO_APIC_H__
#define __I_IO_APIC_H__

#include "iDriver.h"
#include "arch_ifclist.h"


class iIOApic:public iDriverBase {
public:
	DECLARE_INTERFACE(I_IO_APIC, iIOApic, iDriverBase);

	virtual void *get_base(void)=0; // retrn APIC base address
	virtual _u8 get_id(void)=0;
	virtual _u8 get_version(void)=0;
	virtual _u8 get_flags(void)=0;
	virtual void set_irq(_u8 irq, _u16 dst, _u8 vector, _u32 flags=0)=0;
};

#endif

