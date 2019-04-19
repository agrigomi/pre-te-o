// bios32.c
#include "dtype.h"
#include "stub.h"
#include "bios.h"
#include "boot.h"
#include "lib.h"

typedef struct { // Device Address Packet
	_u8	packet_sz; /* Packet size in bytes. The value in this field shall be 16 (10h) or greater. If the packet size
	 is less than 16 the request shall be rejected with CF = 1b and AH = 01h. */

	_u8	reserved_1;
	
	_u8	sec_count; /* number of sectors to transfer
	 This field shall contain a maximum value of 127 (7Fh). If a
	 any other value is supplied, the request shall be rejected with CF=1b and AH=01h. If this
	 field is set to FFh, then the transfer buffer address shall be found at offset 10h, the number
	 of blocks to transfer shall be found at offset 18h, and the transfer buffer at offset 4 shall be
	 ignored. If this field is set to 00h, then no data shall be transferred. */
	 
	 _u8	reserved_2;
	 
	 _u16 	buffer_32[2]; /* Address of host transfer buffer. This is the host buffer that Read/Write operations shall
	 use to transfer the data. This is a 32-bit host address of the form Seg:Offset. If this field is
	 set to FFFFh:FFFFh then the address of the transfer buffer shall be found at offset 10h.*/
	 
	 _lba_t	lba_address;	/* Starting logical block address on the target device of the data to be transferred. This is a
	64-bit unsigned linear address. If the device supports LBA addressing this value should be
	passed unmodified. If the device does not support LBA addressing the routine making the
	INT 13h call shall convert this LBA to a CHS address using the current geometry in the
	following formula:
		LBA = (C1 * H0 + H1) * S0 + S1 - 1
	Where:
		C1 = Selected Cylinder Number
		H0 = Number of Heads (Maximum Head Number + 1)
		H1 = Selected Head Number
		S0 = Maximum Sector Number
		S1 = Selected Sector Number
	For ATA compatible devices with less than or equal to 15,482,880 logical sectors, the H0
	and S0 values are supplied by words 3 and 6 of the data returned as a result of an
	IDENTIFY DEVICE command. */
	
	_u32	buffer_64[2]; /* 64-bit unsigned linear address of the host transfer buffer. This is the host buffer that
	Read/Write operations shall use to transfer the data if the data at offset 4 is set to
	FFFFh:FFFFh, or the data at offset 2 is set to FFh. */
	
	_u16	total_sec_count; /* Total number of blocks to transfer when the data at offset 2 is set to FFh */
	_u16	reserved_3;	 /* Reserved. The value in this field shall be 00h. */
} dap_t;

void bios_print_char(_char_t c) {
	__asm__ __volatile__ (
		"movb	%0,%%al\n\t"
		"movb	$0x7,%%bl\n\t"
		"movb	$0x0e, %%ah\n\t"
		"int	$0x10\n"
		:
		:"m" (c)
	);
}


void bios_wait(_u32 micro_s) {
	_u16 lo = (_u16)(micro_s);
	_u16 hi = (_u16)(micro_s >> 16);
	
	__asm__ __volatile__ (
		"movw	%0,%%dx;\n"
		"movw	%1,%%cx;\n"
		"movb	$0x86,%%ah;\n"
		"int	$0x15;\n"
		:
		:"m"(lo),"m"(hi)
	);
}

//---- print text with attributes
//input:
//   color: 	attribute if string contains only characters (bit 1 of AL is zero).
//	0      0000      black
//	1      0001      blue
//	2      0010      green
//	3      0011      cyan
//	4      0100      red
//	5      0101      magenta
//	6      0110      brown
//	7      0111      light gray
//	8      1000      dark gray
//	9      1001      light blue
//	A      1010      light green
//	B      1011      light cyan
//	C      1100      light red
//	D      1101      light magenta
//	E      1110      yellow
//	F      1111      white
//   sz:  	number of characters in string (attributes are not counted).
//   row,col:  	row,column at which to start writing.
//   _ptr: 	points to string to be printed. 
void bios_display_text(_str_t _ptr, _u16 sz, _u8 row, _u8 col, _u8 color) {
 	__asm__ __volatile__ (
		"movw	%0,%%cx\n"
		"movb	%1,%%dl\n"
		"movb	%2,%%dh\n"
		"movb	%3,%%bl\n"
		"movl	%4,%%ebp\n"
		"movb	$0x0,%%al\n"
		"movb	$0x0,%%bh\n"
		"movb	$0x13,%%ah\n"
		"int	$0x10\n"
		:
		:"m"(sz),"m"(col),"m"(row),"m"(color),"m"(_ptr)
	);
}

void bios_print_text(_str_t p, _u16 sz) {
	_u32 _sz = sz;
	_u32 i = 0;
	
	while(_sz) {
		bios_print_char(*(p + i));
		
		i++;
		_sz--;
	}	
}

// display byte in hex format
void bios_print_byte(_u8 c) {
	_char_t al = c;
	_u8 bl = 0x30;
	
	al = c >> 4;

	if(al < 0x0a)
		al += bl;
	else
		al += bl + 7;
		
	bios_print_char(al);

	al = c & 0x0f;
	 
	if(al < 0x0a)
		al += bl;
	else
		al += bl + 7;

	bios_print_char(al);
}
// display word in hex format
void bios_print_word(_u16 w) {
	_u8 x;
	
	x = (_u8)(w >> 8);
 	bios_print_byte(x);
	x = (_u8)w;
	bios_print_byte(x);
}

// display double word in hex format
void bios_print_dword(_u32 dw) {
	_u16 x = (_u16)(dw>>16);
	
	bios_print_word(x);
	x = (_u16)dw;
	bios_print_word(x);
}

void bios_print_qword(_u32 qw[2]) {
	bios_print_dword(qw[1]);
	bios_print_dword(qw[0]);
}


// display hex dump
void bios_print_hex(_u8 *ptr,_u16 sz) {
	_u16 c = sz;
	_u16 i = 0;
	
	while(c) {
	 	bios_print_byte(*(ptr+i));
		bios_print_char(0x20);
		
		i++;
		c--;
	}	
}

void bios_set_cursor_pos(_u8 row, _u8 col) {
	// set cursor position
	__asm__ __volatile__ (
		"movb	$2,%%ah\n"
		"movb	$0,%%bl\n"
		"movb	%0,%%dl\n"
		"movb	%1,%%dh\n"
		"int	$0x10\n"
		:
		:"m"(col),"m"(row)
	);
}

// waiting for keyboard input
//output:
//	16 bit word as:
//	 scan code in hi byte and
//	 ascii code in lo byte
_u16 bios_wait_key(void) {
	_u16 res = 0;
	
	__asm__ __volatile__ (
		"movb	$0,%%ah\n"
		"int	$0x16\n"
		"movw	%%ax,%0\n"
		:"=m"(res)
		:
	);
	
	return res;
}

// hide display cursor
void bios_hide_cursor(void) {
	__asm__ __volatile__ (
		"movb	$1,%ch\n"
		"shlb	$5,%ch\n"
		"movb	$0,%cl\n"
		"movb	$1,%ah\n"
		"int	$0x10\n"
	);
}

// reading keyboard input buffer
//output:
//	16 bit word as:
//	 scan code in hi byte and
//	 ascii code in lo byte
_u16 bios_get_key(void) {
 	_u16 res = 0;
	
	__asm__ __volatile__ (
		"movb	$1,%%ah\n"
		"int	$0x16\n"
		"jz	bios_get_key_done\n"
		"call	bios_wait_key\n"
		"movw	%%ax,%0\n"
		"bios_get_key_done:\n"
		:"=m"(res)
		:
	);
	
	return res;
}

void bios_clear_screen(void) {
 	__asm__ __volatile__ (
		"movw	0x7d0,%cx\n"
		"movw	$0x00,%di\n"
		"movw	$0xb800,%ax\n"
		"movw	%ax,%es\n"
		"__write_to_video:\n"
		"movw	$0x0720,%ax\n"
		"movw	%ax,%es:(%di)\n"
		"addw	$0x02,%di\n"
		"decw	%cx\n"
		"cmpw	$0x00,%cx\n"
		"jne	__write_to_video\n"
		"movw	%ds,%ax\n"
		"movw	%ax,%es\n"
	);
}

_u32 bios_get_rtc(void) {
	_u32 r = 0;
	
	__asm__ __volatile__ (
		"movb	$0,%%ah\n"
		"int	$0x1a\n"
		"shll	$0x10,%%ecx\n"
		"orl	%%edx,%%ecx\n"
		"movl	%%ecx,%0\n"
		:"=m"(r)
		:
	);
	
	return r;
}

void bios_set_rtc(_u32 tm) {
	__asm__ __volatile__ (
		"movb	$1,%%ah\n"
		"movl	%0,%%ecx\n"
		"shll	$0x10,%%ecx\n"
		"movl	%0,%%edx\n"
		"int	$0x1a\n"
		:
		:"m"(tm)
	);
}

void bios_enable_a20(void) {
 	__asm__ __volatile__ (
		"movw	$0x2401,%ax\n"
		"int	$0x15\n"
	);
}

// using int 0x15 AX=0xE820 to receive memory map array
_u16 bios_get_memory_map_e820(_u8 *buffer,_u16 sz) {
 	_u16 _sz = sz;
	_u8 *ptr = buffer;
	_u16 r = 0;
	
 	__asm__ __volatile__ (
		"xorl	%%eax,%%eax\n"		// EAX=0
		"movl	%%eax,%%ebx\n"		// EBX=0
		"movl	%%eax,%%ecx\n"		// ECX=0
		"movl	%2,%%edi\n"		// pointer to entry
		"__e820_loop:\n"
		"movw	$0x0e820,%%ax\n"	// EAX=0E820 - bios function
		"movl	$0x534D4150,%%edx\n"	// EDX=ascii 'SMAP'
		"movw	%3,%%cx\n"		// size of array
		"cmpw	$0,%%cx\n"
		"je	__done_e820\n"
		"int	$0x15\n"		// call bios
		"jc	__done_e820\n"
		"addw	%%cx,%0\n"		// r += returned bytes (cx)
		"addl	%%ecx,%%edi\n"
		"subw	%%cx,%1\n"
		"cmpl	$0,%%ebx\n"
		"jne	__e820_loop\n"
		"__done_e820:\n"
		:"=m"(r),"=m"(_sz)
		:"m"(ptr),"m"(_sz)
	);
	
	return r;
}

/*
conventional read sector by BIOS
input:
	mode:	read or write
	lba:	linear sector address
	dev:	bios device number
	_ptr:	pointer to destination buffer
output:
	error code if CF = 1
	0=OK
*/
_u8 bios_rw_sector_conv(_u8 mode, _u32 lba, _u8 dev, _u8 *_ptr) {
	_u16 mcyl = 0;		// maximum cylinders
	_u8  mhd  = 0;		// maximum heads
	_u8  mspt = 0;		// maximum sectors per track
	_u16 _cyl = 0;		// cylinder
	_u8  _hd  = 0;		// head
	_u8  _sec = 0;		// sector
	_u8  err = 0;		// error code
	_u16 temp = 0;
	
	__asm__ __volatile__ (
		"movb	%4,%%dl\n"		// DL = bios device number
		"movb	$8,%%ah\n"		//; read device parameters function
		"int	$0x13\n"
		"movb	%%ah,%0\n"		// store error code
		"xorw	%%ax,%%ax\n"		// AX = 0
		"movb	%%cl,%%al\n"		//; hi order of cylinder number --> AX
		"andw	$0xc0,%%ax\n"		//; keep only 2 bits 
		"shlw	$2,%%ax\n"		//; move 2 bits to hi order of word
		"movb	%%ch,%%al\n"		//; load lo part of max number of cylinders
		"movw	%%ax,%1\n"		//; store max. munber of cylinders (mcyl)
		"incb	%%dh\n"			//; max. head number + 1
		"movb	%%dh,%2\n"		//; store total number of heads (mhd)
		"movb	%%cl,%%al\n"		//; load sectors per track
		"andb	$0x3f,%%al\n"		//; clear hi order 2 bits for cylinder
		"movb	%%al,%3\n"		//; store max. munber of sectors per track (mspt)
		
		:"=m"(err),"=m"(mcyl),"=m"(mhd),"=m"(mspt)
		:"m"(dev)
	);
	
	if(!err) { // calculating CHS
	 	_cyl = lba / (mhd * mspt);
		temp = lba % (mhd * mspt);
		_hd = temp / mspt;
		_sec = temp % mspt + 1;
		
		__asm__ __volatile__ (
			"movb	%1,%%dl\n"		//; load device number (dev)
			"movw	%%ds,%%ax\n"
			"movw	%%ax,%%es\n"		//; ES = DS
			"movl	%2,%%ebx\n"		//; load buffer (_ptr)
			"movw	%3,%%ax\n"		//; AX = (_cyl)
			"movb	%%al,%%ch\n"		//; load cylinder
			"movb	%4,%%dh\n"		//; load head (_hd)
			"movb	%%ah,%%cl\n"		//; load hi order of cylinder
			"shlb	$6,%%cl\n"
			"orb	%5,%%cl\n"		//; load sector (_sec)
			"movb	%6,%%ah\n"		//; sector R/W function
			"movb	$1,%%al\n"		//; one sector
			"int	$0x13\n"		//; call BIOS
			"mov	%%ah,%0\n"
		
			:"=m"(err)
			:"m"(dev),"m"(_ptr),"m"(_cyl),"m"(_hd),"m"(_sec),"m"(mode)
		);
	}
	
	return err;	
}


/*---- extended sector read function using LBA address
input:
	mode:	read or write
	lba:	linear sector address
	dev:	bios device number
	count:	number of sectors to read
	_ptr:	destination buffer
output:
	error code if CF = 1
	0=OK
*/
_u8 bios_rw_sector_ex(_u8 mode, _lba_t *p_lba, _u8 dev, _u8 count, _u8 *_ptr) {
	_u8 res = 0;
	_u16 data_seg = 0;
	dap_t	_dap;
	dap_t	*p_dap = &_dap;
	_u8 fn = mode|0x40;
	
	__asm__ __volatile__ (
		"movw	%%ds,%%ax\n"
		"movw	%%ax,%0\n"
		:"=m"(data_seg)
		:
	);
	
	_dap.packet_sz 			= sizeof(_dap);
	_dap.lba_address._lw	 	= p_lba->_lw;	// set lo order of lba address
	_dap.lba_address._hw 		= p_lba->_hw;	// set hi order of lba address
	_dap.sec_count 			= count;	// set count of sectors to read	
	_dap.buffer_32[0]		= 0xffff;
	_dap.buffer_32[1]		= 0xffff;
	_dap.buffer_64[0] 		= (_u32)_ptr;
	_dap.buffer_64[1]		= 0;
	
	__asm__ __volatile__ (
		"movw	%%ss,%%bx\n"
		"movw	%%bx,%%ds\n"	// DS = SS
		"movl	%1,%%esi\n"
		"movb	%3,%%dl\n"
		"movb	%4,%%ah\n"	// extended read bios function
		"int	$0x13\n"
		"movb	%%ah,%0\n"	// set error code
		"movw	%2,%%bx\n"
		"movw	%%bx,%%ds\n"	// restore data segment
		:"=m"(res)
		:"m"(p_dap),"m"(data_seg),"m"(dev),"m"(fn)
	);
	
	return res;	
}

/*---- sector read function using LBA address
input:
	mode:	read (02) or write (03)
	lba:	linear sector address
	dev:	bios device number
	count:	number of sectors to read
	_ptr:	destination buffer
output:
	error code if CF = 1
	0=OK
*/	
_u8 bios_rw_sector(_u8 mode, _lba_t *p_lba, _u8 dev, _u8 count, _u8 *_ptr) {
	_u8 res = 0;
	_u8 c = count;
	_u16 lba32 = p_lba->_lw;
	
	_u8 conv = 0;
	
	__asm__ __volatile__ (
		"movb	$0x41,%%ah\n"
		"movb	%1,%%dl\n"
		"movw	$0xaa55,%%bx\n"
		"int	$0x13\n"
		"jnc	extended_read\n"
		"movb	$1,%0\n"
		"extended_read:\n"
		:"=m"(conv)
		:"m"(dev)
	);
	
	if(conv) {
		while(c) { // conventional read one by one
			res = bios_rw_sector_conv(mode,lba32,dev,_ptr);
			lba32++;
			_ptr += SZ_SECTOR;
			c--;
		}
	} else
		res = bios_rw_sector_ex(mode,p_lba,dev,count,_ptr);

	return res;	
}

/* ----- receive device parameters table by BIOS
input:
	dev:	BIOS device number
output:
	dp:	device parameters table
return:	
	error code if CF = 1
	0=OK
*/
_u8 bios_get_device_parameters(_u8 dev, _bios_device_parameters_t *dp) {
	_u8 r = 0;
	_u16 data_seg = 0; // data segment backup
	_bios_device_parameters_t _dp; // stack location of DP
	_bios_device_parameters_t *p_dp = &_dp; // local stack pointer of DP;
	
	p_dp->size = sizeof(_bios_device_parameters_t);
	
	__asm__ __volatile__ (
		"movb	$0x48,%%ah\n"
		"movb	%1,%%dl\n"
		"movw	%2,%%si\n"
		"movw	%%ds,%%bx\n"
		"movw	%%bx,%3\n"
		"movw	%%ss,%%bx\n"
		"movw	%%bx,%%ds\n"
		"int	$0x13\n"
		"movb	%%ah,%0\n"
		"movw	%3,%%bx\n"
		"movw	%%bx,%%ds\n"
		:"=m"(r)
		:"m"(dev),"m"(p_dp),"m"(data_seg)
	);

	if(r == 0) {// No error
		mem_cpy((_u8 *)dp, (_u8 *)p_dp, sizeof(_bios_device_parameters_t));
	}

	return r;
}
