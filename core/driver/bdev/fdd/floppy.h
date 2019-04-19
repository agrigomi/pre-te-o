#ifndef __FLOPPY_H__
#define __FLOPPY_H__

// FDC register offset from base
enum _fdc_registers_ {
	STATUS_REGISTER_A                = 0, // read-only
	STATUS_REGISTER_B                = 1, // read-only
	DIGITAL_OUTPUT_REGISTER          = 2,
	TAPE_DRIVE_REGISTER              = 3,
        MAIN_STATUS_REGISTER             = 4, // read-only
	DATARATE_SELECT_REGISTER         = 4, // write-only
	DATA_FIFO                        = 5,
	DIGITAL_INPUT_REGISTER           = 7, // read-only
	CONFIGURATION_CONTROL_REGISTER   = 7  // write-only
};

enum _fdc_commands {
	READ_TRACK =                 2,	// generates IRQ6
	SPECIFY =                    3,      // * set drive parameters
	SENSE_DRIVE_STATUS =         4,
	WRITE_DATA =                 5,      // * write to the disk
	READ_DATA =                  6,      // * read from the disk
	RECALIBRATE =                7,      // * seek to cylinder 0
	SENSE_INTERRUPT =            8,      // * ack IRQ6, get status of last command
	WRITE_DELETED_DATA =         9,
	READ_ID =                    10,	// generates IRQ6
	READ_DELETED_DATA =          12,
	FORMAT_TRACK =               13,     // *
	SEEK =                       15,     // * seek both heads to cylinder X
	VERSION =                    16,	// * used during initialization, once
	SCAN_EQUAL =                 17,
	PERPENDICULAR_MODE =         18,	// * used during initialization, once, maybe
	CONFIGURE =                  19,     // * set controller parameters
	LOCK =                       20,     // * protect controller params from a reset
	VERIFY =                     22,
	SCAN_LOW_OR_EQUAL =          25,
	SCAN_HIGH_OR_EQUAL =         29
};

//digital output register (DOR) bits
typedef union {
	_u8 value;	// value of Digital Output Register
	struct {
		_u8 dselect	:2;	// "Select" drive number for next access
		_u8 reset	:1;	// Clear = enter reset mode, Set = normal operation
		_u8 irq		:1;	// Set to enable IRQs and DMA
		_u8 mota	:1;	// Set to turn drive 0's motor ON
		_u8 botb	:1;	// Set to turn drive 1's motor ON
		_u8 motc	:1;	// Set to turn drive 2's motor ON
		_u8 motd	:1;	// Set to turn drive 3's motor ON
	};
}_fdc_dor_t;

// main status register (MSR) bits
typedef union {
	_u8 value;
	struct {
		_u8 acts	:1;	// Drive 0 is seeking
		_u8 actb	:1;	// Drive 1 is seeking
		_u8 actc	:1;	// Drive 2 is seeking
		_u8 actd	:1;	// Drive 3 is seeking
		_u8 cb		:1;	// Command Busy: set when command byte received, cleared at end of Result phase
		_u8 ndma	:1;	// Set in Execution phase of PIO mode read/write commands only.
		_u8 dio		:1;	// Set if FIFO IO port expects an IN opcode
		_u8 rqm		:1;	// Set if it's OK (or mandatory) to exchange bytes with the FIFO IO port
	};
}_fdc_msr_t;

#endif

