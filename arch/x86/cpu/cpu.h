#ifndef __CPU_H__
#define __CPU_H__

// usage:
//	_cpuid(<function number>)
struct _cpuid {
	_u32 eax;
	_u32 ebx;
	_u32 ecx;
	_u32 edx;
	
	_cpuid() { eax = ebx = ecx = edx = 0; }
	_cpuid(_u32 fn, _u32 b=0, _u32 c=0, _u32 d=0) { 
		read(fn, b, c, d); 
	}
	void read(_u32 fn, _u32 b=0, _u32 c=0, _u32 d=0) {
		eax = fn;
		ebx = b; ecx = c; edx = d;
		__asm__ __volatile__(
			"cpuid\n"
			: "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
			:  "a" (eax), "b" (ebx), "c" (ecx), "d" (edx)
		);
	}
};

// require min 12 byte by output
void cpu_vendor_id(_s8 *out);
_u16 cpu_logical_count(void);
_u8  cpu_apic_id(void);
_u8 cpu_get_cores(void);

#endif
