#include "bootx86.h"

void __attribute__((optimize("O0"))) print_char(_char_t c) {
	__asm__ __volatile__ (
		"movb	%[c],%%al\n\t"
		"movb	$0x7,%%bl\n\t"
		"movb	$0x0e, %%ah\n\t"
		"int	$0x10\n"
		:
		:[c] "m" (c)
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
void __attribute__((optimize("O0"))) display_text(_str_t _ptr, _u16 sz, _u8 row, _u8 col, _u8 color) {
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

void __attribute__((optimize("O0"))) print_text(_str_t p, _u16 sz) {
	_u32 _sz = sz;
	_u32 i = 0;
	
	while(_sz) {
		print_char(*(p + i));
		
		i++;
		_sz--;
	}	
}

// display byte in hex format
void __attribute__((optimize("O0"))) print_byte(_u8 c) {
	_char_t al = c;
	_u8 bl = 0x30;
	
	al = c >> 4;

	if(al < 0x0a)
		al += bl;
	else
		al += bl + 7;
		
	print_char(al);

	al = c & 0x0f;
	 
	if(al < 0x0a)
		al += bl;
	else
		al += bl + 7;

	print_char(al);
}
// display word in hex format
void __attribute__((optimize("O0"))) print_word(_u16 w) {
	_u8 x;
	
	x = (_u8)(w >> 8);
 	print_byte(x);
	x = (_u8)w;
	print_byte(x);
}

// display double word in hex format
void __attribute__((optimize("O0"))) print_dword(_u32 dw) {
	_u16 x = (_u16)(dw>>16);
	
	print_word(x);
	x = (_u16)dw;
	print_word(x);
}

void __attribute__((optimize("O0"))) print_qword(_u32 qw[2]) {
	print_dword(qw[1]);
	print_dword(qw[0]);
}


static __attribute__((optimize("O0"))) _u8 get_byte(_u8 *ptr) {
	_u8 r = 0;
	_u16 seg = (_u16)((_u32)ptr / 16);
	_u16 off = (_u16)((_u32)ptr % 16);
	
	__asm__ __volatile__ (
		"movw	%%es, %%cx\n"
		"movw	%[seg], %%ax\n"
		"movw	%%ax, %%es\n"
		"movw	%[off], %%bx\n"
		"movb	%%es:(%%bx), %%al\n"
		"movb	%%al, %[r]\n"
		"movw	%%cx, %%es\n"
		:[r] "=m" (r)
		:[seg] "m" (seg), [off] "m" (off)
	);
	
	return r;
}

// display hex dump
void print_hex(_u8 *ptr,_u16 sz) {
	_u16 c = sz;
	_u16 i = 0;
	
	while(c) {
	 	//print_byte(*(ptr+i));
		print_byte(get_byte(ptr+i));
		print_char(0x20);
		
		i++;
		c--;
	}	
}

// hide display cursor
void __attribute__((optimize("O0"))) hide_cursor(void) {
	__asm__ __volatile__ (
		"movb	$1,%ch\n"
		"shlb	$5,%ch\n"
		"movb	$0,%cl\n"
		"movb	$1,%ah\n"
		"int	$0x10\n"
	);
}

void __attribute__((optimize("O0"))) set_cursor_pos(_u8 row, _u8 col) {
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

void __attribute__((optimize("O0"))) clear_screen(void) {
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
