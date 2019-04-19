#include "mgtype.h"
#include "cpu.h"
#include "str.h"

#define VENDOR_INTEL	"GenuineIntel"
#define VENDOR_AMD	"AuthenticAMD"

// require min 12 byte by output
void cpu_vendor_id(_s8 *out) {
	_cpuid vend(0);
	*(_u32 *)(out + 0) = vend.ebx;
	*(_u32 *)(out + 4) = vend.edx;
	*(_u32 *)(out + 8) = vend.ecx;
}

_u16 cpu_logical_count(void) {
	_u16 r = 0;
	
	_cpuid lunits(1);
	r = (lunits.ebx & 0xff0000) >> 16;
	return r;
}

_u8 cpu_get_cores(void) {
	_u8 r = 1;
	
	_cpuid func4(0);
	if(func4.eax >= 4) {
		_s8 vendor[14];

		cpu_vendor_id(vendor);
		if(mem_cmp((_u8 *)vendor, (_u8 *)VENDOR_INTEL, 12) == 0) {
			_cpuid cores(4);
			r += ((cores.eax & 0xfc000000) >> 26);
		} else {
			if(mem_cmp((_u8 *)vendor, (_u8 *)VENDOR_AMD, 12) == 0) {
				_cpuid cores(0x80000008);
				r += (_u8)cores.ecx;
			}
		}
	}
	
	return r;
}

_u8  cpu_apic_id(void) {
	_u8 r = 0;
	
	_cpuid apics(1);
	r = (apics.ebx & 0xff000000) >> 24;
	return r;
}
