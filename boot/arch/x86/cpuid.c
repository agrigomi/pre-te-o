#ifndef _DEBUG
#include "dtype.h"
#include "boot.h"
#else
#include "mgtype.h"
#define BEGIN_CODE32
#define END_CODE32
#endif

static void cpuid(_u32 selector, _u32 *result) {
       __asm__ __volatile__ (
		"cpuid\n"
		: "=a" (result[0]),
		  "=b" (result[1]),
		  "=c" (result[2]),
		  "=d" (result[3])
		: "a"(selector)
       );
}

// return 1 if CPU has HT, otherwise 0
_u8 is_ht_support(void) {
	_u8 r = 0;
	_u32 reg[4]={0,0,0,0};

	cpuid(1,reg);
	if(reg[3] & 0x10000000)
		r = 1;

	return r;
}

_u32 ht_num_per_socket(void) {
	_u32 r = 0;
	_u32 reg[4]={0,0,0,0};

	if(is_ht_support()) {
		cpuid(1, reg);
		r = (reg[1] & 0x00FF0000);
		r >>= 16;
	}

	return r;
}

_u32 core_num_per_socket(void) {
	_u32 r = 1;
	_u32 reg[4]={0,0,0,0};

	cpuid(0, reg);

	if(reg[0] >= 4) {
		cpuid(4, reg);
		r = ((reg[0] & 0xfc000000) >> 26)+1;
	}

	return r;
}

// return 1 if CPU supports Long Mode
_u8 lm_support(void) {
	_u8 r = 0;
	_u32 reg[4]={0,0,0,0};

	cpuid(0x80000000, reg);
	if(reg[0] >= 0x80000001) {
		cpuid(0x80000001, reg);
		if(reg[3] & (1 << 29))
			r = 1;
	}

	return r;
}

#define INITIAL_APIC_ID_BITS 0xff000000 //(or (0xFF << 24) )

_u8 get_initial_apic_id(void) {
	_u32 reg[4]={0,0,0,0};

	cpuid(1, reg);
	return (_u8)((reg[1] & INITIAL_APIC_ID_BITS) >> 24);
}

static _u32 find_maskwidth(_u32 count_item) {
	_u32 mask_width, cnt = count_item;
	__asm__ __volatile__(
		"mov %1, %%eax\n"
		"mov $0, %%ecx\n"
		"mov %%ecx, %0\n"
		"dec %%eax\n"
		"bsr %%ax, %%cx\n"
		"jz _find_maskwidth_next_\n"
		"inc %%cx\n"
		"mov %%ecx, %0\n"
		"_find_maskwidth_next_:\n"
		"mov %2, %%eax\n"
		:"=m"(mask_width)
		:"m"(cnt), "m"(mask_width)
	);
	
	return mask_width;
}

//
// This routine extracts a subset of bit fields from the 8-bit Full_ID
// The return value, or subID, is a non-zero-based, 8-bit value
//
_u8 get_nzb_sub_id(_u8 full_id,
		   _u8 max_sub_id_value,
		   _u8 shift_sount) {
	_u32 mask_width;
	_u8 sub_id, mask_bits;
	
	mask_width = find_maskwidth((_u32) max_sub_id_value);
	mask_bits = ((_u8) (0xff << shift_sount)) ^ ((_u8) (0xff << (shift_sount + mask_width))) ;
	sub_id = full_id & mask_bits;
	
	return sub_id;
}
