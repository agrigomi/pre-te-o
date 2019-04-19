#ifndef __IO_APIC_H__
#define __IO_APIC_H__

#include "iIOApic.h"
#include "iSystemLog.h"
#include "mp_scan.h"
#include "str.h"

#define DEFAULT_IO_APIC_ADDR	0xFEC00000

#define IOREGSEL  0x00
#define IOWIN     (0x10U / sizeof(_u32))

#define IOAPICID   0x00U
#define IOAPICVER  0x01U
#define IOAPICARB  0x02U
#define IOREDTBL   0x10U

union _ioapic_register_t {
	_u32	value;
	struct {
		_u8	reg_addr;   		// APIC Register Address.
		_u32 			: 24;  // Reserved.
	}__attribute__ ((packed));
	struct { // I/O Redirection Register. (LO)
		_u8 	vector;               // Interrupt Vector.
		_u32	delivery_mode	: 3;  // Delivery Mode.
		_u32	destination_mode: 1;  // Destination mode.
		_u32	delivery_status	: 1;  // Delivery status (RO).
		_u32	polarity	: 1;  // Interrupt Input Pin Polarity.
		_u32	irr 		: 1;  // Remote IRR (RO).
		_u32	trigger_mode 	: 1;  // Trigger Mode.
		_u32	masked 		: 1;  // Interrupt Mask.
		_u32			: 15; // Reserved.
	} __attribute__ ((packed));
	struct { // I/O Redirection Register. (HI)
		_u32			: 24; // Reserved.
		_u8	destination	: 8;  // Destination Field.
	} __attribute__ ((packed));
	struct { // ID register
		_u32			 : 24; // reserved
		_u32	 apic_id	 : 4;  // IO APIC ID.
		_u32			 : 4; // Reserved.
	} __attribute__ ((packed));
} __attribute__ ((packed));

class cIOApic:public iIOApic {
public:
	cIOApic() {
		m_p_mpc_ioapic = 0;
		m_p_mpf = 0;
		DRIVER_TYPE = DTYPE_BUS;
		DRIVER_CLASS = DCLASS_SYS;
		str_cpy((_str_t)_d_ident_, (_str_t)"i/o apic", sizeof(_d_ident_));
		ERROR_CODE = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}

	//virtual ~cIOApic() {}

	void *get_base(void);
	_u8 get_id(void);
	_u8 get_version(void);
	_u8 get_flags(void);
	void set_irq(_u8 irq, _u16 dst, _u8 vector, _u32 flags=0);

	// iDriverBase
	DECLARE_DRIVER_INIT();

	// iBase
	DECLARE_BASE(cIOApic)
	DECLARE_INIT();
	DECLARE_DESTROY();
	void driver_uninit(void) {}

protected:
	_mpc_ioapic_t	*m_p_mpc_ioapic;
	_mp_t 		*m_p_mpf;

	_u32 read(_u8 addr);
	void write(_u8 addr, _u32 value);
};

#endif

