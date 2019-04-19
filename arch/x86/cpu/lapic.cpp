#include "iIOApic.h"
#include "lapic.h"
#include "startup_context.h"
#include "cpu_host.h"

//#define _DEBUG_
#include "debug.h"

// offset to register pointer conversion
#define REG_PTR(offset)	LAPIC_REG_PTR(m_p_reg_base, offset)

IMPLEMENT_BASE(cLocalApic, "cLocalApic", 0x0001)
IMPLEMENT_INIT(cLocalApic, p_rb) {
	bool r = false;

	REPOSITORY = p_rb;

	INIT_MUTEX();

	m_p_reg_base = LAPIC_BASE();
	REGISTER_DRIVER();
	r = true;

	return r;
}
IMPLEMENT_DESTROY(cLocalApic) {
	RELEASE_MUTEX();
}

IMPLEMENT_DRIVER_INIT(cLocalApic, host) {
	bool r = false;

	if(!(DRIVER_HOST = host)) {
		_driver_request_t dr;
		dr.flags = DRF_INTERFACE;
		dr._d_interface_ = I_IO_APIC;

		iDriverRoot *p_dh = GET_DRIVER_ROOT();
		if(p_dh) {
			DRIVER_HOST = p_dh->driver_request(&dr);
			if(!DRIVER_HOST)
				DRIVER_HOST = (iDriverBase *)OBJECT_BY_INTERFACE(I_IO_APIC, RY_STANDALONE);
			RELEASE_OBJECT(p_dh);
		}
	}

	if(DRIVER_HOST)
		r = true;

	return r;
}

bool _OPTIMIZE_SIZE_ cLocalApic::error_code(void) {
	_lapic_register_t err(REG_PTR(LAPIC_ESR));
	ERROR_CODE = err.value;
	return (err.value & 0x000000ef) ? false : true;
}

void _OPTIMIZE_SIZE_ cLocalApic::init(void) {
	_msr_apic_base_t msr;

	DBG("LAPIC: init start\r\n","");
	_lapic_register_t reg;

	reg.get(REG_PTR(LAPIC_ID));
	m_id = reg.lapic_id;

	// init LVT error
	DBG("LAPIC: init LVT err\r\n","");
	reg.get(REG_PTR(LAPIC_LVT_ERROR));
	reg.lvt_err_masked = 1;
	reg.set(REG_PTR(LAPIC_LVT_ERROR));

	// init LVT LINT0
	DBG("LAPIC: init LVT LINT0\r\n","");
	reg.get(REG_PTR(LAPIC_LVT_LINT0));
	reg.lvt_lint_masked = 1;
	reg.set(REG_PTR(LAPIC_LVT_LINT0));

	// init LVT LINT1
	DBG("LAPIC: init LVT LINT1\r\n","");
	reg.get(REG_PTR(LAPIC_LVT_LINT1));
	reg.lvt_lint_masked = 1;
	reg.set(REG_PTR(LAPIC_LVT_LINT1));

	// init Task Priority
	DBG("LAPIC: init Task Priority\r\n","");
	reg.get(REG_PTR(LAPIC_TPR));
	reg.tpr_sub_class = 0;
	reg.tpr_priority = 0;
	reg.set(REG_PTR(LAPIC_TPR));

	// init spurious interrupt
	DBG("LAPIC: init SIV\r\n","");
	reg.get(REG_PTR(LAPIC_SRI));
	reg.focus_checking = 1;
	reg.lapic_enable = 0;
	reg.set(REG_PTR(LAPIC_SRI));

	// init command register
	DBG("LAPIC: init command register\r\n", "");
	reg.get(REG_PTR(LAPIC_ICR_LO));
	reg.icr_del_mode = ICR_DMODE_INIT;
	reg.icr_dst_mode = ICR_DST_MODE_PHYS;
	reg.icr_level = ICR_LEVEL_DEASSERT;
	reg.icr_trigger_mode = ICR_TRIGGER_LEVEL;
	reg.icr_shorthand = ICR_SHORTHAND_ALL_IN;
	reg.set(REG_PTR(LAPIC_ICR_LO));

	// init timer divide configuration register
	DBG("LAPIC: init timer divide configuration register\r\n", "");
	reg.get(REG_PTR(LAPIC_DIVIDE_TIMER));
	reg.tdcr_divider = TDCR_DIV1;
	reg.set(REG_PTR(LAPIC_DIVIDE_TIMER));

	// init LVT timer
	DBG("LAPIC: init LVT timer\r\n","");
	reg.get(REG_PTR(LAPIC_LVT_TIMER));
	reg.lvt_tm_vector = 0;
	reg.lvt_tm_mode = APIC_TIMER_PERIODIC;
	reg.lvt_tm_masked = 0;
	reg.set(REG_PTR(LAPIC_LVT_TIMER));

	reg.get(REG_PTR(LAPIC_CTC));
	reg.value = 0;
	reg.set(REG_PTR(LAPIC_ITC));

	// local destination register (LDR)
	DBG("LAPIC: init LDR\r\n","");
	reg.get(REG_PTR(LAPIC_LDR));
	reg.ldr_apic_id = m_id;
	reg.set(REG_PTR(LAPIC_LDR));

	// destination format register
	DBG("LAPIC: init DFR\r\n","");
	reg.get(REG_PTR(LAPIC_DFR));
	reg.dfr_model = DFR_MODEL_FLAT;
	reg.set(REG_PTR(LAPIC_DFR));

	DBG("LAPIC: init done\r\n","");
}

void _OPTIMIZE_SIZE_ cLocalApic::set_sri(_u8 vector) {
	_lapic_register_t reg(REG_PTR(LAPIC_SRI));
	reg.sri_vector = vector;
	reg.set(REG_PTR(LAPIC_SRI));
}

void _OPTIMIZE_SIZE_ cLocalApic::enable(void) {
	_lapic_register_t reg(REG_PTR(LAPIC_SRI));
	reg.lapic_enable = 1;
	reg.set(REG_PTR(LAPIC_SRI));
}

void _OPTIMIZE_SIZE_ cLocalApic::disable(void) {
	_lapic_register_t reg(REG_PTR(LAPIC_SRI));
	reg.lapic_enable = 0;
	reg.set(REG_PTR(LAPIC_SRI));
}

void *_OPTIMIZE_SIZE_ cLocalApic::get_base(void) {
	_msr_apic_base_t msr;
	if(!m_base)
		m_base = (msr.apic_base << 12) & 0x00000000fffff000;
	return (void *)p_to_k(m_base);
}

_u8 _OPTIMIZE_SIZE_ cLocalApic::get_id(void) {
	_lapic_register_t reg(REG_PTR(LAPIC_ID));
	return reg.lapic_id;
}

void _OPTIMIZE_SIZE_ cLocalApic::set_id(_u8 id) {
	_lapic_register_t reg(REG_PTR(LAPIC_ID));
	reg.lapic_id = id;
	reg.set(REG_PTR(LAPIC_ID));
}

_u8 _OPTIMIZE_SIZE_ cLocalApic::get_version(void) {
	_lapic_register_t reg(REG_PTR(LAPIC_VERSION));
	return reg.lapic_version;
}

void _OPTIMIZE_ALL_ cLocalApic::end_of_interrupt(void) {
	_lapic_register_t reg;
	reg.value = 0;
	reg.set(REG_PTR(LAPIC_EOI));
}

void _OPTIMIZE_SIZE_ cLocalApic::set_timer(_u8 vector, _u32 countdown, _u8 mode) {
	_lapic_register_t reg(REG_PTR(LAPIC_LVT_TIMER));

	DBG("LAPIC: set timer(vector=0x%x, countdown=0x%x, mode=%d)\r\n", vector, countdown,mode);

	reg.lvt_tm_vector = vector;
	reg.lvt_tm_masked = 0;
	reg.lvt_tm_mode = mode;
	reg.set(REG_PTR(LAPIC_LVT_TIMER));

	reg.value = countdown;
	reg.set(REG_PTR(LAPIC_ITC));
}

_u32 _OPTIMIZE_ALL_ cLocalApic::get_timer_count(void) {
	_lapic_register_t reg(REG_PTR(LAPIC_CTC));
	return reg.value;
}

bool _OPTIMIZE_NONE_ cLocalApic::send_init_ipi(_u16 cpu_id, _u32 startup_vector) {
	_lapic_register_t icr_lo(REG_PTR(LAPIC_ICR_LO));
	_lapic_register_t icr_hi(REG_PTR(LAPIC_ICR_HI));

	icr_lo.icr_del_mode = ICR_DMODE_INIT;
	icr_lo.icr_dst_mode = ICR_DST_MODE_PHYS;
	icr_lo.icr_level = ICR_LEVEL_ASSERT;
	icr_lo.icr_trigger_mode = ICR_TRIGGER_LEVEL;
	icr_lo.icr_shorthand = ICR_SHORTHAND_NONE;
	icr_lo.icr_vector = 0;

	icr_hi.icr_dst_field = (_u8)cpu_id;

	icr_hi.set(REG_PTR(LAPIC_ICR_HI));
	icr_lo.set(REG_PTR(LAPIC_ICR_LO));

	_u32 tmout=10000;
	while(tmout--);

	_lapic_register_t reg_ver(REG_PTR(LAPIC_VERSION));
	if((reg_ver.lapic_version & 0xf0) != 0x00) {
		for(_u32 i = 0; i < 2; i++) {
			_lapic_register_t icr_lo(REG_PTR(LAPIC_ICR_LO));

			icr_lo.icr_vector = startup_vector >> 12;
			icr_lo.icr_del_mode = ICR_DMODE_STARTUP;
			icr_lo.icr_dst_mode = ICR_DST_MODE_PHYS;
			icr_lo.icr_level = ICR_LEVEL_ASSERT;
			icr_lo.icr_trigger_mode = ICR_TRIGGER_LEVEL;
			icr_lo.icr_shorthand = ICR_SHORTHAND_NONE;

			icr_lo.set(REG_PTR(LAPIC_ICR_LO));

			tmout = 200;
			while(tmout--);
		}
	}

	return error_code();
}

bool _OPTIMIZE_NONE_ cLocalApic::send_ipi(_u16 cpu_id, _u8 vector) {
	_lapic_register_t icr_lo(REG_PTR(LAPIC_ICR_LO));
	_lapic_register_t icr_hi(REG_PTR(LAPIC_ICR_HI));

	icr_lo.icr_del_mode = ICR_DMODE_FIXED;
	icr_lo.icr_dst_mode = ICR_DST_MODE_PHYS;
	icr_lo.icr_level = ICR_LEVEL_ASSERT;
	icr_lo.icr_trigger_mode = ICR_TRIGGER_LEVEL;
	icr_lo.icr_shorthand = ICR_SHORTHAND_NONE;
	icr_lo.icr_vector = vector;

	icr_hi.icr_dst_field = (_u8)cpu_id;

	icr_hi.set(REG_PTR(LAPIC_ICR_HI));
	icr_lo.set(REG_PTR(LAPIC_ICR_LO));

	return error_code();
}
