#ifndef __LAPIC_H__
#define __LAPIC_H__

#include "iLocalApic.h"
#include "iSyncObject.h"
#include "iSystemLog.h"
#include "msr.h"
#include "addr.h"
#include "str.h"

// Local apic register offset (in bytes)
#define LAPIC_ID		0x20
#define LAPIC_VERSION		0x30
#define LAPIC_TPR		0x80 // task priority
#define LAPIC_APR		0x90 // arbitration priority
#define LAPIC_PPR		0xa0 // processor priority
#define LAPIC_EOI		0xb0 // End of interrupt
#define LAPIC_RRR		0xc0 // remote read
#define LAPIC_LDR		0xd0 // logical destination register
#define LAPIC_DFR		0xe0 // destination format register
#define LAPIC_SRI		0xf0 // spurious interrupt
#define LAPIC_ESR		0x280 // error status register
#define LAPIC_LVT_CMCI		0x2f0
#define LAPIC_ICR_LO		0x300 // interrupt command 0-31
#define LAPIC_ICR_HI		0x310 // interrupt command 32-63
#define LAPIC_LVT_TIMER	0x320
#define LAPIC_LVT_TS		0x330 // thermal sensor
#define LAPIC_LVT_LINT0	0x350
#define LAPIC_LVT_LINT1	0x360
#define LAPIC_LVT_ERROR	0x370
#define LAPIC_ITC		0x380 // initial timer count
#define LAPIC_CTC		0x390 // current timer count
#define LAPIC_DIVIDE_TIMER	0x3e0
#define LAPIC_EX_FEATURES	0x400
#define LAPIC_EX_CONTROL	0x410

// ICR delivery modes
#define ICR_DMODE_FIXED	0x00
#define ICR_DMODE_LOW_PRI	0x01
#define ICR_DMODE_SMI		0x02
#define ICR_DMODE_NMI		0x04
#define ICR_DMODE_INIT		0x05
#define ICR_DMODE_STARTUP	0x06
#define ICR_DMODE_EXTINT	0x07
// ICR delivery status
#define ICR_DSTATE_IDLE	0x00
#define ICR_DSTATE_PENDING	0x01
// ICR destination modes
#define ICR_DST_MODE_PHYS	0x00
#define ICR_DST_MODE_LOGIC	0x01
// destination models
#define DFR_MODEL_FLAT		0x0f
#define DFR_MODEL_CLUSTER	0x00
// ICR level
#define ICR_LEVEL_DEASSERT	0x00
#define ICR_LEVEL_ASSERT	0x01
// ICR shorthand
#define ICR_SHORTHAND_NONE	0x00
#define ICR_SHORTHAND_SELF	0x01
#define ICR_SHORTHAND_ALL_IN	0x02
#define ICR_SHORTHAND_ALL_EX	0x03
// ICR trigger mode
#define ICR_TRIGGER_EDGE	0x00
#define ICR_TRIGGER_LEVEL	0x01
// RDCR (Timer divide) values
#define TDCR_DIV2		0x00
#define TDCR_DIV4		0x01
#define TDCR_DIV8		0x02
#define TDCR_DIV16		0x03
#define TDCR_DIV32		0x08
#define TDCR_DIV64		0x09
#define TDCR_DIV128		0x0a
#define TDCR_DIV1		0x0b

union _lapic_register_t {
	_u32 value;
	struct {// Local APIC ID
		_u32 			:24;	// reserved
		_u8	lapic_id;		// local apic ID
	}__attribute__ ((packed));
	struct { // version
		_u8	lapic_version;		// version
		_u32			:8;	// reserved
		_u8	max_lvt_entry;		// Max. LVT entry
		_u32			:8; 	// reserved
	}__attribute__ ((packed));
	struct { // spurious interrupt vector
		_u8	sri_vector;
		_u32	lapic_enable	:1;  	// enable/disable bit
		_u32	focus_checking	:1;  	// focus processor checking
		_u32			:22; 	// reserved
	}__attribute__ ((packed));
	struct {// timer divide configuration register
		_u32	tdcr_divider	:4;  	// divider
		_u32			:28; 	// reserved
	}__attribute__ ((packed));
	struct { // LVT error
		_u8	lvt_err_vector;
		_u32			:4;	// reserved
		_u32	lvt_err_delivs	:1;	// delivery sattus (read only)
		_u32			:3;	// reserved
		_u32	lvt_err_masked	:1;	// 0:not masked / 1:masked
		_u32			:15;	// reserved
	}__attribute__ ((packed));
	struct { // LVT lint
		_u8	lvt_lint_vector;
		_u32	lvt_lint_dmode	:3;	// delivery mode
		_u32			:1;	// reserved
		_u32	lvt_lint_dstate	:1;	// delivery status (read only)
		_u32	lvt_lint_intpp	:1;	// Interrupt input pin polarity
		_u32	lvt_lint_irr	:1;	// remote IRR (read only)
		_u32	lvt_lint_tm	:1;	// trigger mode
		_u32	lvt_lint_masked	:1;	// 0:not masked / 1:masked
 		_u32			:15;	// reserved
	}__attribute__ ((packed));
	struct { // LVT timer register
		_u8	lvt_tm_vector;		// timer vector
		_u32			:4;	// reserved
		_u32	lvt_tm_dstate	:1;	// delivery status(read only)
		_u32			:3;	// reserved
		_u32	lvt_tm_masked	:1;	// interrupt mask
		_u32	lvt_tm_mode	:1;	// timer mode (0:one-shot / 1:periodic)
		_u32			:14;	// reserved
	}__attribute__ ((packed));
	struct { // Task Priority Register
		_u32	tpr_sub_class	:4;	// task priority sub-class
		_u32	tpr_priority	:4;	// task priority
		_u32			:24;	// reserved
	}__attribute__ ((packed));
	struct { // Interrupt command register LO
		_u8	icr_vector;
		_u32	icr_del_mode	:3;	// delivery mode
		_u32	icr_dst_mode	:1;	// destination mode
		_u32	icr_del_state	:1;	// delivery status
		_u32			:1;	// reserved
		_u32	icr_level	:1;	// 0:de-assert/ 1:assert
		_u32	icr_trigger_mode:1;	// 0:edge / 1:level
		_u32			:2;	// reserved
		_u32	icr_shorthand	:2;	// destination shorthand
		_u32			:12;	// reserved
	}__attribute__ ((packed));
	struct { // Interrupt Command Register HI
		_u32			:24;	// reserved
		_u8	icr_dst_field;		// destination field
	}__attribute__ ((packed));
	struct { // local destination register (LDR)
		_u32			:24;	// reserved
		_u8	ldr_apic_id;		// local apic id
	}__attribute__ ((packed));
	struct { // destination format register (DFR)
		_u32			:28;	// reserved
		_u32	dfr_model	:4;	// destination model
	}__attribute__ ((packed));
	struct { // error status register (ESR)
		_u32	esr_send_chrcksum	:1;
		_u32	esr_receive_checksum	:1;
		_u32	esr_send_accept		:1;
		_u32	esr_receive_accept	:1;
		_u32				:1;
		_u32	esr_send_illegal_vector	:1;
		_u32	esr_receive_illegal_vector :1;
		_u32	esr_illegal_register	:1;
		_u32				:24;
	}__attribute__ ((packed));

	_lapic_register_t() 				{ value = 0; } // default constructor
	_OPTIMIZE_SIZE_ _lapic_register_t(_u32  *ptr)	{ value = *ptr; }
	void _OPTIMIZE_SIZE_ get(_u32 *ptr) 		{ value = *ptr; }
	void _OPTIMIZE_SIZE_ set(_u32 *ptr) 		{ *ptr = value; }
};

// read LAPIC base32
#define LAPIC_BASE()	((_u32 *)p_to_k((msr_read(MSR_APIC_BASE) & 0x00000000fffff000)));
// make register pointer
#define LAPIC_REG_PTR(base, offset) ((_u32 *)((_u32 *)base + (offset >> 2)))

class cLocalApic:public iLocalApic {
public:
	_OPTIMIZE_SIZE_ cLocalApic() {
		DRIVER_TYPE = DTYPE_BUS;
		DRIVER_CLASS = DCLASS_SYS;
		str_cpy((_str_t)DRIVER_IDENT, (_str_t)"local apic", sizeof(DRIVER_IDENT));
		m_base = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	//virtual ~cLocalApic() {
	//	CALL_DESTROY();
	//}

	void init(void);
	void enable(void);
	void disable(void);
	void *get_base(void);
	void set_id(_u8 id);
	_u8 get_id(void);
	_u8 get_version(void);
	void set_sri(_u8 vector);
	void end_of_interrupt(void);
	void set_timer(_u8 vector, _u32 countdown, _u8 mode=APIC_TIMER_PERIODIC);
	_u32 get_timer_count(void);
	bool send_init_ipi(_u16 cpu_id, _u32 startup_vector);
	bool send_ipi(_u16 cpu_id, _u8 vector);

	// iBase
	DECLARE_BASE(cLocalApic);
	DECLARE_INIT();
	DECLARE_DESTROY();

	DECLARE_DRIVER_INIT();

protected:
	_u32		*m_p_reg_base;
	_u8		m_id;	// local apic id
	_u64		m_base;

	bool error_code(void);
	void driver_uninit(void) {}
};

#endif

