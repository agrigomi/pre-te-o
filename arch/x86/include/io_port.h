#ifndef __IO_PORT__
#define __IO_PORT__

// CMOS
#define CMOS_REG_SEL		0x70
#define CMOS_REG_VALUE		0x71

// keyboard
#define IO_KBD_INPUT		0x60
#define IO_KBD_STATUS		0x64

// PCI
#define PCI_CONFIG_ADDRESS	0xcf8
#define PCI_CONFIG_DATA		0xcfc

// IDE
#define IDE_PRIMARY_BASE	0x1f0
#define IDE_PRIMARY_CTRL	0x3f4
#define IDE_SECONDARY_BASE	0x170
#define IDE_SECONDARY_CTRL	0x374

// Floppy
#define FDC_BASE		0x3f0
#define FDC_CMOS_REG		0x10
#define FDC_DMA_CHANNEL		2

// DMA 8 bit transfer
#define DMA8_CH0_ADDR		0x00
#define DMA8_CH0_COUNT		0x01
#define DMA8_CH1_ADDR		0x02
#define DMA8_CH1_COUNT		0x03
#define DMA8_CH2_ADDR		0x04
#define DMA8_CH2_COUNT		0x05
#define DMA8_CH3_ADDR		0x06
#define DMA8_CH3_COUNT		0x07
#define DMA8_STATUS		0x08
#define DMA8_COMMAND		0x08
#define DMA8_REQUEST		0x09
#define DMA8_SCMASK		0x0a // single channel mask register
#define DMA8_MODE		0x0b
#define DMA8_FLIPFLOP		0x0c
#define DMA8_MASTER_RESET	0x0d
#define DMA8_MASK_RESET		0x0e
#define DMA8_MCMASK		0x0f // multi channel mask register
// DMA 16 bit transfer
#define DMA16_CH4_ADDR		0xc0
#define DMA16_CH4_COUNT		0xc2
#define DMA16_CH5_ADDR		0xc4
#define DMA16_CH5_COUNT		0xc6
#define DMA16_CH6_ADDR		0xc8
#define DMA16_CH6_COUNT		0xca
#define DMA16_CH7_ADDR		0xcc
#define DMA16_CH7_COUNT		0xce
#define DMA16_STATUS		0xd0
#define DMA16_COMMAND		0xd0
#define DMA16_REQUEST		0xd2
#define DMA16_SCMASK		0xd4 // single channel mask register
#define DMA16_MODE		0xd6
#define DMA16_FLIPFLOP		0xd8
#define DMA16_MASTER_RESET	0xda
#define DMA16_MASK_RESET	0xdc
#define DMA16_MCMASK		0xde // multi channel mask register
// DMA address extension (page)
#define DMA_CH0_PAGE		0x87 // unusable
#define DMA_CH1_PAGE		0x83
#define DMA_CH2_PAGE		0x81
#define DMA_CH3_PAGE		0x82
#define DMA_CH4_PAGE		0x8f // unusable
#define DMA_CH5_PAGE		0x8b
#define DMA_CH6_PAGE		0x89
#define DMA_CH7_PAGE		0x8a


// BYTE I/O
inline _u8 inportb(_u32 port) {
	// read a byte from a port
	_u8 ret;
	asm volatile ("inb %%dx, %%al":"=a"(ret):"d"(port));
	return ret;
}

inline void outportb(_u32 port, _u8 value) {
	// write a byte to a port
	asm volatile ("outb %%al, %%dx": :"d"(port),"a"(value));
}

// DWORD I/O
inline _u32 inportl(_u32 port) {
	// read a 32 bit from a port
	_u32 ret;
	asm volatile ("inl %%dx, %%eax":"=a"(ret):"d"(port));
	return ret;
}

inline void outportl(_u32 port, _u32 value) {
	// write a 32 bit to a port
	asm volatile ("outl %%eax, %%dx": :"d"(port),"a"(value));
}

#endif

