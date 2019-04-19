#ifndef __EHCI_H__
#define __EHCI_H__

#define MAX_EHCI_NDP	256

struct __attribute__((packed)) _reg_ehci_t {
	struct { //  USB Command Register
		_u32	rs	:1;	// Run/Stop (RS) R/W. Default 0b. 1=Run. 0=Stop.
		_u32 	hcreset	:1;	// Host Controller Reset (HCRESET)  R/W
		_u32	fls	:2;	// Frame List Size  (R/W or RO). Default 00b
		_u32	pse	:1;	// Periodic Schedule Enable  R/W. Default 0b
		_u32	ase	:1;	// Asynchronous Schedule Enable  R/W
		_u32	iaad	:1;	// Interrupt on Async Advance Doorbell  R/W.
		_u32	lhcr	:1;	// Light Host Controller Reset (OPTIONAL)  R/W.
		_u32	aspmc	:2;	// Asynchronous Schedule Park Mode Count (OPTIONAL)  RO or R/W
		_u32		:1;
		_u32	aspme	:1;	// Asynchronous Schedule Park Mode Enable (OPTIONAL)  RO or R/W
		_u32		:4;
		_u32	itc	:8;	// Interrupt Threshold Control  R/W
		_u32		:8;
	}__attribute__((packed)) usbcmd;

	struct { // USB Status Register
		_u32	uabint	:1;	// USB Interrupt (USBINT)  R/WC
		_u32	errint	:1;	// USB Error Interrupt (USBERRINT)  R/WC
		_u32	pcd	:1;	// Port Change Detect  R/WC
		_u32	flr	:1;	// Frame List Rollover  R/WC
		_u32	hfe	:1;	// Host System Error
		_u32	iaa	:1;	// Interrupt on Async Advance  R/WC
		_u32		:6;
		_u32	hch	:1;	// HCHalted  RO.
		_u32	recl	:1;	// Reclamation  RO
		_u32	pss	:1;	// Periodic Schedule Status  RO. 0=Default
		_u32	ass	:1;	// Asynchronous Schedule Status  RO
		_u32		:16;	
	}__attribute__((packed)) usbsts;

	struct { // USB Interrupt Enable Register
		_u32	ie	:1;	// USB Interrupt Enable.
		_u32	eie	:1;	// USB Error Interrupt Enable.
		_u32	pcie	:1;	// Port Change Interrupt Enable
		_u32	flre	:1;	// Frame List Rollover Enable
		_u32	hsee	:1;	// Host System Error Enable
		_u32	iaae	:1;	// Interrupt on Async Advance Enable
		_u32		:26;
	}__attribute__((packed)) usbintr;

	struct { // Frame Index Register
		_u32	fi	:14;	// Frame Index
		_u32		:18;
	}__attribute__((packed)) frindex;

	_u32 	ctrldssegment;		// Control Data Structure Segment Register

	struct { // Periodic Frame List Base Address Register
		_u32	zero	:12;
		_u32	baddrl	:20;	// Base Address (Low)
	}__attribute__((packed)) periodiclistbase;

	struct { // Current Asynchronous List Address Register
		_u32		:5;
		_u32	lpl	:27;	// Link Pointer Low (LPL).
	}__attribute__((packed)) asynclistaddr;

	struct { // Configure Flag Register
		_u32	cf	:1;	// Configure Flag (CF) R/W
		_u32		:31;
	}__attribute__((packed)) configflag;

	struct { // Port Status and Control Register
		_u32	ccs	:1;	// Current Connect Status RO. 1=Device is present. 0=No device
		_u32	csc	:1;	// Connect Status Change R/WC
		_u32	ped	:1;	// Port Enabled/Disabled R/W. 1=Enable. 0=Disable. Default = 0.
		_u32	pedc	:1;	// Port Enable/Disable Change R/WC. 1=Port enabled/disabled
		_u32	oca	:1;	// Over-current Active RO. Default = 0
		_u32	occ	:1;	// Over-current Change R/WC. Default = 0
		_u32	fpr	:1;	// Force Port Resume  R/W.
		_u32	suspend	:1;	// Suspend R/W. 1=Port in suspend state
		_u32	pr	:1;	// Port Reset  R/W. 1=Port is in Reset
		_u32		:1;
		_u32	ls	:2;	// Line Status RO.
		_u32	pp	:1;	// Port Power (PP) R/W
		_u32	po	:1;	// Port Owner  R/W Default = 1b
		_u32	pic	:2;	// Port Indicator Control. Default = 00b
		_u32	ptc	:4;	// Port Test Control R/W. Default = 0000b
		_u32	wce	:1;	// Wake on Connect Enable (WKCNNT_E)  R/W. Default = 0b.
		_u32	wde	:1;	// Wake on Disconnect Enable (WKDSCNNT_E)  R/W. Default = 0b.
		_u32	woce	:1;	// Wake on Over-current Enable (WKOC_E)  R/W. Default = 0b
		_u32		:9;
	}__attribute__((packed)) portsc[MAX_EHCI_NDP];
};


#endif

