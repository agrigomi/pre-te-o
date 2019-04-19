#include "ioapic.h"
#include "startup_context.h"
#include "msr.h"
#include "addr.h"
#include "cpu_host.h"
#include "io_port.h"

IMPLEMENT_BASE(cIOApic, "cIOApic", 0x0001);
IMPLEMENT_INIT(cIOApic, p_repository) {
	bool r = false;

	REPOSITORY = p_repository;

	INIT_MUTEX();

	if((m_p_mpf = mp_find_table())) {
		m_p_mpc_ioapic = mp_get_ioapic(m_p_mpf);
	}

	// disable 8259 (PIC)
	outportb(0x21, 0xff);
	outportb(0xa1, 0xff);

	r = true;
	REGISTER_DRIVER();

	return r;
}

IMPLEMENT_DESTROY(cIOApic) {
	RELEASE_MUTEX();
}

IMPLEMENT_DRIVER_INIT(cIOApic, host) {
	bool r = false;

	if(!(DRIVER_HOST = host)) // attach to common driver host
		DRIVER_HOST = GET_DRIVER_ROOT();

	if(DRIVER_HOST)
		r = true;
	return r;
}

void *_OPTIMIZE_SIZE_ cIOApic::get_base(void) {
	void *r = 0;
	if(m_p_mpc_ioapic)
		r = (void *)p_to_k((_u64)m_p_mpc_ioapic->ptr);
	else
		r = (void *)p_to_k((_u64)DEFAULT_IO_APIC_ADDR);

	return r;
}

_u8 _OPTIMIZE_SIZE_ cIOApic::get_id(void) {
	_u8 r = 0;

	if(m_p_mpc_ioapic)
		r = m_p_mpc_ioapic->id;
	else {
		_ioapic_register_t reg;
		reg.value = read(IOAPICID);
		r = reg.apic_id;
	}

	return r;
}

_u8 _OPTIMIZE_SIZE_ cIOApic::get_version(void) {
	_u8 r = 0;
	if(m_p_mpc_ioapic)
		r = m_p_mpc_ioapic->version;
	else {
		//???
	}

	return r;
}

_u8 _OPTIMIZE_SIZE_ cIOApic::get_flags(void) {
	_u8 r = 1;
	if(m_p_mpc_ioapic)
		r = m_p_mpc_ioapic->flags;
	return r;
}

_u32 _OPTIMIZE_SIZE_ cIOApic::read(_u8 addr) {
	_ioapic_register_t reg;
	_u32 *base = (_u32 *)get_base();
	_u32 r = 0;

	if(base) {
		reg.value = base[IOREGSEL];
		reg.reg_addr = addr;
		base[IOREGSEL] = reg.value;
		r = base[IOWIN];
	}

	return r;
}

void _OPTIMIZE_SIZE_ cIOApic::write(_u8 addr, _u32 value) {
	_ioapic_register_t reg;
	_u32 *base = (_u32 *)get_base();
	if(base) {
		reg.value = base[IOREGSEL];
		reg.reg_addr = addr;
		base[IOREGSEL] = reg.value;
		base[IOWIN] = value;
	}
}

void _OPTIMIZE_SIZE_ cIOApic::set_irq(_u8 irq, _u16 dst, _u8 vector, _u32 flags) {
	_ioapic_register_t reg_lo;
	_ioapic_register_t reg_hi;

	reg_lo.value = read(IOREDTBL + (irq << 1));
	reg_hi.value = read(IOREDTBL + (irq << 1) + 1);

	reg_hi.destination      = (_u8)dst;
	reg_lo.vector           = vector;
	reg_lo.delivery_mode    = (flags & IRQF_LO_PRIORITY)   ? 1 : 0;
	reg_lo.destination_mode = (flags & IRQF_DST_LOGICAL)   ? 1 : 0;
	reg_lo.polarity         = (flags & IRQF_POLARITY_LO)   ? 1 : 0;
	reg_lo.trigger_mode     = (flags & IRQF_TRIGGER_LEVEL) ? 1 : 0;
	reg_lo.masked           = (flags & IRQF_MASKED)        ? 1 : 0;

	write(IOREDTBL + (irq << 1), reg_lo.value);
	write(IOREDTBL + (irq << 1) + 1, reg_hi.value);
}

