#ifndef __AHCI_H__
#define __AHCI_H__

#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	0x96690101	// Port multiplier

#define SATA_CMD_READ_PIO          0x20
#define SATA_CMD_READ_PIO_EX       0x24
#define SATA_CMD_READ_DMA          0xC8
#define SATA_CMD_READ_DMA_EX       0x25
#define SATA_CMD_WRITE_PIO         0x30
#define SATA_CMD_WRITE_PIO_EX      0x34
#define SATA_CMD_WRITE_DMA         0xCA
#define SATA_CMD_WRITE_DMA_EX      0x35
#define SATA_CMD_CACHE_FLUSH       0xE7
#define SATA_CMD_CACHE_FLUSH_EX    0xEA
#define SATA_CMD_PACKET            0xA0
#define SATA_CMD_IDENTIFY_PACKET   0xA1
#define SATA_CMD_IDENTIFY          0xEC
#define SATAPI_CMD_READ            0xA8
#define SATAPI_CMD_WRITE           0xAA
#define SATAPI_CMD_EJECT           0x1B

#define SATA_IDENT_DEVICETYPE   0
#define SATA_IDENT_CYLINDERS    2
#define SATA_IDENT_HEADS        6
#define SATA_IDENT_SECTORS      12
#define SATA_IDENT_SERIAL       20
#define SATA_IDENT_MODEL        54
#define SATA_IDENT_CAPABILITIES 98
#define SATA_IDENT_FIELDVALID   106
#define SATA_IDENT_MAX_LBA      120
#define SATA_IDENT_COMMANDSETS  164
#define SATA_IDENT_MAX_LBA_EXT  200

#define SATA_PACKET_SIZE	16

typedef struct {
	_u32	cap;		//(0) Host Capabilities
	_u32	ghc;		//(1) Global Host Control
	_u32	is;		//(2) Interrupt Status
	_u32	pi;		//(3) Ports Implemented
	_u32	vs;		//(4) Version
	_u32	ccc_ctrl;	//(4) Command Completion Coalescing Control
	_u32	ccc_ports;	//(6) Command Completion Coalsecing Ports
	_u32	em_loc;		//(7) Enclosure Management Location
	_u32	em_ctl;		//(8) Enclosure Management Control
	_u32	cap2;		//(9) Host Capabilities Extended
	_u32	bohc;		//(10) BIOS/OS Handoff Control and Status
}__attribute__((packed)) _hba_mem_t;

typedef struct {
	_u32	clb;		// Command List Base Address, 1K-byte aligned
	_u32	clbu;		// Command List Base Address Upper 32-Bits
	_u32	fb;		// FIS base address, 256-byte aligned
	_u32	fbu;		// FIS base address upper 32 bits
	_u32	is;		// interrupt status
	_u32	ie;		// interrupt enable
	_u32	cmd;		// command and status
	_u32	rsv0;		// reserved
	_u32	tfd;		// task file data
	_u32	sig;		// signature
	_u32	ssts;		// SATA status (SCR0:SStatus)
	_u32	sctl;		// SATA control (SCR2:SControl)
	_u32	serr;		// SATA error (SCR1:SError)
	_u32	sact;		// SATA active (SCR3:SActive)
	_u32	ci;		// command issue
	_u32	sntf;		// SATA notification (SCR4:SNotification)
	_u32	fbs;		// FIS-based switch control
	_u32	rsv1[11];	// reserved
	_u32	vs[4];		// vendor specific
}__attribute__((packed)) _hba_port_t;

#define HBAP_CMD_ST	(1<<0)
#define HBAP_CMD_FRE	(1<<4)
#define HBAP_CMD_FR	(1<<14)
#define HBAP_CMD_CR	(1<<15)

typedef struct { // command header
	// DW0
	_u8	cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
	_u8	a:1;		// ATAPI
	_u8	w:1;		// Write, 1: H2D, 0: D2H
	_u8	p:1;		// Prefetchable
	_u8	r:1;		// Reset
	_u8	b:1;		// BIST
	_u8	c:1;		// Clear busy upon R_OK
	_u8	rsv0:1;		// Reserved
	_u8	pmp:4;		// Port multiplier port
 	_u16	prdtl;		// Physical region descriptor table length in entries
 	// DW1
	volatile
	_u32	prdbc;		// Physical region descriptor byte count transferred
 	// DW2, 3
	_u32	ctba;		// Command table descriptor base address
	_u32	ctbau;		// Command table descriptor base address upper 32 bits
	// DW4 - 7
	_u32	rsv1[4];	// Reserved
}__attribute__((packed)) _cmd_hdr_t;

typedef struct { // hysical Region Descriptor Table (PRDT)
	_u32	dba;		// data base address
	_u32	dbau;		// data base address upper 32
	_u32	rsv0;		// reserved
	// DW3
	_u32	dbc	:22;	// data byte count max 4M
	_u32	rsv1	:9;	// reserved
	_u32	i	:1;	// interrupt on complete
}__attribute__((packed)) _prdt_entry_t;

typedef struct {
	_u8	cfis[64]; 	// command FIS
	_u8 	acmd[SATA_PACKET_SIZE];	// atapi commands
	_u8	reserved[48];
	_prdt_entry_t prdt_entry[1];
}__attribute__((packed)) _cmd_tbl_t;

#define SATA_DEV_BUSY 	0x80
#define SATA_DEV_DRQ 	0x08

typedef struct { // host to device
	_u8	fis_type;
	_u8	pmport	:4;	// Port multiplier
	_u8	rsv0	:3;
	_u8	c	:1;	// 1: Command, 0: Control
	_u8	command;	// Command register
	_u8	featurel;	// Feature register, 7:0
	//dword 1
	_u8	lba0;		// LBA low register, 7:0
	_u8	lba1;		// LBA mid register, 15:8
	_u8	lba2;		// LBA high register, 23:16
	_u8	device;		// Device register
	// dword 2
	_u8	lba3;		// LBA register, 31:24
	_u8	lba4;		// LBA register, 39:32
	_u8	lba5;		// LBA register, 47:40
	_u8	featureh;	// Feature register, 15:8
	// dword 3
	_u8	countl;		// Count register, 7:0
	_u8	counth;		// Count register, 15:8
	_u8	icc;		// Isochronous command completion
	_u8	control;	// Control register
	// dword 4
	_u8	rsv1[4];	// Reserved
}__attribute__((packed)) _fis_reg_h2d_t;

typedef struct { // device to host
	// DWORD 0
	_u8	fis_type;    // FIS_TYPE_REG_D2H
	_u8	pmport:4;    // Port multiplier
	_u8	rsv0:2;      // Reserved
	_u8	i:1;         // Interrupt bit
	_u8	rsv1:1;      // Reserved
	_u8	status;      // Status register
	_u8	error;       // Error register
	// DWORD 1
	_u8	lba0;        // LBA low register, 7:0
	_u8	lba1;        // LBA mid register, 15:8
	_u8	lba2;        // LBA high register, 23:16
	_u8	device;      // Device register
	// DWORD 2
	_u8	lba3;        // LBA register, 31:24
	_u8	lba4;        // LBA register, 39:32
	_u8	lba5;        // LBA register, 47:40
	_u8	rsv2;        // Reserved
 	// DWORD 3
	_u8	countl;      // Count register, 7:0
	_u8	counth;      // Count register, 15:8
	_u8	rsv3[2];     // Reserved
 	// DWORD 4
	_u8	rsv4[4];     // Reserved
}__attribute__((packed)) _fis_reg_d2h_t;

#endif

