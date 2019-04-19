#ifndef __UHCI_H__
#define __UHCI_H__

struct __attribute__((packed)) _reg_uhci_t {
	struct {
		_u16	rs	:1;	// Run/Stop (RS). 1=Run. 0=Stop
		_u16	hcreset	:1;	// Host Controller Reset (HCRESET)
		_u16	greset	:1;	// Global Reset (GRESET).
		_u16	egsm	:1;	// Enter Global Suspend Mode (EGSM). 1=Host Controller enters the Global Suspend mode.
		_u16	fgr	:1;	// Force Global Resume (FGR). 1=Host Controller sends the Global Resume signal on the USB
		_u16	swdbg	:1;	// Software Debug (SWDBG). 1=Debug mode. 0=Normal Mode.
		_u16	cf	:1;	// Configure Flag (CF).
		_u16	maxp	:1;	// Max Packet (MAXP). 1=64 bytes. 0=32 bytes.
		_u16		:8;
	}__attribute__((packed)) usbcmd;

	struct {
		_u16	usbint	:1;	// USB Interrupt (USBINT). HC sets this bit to 1 when the cause of an interrupt
		_u16	ei	:1;	// USB Error Interrupt.
		_u16	rd	:1;	// Resume Detect.
		_u16	hse	:1;	// Host System Error
		_u16	hcpe	:1;	// Host Controller Process Error.
		_u16	hch	:1;	// HC Halted.
		_u16		:10;
	}__attribute__((packed)) usbsts;

	struct {
		_u16	tocrc	:1;	// Timeout/CRC Interrupt Enable. 1= Enabled. 0=Disabled.
		_u16	resume	:1;	// Resume Interrupt Enable. 1= Enabled. 0=Disabled.
		_u16	ioc	:1;	// Interrupt On Complete (IOC) Enable. 1= Enabled. 0=Disabled.
		_u16	sp	:1;	// Short Packet Interrupt Enable. 1=Enabled. 0=Disabled.
		_u16		:12;
	}__attribute__((packed)) usbintr;

	struct {
		_u16	fn	:11;	// Frame List Current Index/Frame Number
		_u16		:5;
	}__attribute__((packed)) frnum;

	struct {
		_u32		:12;
		_u32	addr	:20;	// Base Address. 
	}__attribute__((packed)) frbaseaddr;

	struct {
		_u32	sof	:7;	// SOF Timing Value.
		_u32		:25;
	}__attribute__((packed)) sofmod;

	struct {
		_u16	ccs	:1;	// Current Connect StatusRO. 1=Device is present on port. 0=No device is present.
		_u16	csc	:1;	// Connect Status Change R/WC. 1=Change in Current Connect Status. 0=No change
		_u16	pe	:1;	// Port Enabled/DisabledR/W. 1=Enable. 0=Disable.
		_u16	pec	:1;	// Port Enable/Disable Change R/WC. 1=Port enabled/disabled status has changed.
		_u16	ls	:2;	// Line StatusRO.
		_u16	rd	:1;	// Resume DetectR/W. 1= Resume detected/driven on port. 0=No resume
		_u16		:1;
		_u16	lsda	:1;	// Low Speed Device Attached RO. 1=Low speed. 0=Full speed.
		_u16	pr	:1;	// Port Reset R/W. 1=Port is in Reset. 0=Port is not in Reset
		_u16		:2;
		_u16	suspend	:1;	// SuspendR/W. 1=Port in suspend state. 0=Port not in suspend state.
		_u16	zero	:3;
	}__attribute__((packed)) portsc[2];
};

#endif // __UHCI_H__
