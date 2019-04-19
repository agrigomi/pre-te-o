#ifndef __BOOT_S2_H__
#define __BOOT_S2_H__

//#define __W64

#define BOOT_S1_ADDRESS		0x7c00
#define BOOT_S2_ADDRESS		0x4000
#define BOOT_S3_ADDRESS		0x7e00
#define KERNEL_ADDRESS		0xc000
#define PML4_PT_ADDRESS		0x100000 // page table start address


#define BEGIN_CODE32	asm(".code32\n");
#define END_CODE32	asm(".code16gcc\n");

#define BEGIN_CODE64	asm(".code64\n");
#define END_CODE64	asm(".code16gcc\n");


// prototype of protected mode routine
typedef void _t_pm_proc(void *p_data);
// prototype of long mode routine
typedef void _t_lm_proc(void *p_data);
// prototype of real mode routine
typedef void _t_rm_proc(void *p_data);

// This structure contains the value of one GDT entry.
// We use the attribute 'packed' to tell GCC not to change
// any of the alignment in the structure.
typedef struct  {
	_u16	limit_low;           // The lower 16 bits of the limit.
	_u16	base_low;            // The lower 16 bits of the base.
	_u8  	base_middle;         // The next 8 bits of the base.
	_u8	access;              // Access flags, determine what ring this segment can be used in.
	_u8	granularity;
	_u8	base_high;           // The last 8 bits of the base.
}__attribute__((packed)) _gdt_entry_t;

typedef struct {
	_u16	limit;               // The upper 16 bits of all selector limits.
	_u32	base;                // The address of the first gdt_entry_t structure.
}__attribute__((packed)) _gdt_ptr_t;

typedef struct {
  	_u32	dst;	// absolute or relative address of destination buffer
	_u32	src;	// absolute or relative address of source buffer 
	_u32	sz;	// size in bytes
} copy_32_t;

extern _str_t str_default;
extern _str_t str_countdown;
extern _str_t str_display;
extern _str_t str_mask;
extern _str_t _boot_config;
extern _u8 _gdt[];
extern _u16 _gdt_limit;
extern _str_t err_mem_map;
extern _str_t str_run_kernel_x86;
extern _str_t str_run_kernel_x64;
extern _str_t err_long_mode;
extern _str_t err_a20;
extern _str_t err_read;


//CPUID
extern _u8 is_ht_support(void);
extern _u32 ht_num_per_socket(void);
extern _u32 core_num_per_socket(void);
extern _u8 lm_support(void);
extern _u8 get_initial_apic_id(void);

// GDT
extern _u8 _gdt[];


// a20
int enable_a20(void);

#define MIN_FILE_INFO_SZ	512


// pm/lm
void _pm_call(_t_pm_proc *proc,void *p_data);
void _lm_call(_t_lm_proc *proc,void *p_data);
void _lm_cpu_idle(void *p_context);
void _cpu_init_vector(void);
void _32rm_call(_t_rm_proc *proc,void *p_data);
void _64rm_call(_t_rm_proc *proc,void *p_data);
// setup protected mode GDT
void pm_init_gdt(void);
void lm_init_gdt(void);
void _pm_copy(copy_32_t *p_data); // expected absolute address in p_data->dst
void call_kernel(_u8 dev, _u8 kcpu_mode, _u32 kernel_address,_u32 sz,_str_t kargs);

typedef unsigned int addr_t;

static inline void io_delay(void) {
	const _u16 DELAY_PORT = 0x80;
	asm volatile("outb %%al,%0" : : "dN" (DELAY_PORT));
}

static inline void outb(_u8 v, _u16 port) {
	asm volatile("outb %0,%1" : : "a" (v), "dN" (port));
}

static inline _u8 inb(_u16 port) {
	_u8 v;
	asm volatile("inb %1,%0" : "=a" (v) : "dN" (port));
	return v;
}

static inline void set_fs(_u16 seg) {
	asm volatile("movw %0,%%fs" : : "rm" (seg));
}

static inline void set_gs(_u16 seg) {
	asm volatile("movw %0,%%gs" : : "rm" (seg));
}

static inline _u32 rdfs32(addr_t addr) {
	_u32 v;
	asm volatile("movl %%fs:%1,%0" : "=r" (v) : "m" (*(_u32 *)addr));
	return v;
}

static inline void wrfs32(_u32 v, addr_t addr) {
	asm volatile("movl %1,%%fs:%0" : "+m" (*(_u32 *)addr) : "ri" (v));
}

static inline _u32 rdgs32(addr_t addr) {
	_u32 v;
	asm volatile("movl %%gs:%1,%0" : "=r" (v) : "m" (*(_u32 *)addr));
	return v;
}

static inline _u16 ds(void) {
	_u16 seg;
	asm("movw %%ds,%0" : "=rm" (seg));
	return seg;
}

#endif

