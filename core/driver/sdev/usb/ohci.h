#ifndef __OHCO_H__
#define __OHCI_H__

#define MAX_OHCI_NDP	256

typedef struct {
	struct {
		_u32	rev	:8;	// revision
		_u32		:24;	// reserved
	} __attribute__((packed)) HcRevision;

	struct {
		_u32	cbsr	:2; 	// ControlBulkServiceRatio
		_u32	ple	:1;	// PeriodicListEnable
		_u32	ie	:1;	// IsochronousEnable
		_u32	cle	:1;	// ControlListEnable
		_u32	ble	:1;	// BulkListEnable
		_u32	hcfs	:2;	// HostControllerFunctionalState
		_u32	ir	:1;	// InterruptRouting
		_u32	rwc	:1;	// RemoteWakeupConnected
		_u32	rwe	:1;	// RemoteWakeupEnable
		_u32		:21;
	} __attribute__((packed)) HcControl;

	struct {
		_u32	hcr	:1;	// HostControllerReset
		_u32	clf	:1;	// ControlListFilled
		_u32	blf	:1;	// BulkListFilled
		_u32	ocr	:1;	// OwnershipChangeRequest
		_u32		:12;
		_u32	soc	:2;	// SchedulingOverrunCount
		_u32		:14;
	} __attribute__((packed)) HcCommandStatus;

	struct {
		_u32	so	:1;	// SchedulingOverrun
		_u32	wdh	:1;	// WritebackDoneHead
		_u32	sf	:1;	// StartofFrame
		_u32	rd	:1;	// ResumeDetected
		_u32	ue	:1;	// UnrecoverableError
		_u32	fno	:1;	// FrameNumberOverflow
		_u32	rhsc	:1;	// RootHubStatusChange
		_u32		:23;
		_u32	oc	:1;	// OwnershipChange
		_u32		:1;
	} __attribute__((packed)) HcInterruptStatus;

	struct {
		_u32	so	:1;	// 1 - Enable interrupt generation due to Scheduling Overrun.
		_u32	wdh	:1;	// 1 - Enable interrupt generation due to HcDoneHead Writeback.
		_u32	sf	:1;	// 1 - Enable interrupt generation due to Start of Frame.
		_u32	rd	:1;	// 1 - Enable interrupt generation due to Resume Detect.
		_u32	ue	:1;	// 1 - Enable interrupt generation due to Unrecoverable Error.
		_u32	fno	:1;	// 1 - Enable interrupt generation due to Frame Number Overflow
		_u32	rhsc	:1;	// 1 - Enable interrupt generation due to Root Hub Status Change.
		_u32		:23;	
		_u32	oc	:1;	// 1 - Enable interrupt generation due to Ownership Change.
		_u32	mie	:1;	// A ‘0’ written to this field is ignored by HC. 
					// A '1' written to ield enables interrupt generation due to 
					// events specified inother bits of this register. 
					// This is used by HCD as a Mastenterrupt Enable.
	} __attribute__((packed)) HcInterruptEnable;

	struct {
		_u32	so	:1;	// 1 - Disable interrupt generation due to Scheduling Overrun.
		_u32	wdh	:1;	// 1 - Disable interrupt generation due to HcDoneHead Writeback.
		_u32	sf	:1;	// 1 - Disable interrupt generation due to Start of Frame.
		_u32	rd	:1;	// 1 - Disable interrupt generation due to Resume Detect.
		_u32	ue	:1;	// 1 - Disable interrupt generation due to Unrecoverable Error.
		_u32	fno	:1;	// 1 - Disable interrupt generation due to Frame Number Overflow
		_u32	rhsc	:1;	// 1 - Disable interrupt generation due to Root Hub Status Change.
		_u32		:23;
		_u32	oc	:1;	// 1 - Disable interrupt generation due to Ownership Change.
		_u32	mie	:1;	// A '0' written to this field is ignored by HC. A '1' written to this field
					// disables interrupt generation due to events specified in the other
					// bits of this register. This field is set after a hardware or software reset.
	} __attribute__((packed)) HcInterruptDisable;

	struct {
		_u32	zero	:8;
		_u32	hcca	:24;	//  This is the base address of the Host Controller
					// Communication Area.
	} __attribute__((packed)) HcHCCA;

	struct {
		_u32	zero	:4;
		_u32	pced	:28;	// PeriodCurrentED
	} __attribute__((packed)) HcPeriodCurrentED;

	struct {
		_u32	zero	:4;
		_u32	ched	:28;	// ControlHeadED
	} __attribute__((packed)) HcControlHeadED;

	struct {
		_u32	zero	:4;
		_u32	cced	:28;	// ControlCurrentED
	} __attribute__((packed)) HcControlCurrentED;

	struct {
		_u32	zero	:4;
		_u32	bhed	:28;	// BulkHeadED
	} __attribute__((packed)) HcBulkHeadED;

	struct {
		_u32	zero	:4;
		_u32	bced	:28;	// BulkCurrentED
	} __attribute__((packed)) HcBulkCurrentED;

	struct {
		_u32	zero	:4;
		_u32	dh	:28;	// DoneHead
	} __attribute__((packed)) HcDoneHead;

	struct {
		_u32	fi	:14;	// FrameInterval
		_u32		:2;
		_u32	fsmps	:15;	// FSLargestDataPacket
		_u32	fit	:1;	// FrameIntervalToggle
	} __attribute__((packed)) HcFmInterval;

	struct {
		_u32	fr	:14;	// FrameRemaining
		_u32		:17;
		_u32	frt	:1;	// FrameRemainingToggle
	} __attribute__((packed)) HcFmRemaining;

	struct {
		_u32	fn	:16;	// FrameNumber
		_u32		:16;
	} __attribute__((packed)) HcFmNumber;

	struct {
		_u32	ps	:14;	// PeriodicStart
		_u32		:18;
	} __attribute__((packed)) HcPeriodicStart;

	struct {
		_u32	lst	:12;	// LSThreshold
		_u32		:20;
	} __attribute__((packed)) HcLSThreshold;

	struct {
		_u32	ndp	:8;	// NumberDownstreamPorts
		_u32	psm	:1;	// PowerSwitchingMode
		_u32	nps	:1;	// NoPowerSwitching
		_u32	dt	:1;	// DeviceType
		_u32	ocpm	:1;	// OverCurrentProtectionMode
		_u32	nocp	:1;	// NoOverCurrentProtection
		_u32		:11;
		_u32	potpgt	:8;	// PowerOnToPowerGoodTime
	} __attribute__((packed)) HcRhDescriptorA;

	struct {
		_u32	dr	:16;	// DeviceRemovable
		_u32	ppcm	:16;	// PortPowerControlMask
	} __attribute__((packed)) HcRhDescriptorB;

	struct {
		_u32	lps	:1;	// LocalPowerStatus
		_u32	oci	:1;	// OverCurrentIndicator
		_u32		:13;
		_u32	drwe	:1;	// DeviceRemoteWakeupEnable
		_u32	lpsc	:1;	// LocalPowerStatusChange
		_u32	ocic	:1;	// OverCurrentIndicatorChange
		_u32		:13;
		_u32	crwe	:1;	// ClearRemoteWakeupEnable
	} __attribute__((packed)) HcRhStatus;

	struct {
		_u32	ccs	:1;	// CurrentConnectStatus
		_u32	pes	:1;	// PortEnableStatus
		_u32	pss	:1;	// PortSuspendStatus
		_u32	poci	:1;	// PortOverCurrentIndicator
		_u32	prs	:1;	// PortResetStatus
		_u32		:3;
		_u32	pps	:1;	// PortPowerStatus
		_u32	lsda	:1;	// LowSpeedDeviceAttached
		_u32		:6;
		_u32	csc	:1;	// ConnectStatusChange
		_u32	pesc	:1;	// PortEnableStatusChange
		_u32	pssc	:1;	// PortSuspendStatusChange
		_u32	ocic	:1;	// PortOverCurrentIndicatorChange
		_u32	prsc	:1;	// PortResetStatusChange
		_u32		:11;
	} __attribute__((packed)) HcRhPortStatus[MAX_OHCI_NDP];
} __attribute__((packed)) _reg_ohci_t;

typedef struct { // endpoint descriptor
	struct {
		_u32	fa	:7;	// FunctionAddress
		_u32	en	:4;	// EndpointNumber
		_u32	d	:2;	// direction
		_u32	s	:1;	// speed
		_u32	k	:1;	// skyp
		_u32	f	:1;	// format
		_u32	mps	:11;	// Max Packet Size;
		_u32	usr1	:5;
	}__attribute__((packed));

	struct {
		_u32	usr2	:4;
		_u32	tailp	:28;	// TDQueueTailPointer
	}__attribute__((packed));

	struct {
		_u32	h	:1;	// halted
		_u32	c	:1;	// toggle cary
		_u32		:2;
		_u32	headp	:28;	// TDQueueHeadPointer
	}__attribute((packed));

	struct {
		_u32	usr3	:4;
		_u32	nexted	:28;	// next EPD
	}__attribute((packed));
} __attribute__((packed)) __attribute__((aligned(16))) _ohci_ed_t;

typedef struct { // general transfer descriptor
	struct {
		_u32	usr	:18;	
		_u32	r	:1;	// buffer rounding
		_u32	dp	:2;	// direction/PID
		_u32	di	:3;	// delay interrupt
		_u32	t	:2;	// data toggle
		_u32	ec	:2;	// error count
		_u32	cc	:4;	// condition code
	}__attribute__((packed));

	_u32	cbp;	// current buffer pointer

	struct {	// next transfer descriptor
		_u32		:4;
		_u32	nexttd	:28;
	}__attribute__((packed));

	_u32	ebp;	// end buffer pointer
}__attribute__((packed)) __attribute__((aligned(32))) _ohci_gtd_t;

#endif //__OHCI_H__

