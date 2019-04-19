#ifndef __I_PCI_H__
#define __I_PCI_H__

#include "iDriver.h"

#define I_PCI		"iPCI"

#define MAX_PCI_BUS	 256
#define MAX_PCI_DEVICE	 32    // max. devices per bus
#define MAX_PCI_FUNCTION 8     // max. functions per device

// PCI header type 0
//register	 bits 31-24	 		bits 23-16	 		bits 15-8	 		bits 7-0
//----------------------------------------------------------------------------------------------------------------------------------
//00(00)	 Device ID	 			 			Vendor ID
//01(04)	 Status	 				 			Command
//02(08)	 Class code	 		Subclass	 		Prog IF	 			Revision ID
//03(0C)	 BIST	 	 		Header type	 		Latency Timer	 		Cache Line Size
//04(10)	 Base address #0 (BAR0)
//05(14)	 Base address #1 (BAR1)
//06(18)	 Base address #2 (BAR2)
//07(1C)	 Base address #3 (BAR3)
//08(20)	 Base address #4 (BAR4)
//09(24)	 Base address #5 (BAR5)
//10(28)	 Cardbus CIS Pointer
//11(2C)	 Subsystem ID		 					Subsystem Vendor ID
//12(30)	 Expansion ROM base address
//13(34)	 Reserved	 					 					Capabilities Pointer
//14(38)	 Reserved
//15(3C)	 Max latency	 	 	Min Grant	 		Interrupt PIN	 		Interrupt Line


// PCI header type 1
//register	 bits 31-24	 		bits 23-16	 		bits 15-8	 		bits 7-0
//----------------------------------------------------------------------------------------------------------------------------------
//00(00)	 Device ID	 						Vendor ID
//01(04)	 Status	 							Command
//02(08)	 Class code	 		Subclass			Prog IF	 			Revision ID
//03(0C)	 BIST	 	 		Header type			Latency Timer			Cache Line Size
//04(10)	 Base address #0 (BAR0)
//05(14)	 Base address #1 (BAR1)
//06(18)	 Secondary Latency Timer	Subordinate Bus Number	 	Secondary Bus Number	 	Primary Bus Number
//07(1C)	 Secondary Status	 					I/O Limit	 		I/O Base
//08(20)	 Memory Limit	 						Memory Base
//09(24)	 Prefetchable Memory Limit	 				Prefetchable Memory Base
//10(28)	 Prefetchable Base Upper 32 Bits
//11(2C)	 Prefetchable Limit Upper 32 Bits
//12(30)	 I/O Limit Upper 16 Bits	 				I/O Base Upper 16 Bits
//13(34)	 Reserved	 										Capability Pointer
//14(38)	 Expansion ROM base address
//15(3C)	 Bridge Control	 						Interrupt PIN	 		Interrupt Line

#define PCI_CLASS_UNKNOWN	0	// Devices built before class codes (i.e. pre PCI 2.0)
#define PCI_CLASS_STORAGE	1	// Mass storage controller
#define PCI_CLASS_NETWORK	2	// Network controller
#define PCI_CLASS_DISPLAY	3	// Display controller
#define PCI_CLASS_MMEDIA	4	// Multimedia device
#define PCI_CLASS_MEMORY	5	// Memory Controller
#define PCI_CLASS_BRIDGE	6	// Bridge Device
#define PCI_CLASS_SCC		7	// Simple communications controllers
#define PCI_CLASS_BASE		8	// Base system peripherals
#define PCI_CLASS_IDEV		9	// Inupt devices
#define PCI_CLASS_DSTATIONS	10	// Docking Stations
#define PCI_CLASS_PROCESSOR	11
#define PCI_CLASS_SBUS		12	// Serial Bus
#define PCI_CLASS_WIRELESS_CTRL	0x0D	// Wireless Controllers
#define PCI_CLASS_INTELLI_IO	0x0E	// Intelligent I/O Controllers
#define PCI_CLASS_SAT_CTRL	0x0F	// Satellite Communication Controllers
#define PCI_CLASS_CODEC_CTRL	0x10	// Encryption/Decryption Controllers
#define PCI_CLASS_SIGNAL_CTRL	0x11	// Data Acquisition and Signal Processing Controllers
// 0x12 - 0xFE	 Reserved
#define PCI_CLASS_UNDEFINED	0xFF	// Device does not fit any defined class.

typedef struct {
	union { // 00(00)  vendor / device
		_u32 vd;
		struct {
			_u32 vendor_id	:16;
			_u32 device_id	:16;
		}__attribute__((packed));
	}__attribute__((packed));
	
	union { // 01(04) command / status
		_u32 cs;
		struct {
			_u32 command	:16;
			_u32 status	:16;
		}__attribute__((packed));
	}__attribute__((packed));
	
	union { // 02(08) revision /class code
		_u32 rcc;
		struct {
			_u32 revision_id	:8;
			_u32 prog_if		:8; // Programming Interface Byte
			_u32 subclass		:8;		
			_u32 class_code		:8;
		}__attribute__((packed));
	}__attribute__((packed));
	
	union { // 03(0c) cache line lize / latency timer / header type / BIST
		_u32 clhb;
		struct {
			_u32 cache_line_size	:8;
			_u32 latency_timer	:8;
			_u32 header_type	:8;
			_u32 bist		:8;
		}__attribute__((packed));
	}__attribute__((packed));
	
	// base address registers
	_u32 bar0; // 04(10)
	_u32 bar1; // 05(14)
	
	union { // 06(18)
		_u32 bar2;	// header type 0
		struct {	// header type 1
			_u32 primary_bus_number		:8;
			_u32 secondary_bus_number	:8;
			_u32 subordinate_bus_number	:8;
			_u32 secondary_latency_timer	:8;
		}__attribute((packed));
	}__attribute__((packed));
	
	union { // 07(1c)
		_u32 bar3;	// header type 0
		struct {	// header type 1
			_u32 io_base_lo		:8;
			_u32 io_limit_lo	:8;
			_u32 secondary_status	:16;
		}__attribute__((packed));
	}__attribute__((packed));
	
	union { // 08(20)
		_u32 bar4;	// header type 0
		struct {	// header type 1
			_u32	memory_base	:16;
			_u32	memory_limit	:16;
		}__attribute__((packed));
	}__attribute__((packed));

	union { // 09(24)
		_u32 bar5;	// header type 0
		struct {	// header type 1
			_u32 prefetchable_memory_base	:16;
			_u32 prefetchable_memory_limit	:16;
		}__attribute__((packed));
	}__attribute__((packed));
	
	union { // 10(28)
		_u32 cis; // Cardbus CIS pointer header type 0
		_u32 prefetchable_base_up32;	// header type 1
	}__attribute__((packed));
	
	union { // 11(2c) 
		_u32 svs;
		struct {	// header type 0
			_u32 subsystem_vendor_id	:16;
			_u32 subsystem_id		:16;
		}__attribute__((packed));
		_u32 prefetchable_limit_up32; // header type 1
	}__attribute__((packed));
	
	union { // 12(30)
		_u32 erba_h0;	// header type 0 -- Expansion ROM Base Address
		struct {	// header type 1
			_u32 io_base_hi		:16;
			_u32 io_limit_hi	:16;
		}__attribute__((packed));
	}__attribute__((packed));

	union { // 13(34)
		_u32 cp;
		struct { // header type 0 and 1
			_u32 capabilities_pointer :8;
			_u32 			  :24; // reserved
		}__attribute__((packed));
	}__attribute__((packed));
	
	union { // 14(38)
		_u32 res;	// header type 0 -- reserved
		_u32 erba_h1;	// header type 1 -- Expansion ROM Base Address
	}__attribute__((packed));
	
	union { // 15(3c)
		_u32 iimm;
		struct {	// header type 0
			_u32 interrupt_line	:8;
			_u32 interrupt_pin	:8;
			_u32 min_gnt		:8;
			_u32 max_lat		:8;
		}__attribute__((packed));
		_u32 iibc;
		struct {	// header type 1
			_u32 			:8; // interrupt line
			_u32			:8; // interrupt pin
			_u32 bridge_control	:16;
		}__attribute__((packed));
	}__attribute__((packed));
}__attribute__((packed)) _pci_hdr_t;

class iPCI:public iDriverBase {
public:
	DECLARE_INTERFACE(I_PCI, iPCI, iDriverBase);
	
	virtual _u32 count(void)=0;
	virtual bool device(_u32 index, _pci_hdr_t *p_dev)=0;
	virtual _u32 read(_u32 index, _u8 reg)=0;
	virtual void write(_u32 index, _u8 reg, _u32 value)=0;
};

#endif

