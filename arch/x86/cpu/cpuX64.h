#ifndef __CPU_X64__
#define __CPU_X64__

#include "iSyncObject.h"
#include "iSystemLog.h"
#include "iCPUHost.h"
#include "mp_scan.h"
#include "iLocalApic.h"
#include "iVMMx86.h"

#define CPU_ADDRESS(addr) (_ulong)addr

// CPU flags
#define FLAG_C	 (1<<0)  // carry
#define FLAG_P	 (1<<2)	 // parity
#define FLAG_A	 (1<<4)	 // Auxiliary
#define FLAG_Z	 (1<<6)	 // Zero
#define FLAG_S	 (1<<7)	 // Sign
#define FLAG_T	 (1<<8)	 // Trap
#define FLAG_I	 (1<<9)	 // Interrupt
#define FLAG_D	 (1<<10) // Direction
#define FLAG_O	 (1<<11) // Overflow
#define FLAG_N	 (1<<14) // Nested task
#define FLAG_R	 (1<<16) // Resume
#define FLAG_VM  (1<<17) // Virtual-8086 Mode
#define FLAG_AC  (1<<18) // Alignment Check
#define FLAG_VIF (1<<19) // Virtual Interrupt Flag
#define FLAG_VIP (1<<20) // Virtual Interrupt Pending
#define FLAG_ID	 (1<<21) // ID

#define DEFAULT_IST_INDEX	1

// descriptor type in long mode GDT
#define CODE_DESC_LM	0x18
#define CODE_DESC_C_LM	0x1c // conforming
#define DATA_DESC_LM	0x02
// system descriptor type in GDT
#define TSS_64		0x09
#define INT_64		0x0e
#define TRAP_64		0x0f

// segment mode
#define SM_32_G0	0x04
#define SM_32_G1	0x0c
#define SM_64_G0	0x02
#define SM_64_G1	0x0a

#define STACK_PAGES	1
#define INT_STACK_PAGES	1

#define MAX_GDT		16
// segment selector
// WARNING: code and data segment selectors
//	 are incoming from boot loader in 'starupt_context'
#define GDT_TSS_64	10	// two placess needed by TSS 64
#define GDT_LDT_64	12	//  and LDT 64 system descriptors

#define HALT \
	__asm__ __volatile__ ("hlt\n");	

/* CPU context  like HelenOS*/
typedef struct {
	_u64 rax;
	_u64 rbx;
	_u64 rcx;
	_u64 rdx;
	_u64 rsi;
	_u64 rdi;
	_u64 rbp;
	_u64 r8;
	_u64 r9;
	_u64 r10;
	_u64 r11;
	_u64 r12;
	_u64 r13;
	_u64 r14;
	_u64 r15;
	_u64 alignment;   /* align rbp_frame on multiple of 16 */
	_u64 rbp_frame;   /* imitation of frame pointer linkage */
	_u64 rip_frame;   /* imitation of return address linkage */
	_u64 error_word;  /* real or fake error word */
	_u64 rip;
	_u64 cs;
	_u64 rflags;
	_u64 rsp;         /* only if istate_t is from uspace */
	_u64 ss;          /* only if istate_t is from uspace */
}__attribute__((packed)) _cpu_state_t;

union _cpu_flags_t {
	_u64	flags;
	struct {
		_u32	carry		:1;	// 0  Carry flag
		_u32			:1;	// 1
		_u32	parity		:1;	// 2  Parity flag
		_u32			:1;	// 3
		_u32	adjust		:1;	// 4  Adjust flag
		_u32			:1;	// 5
		_u32	zero		:1;	// 6  Zero flag
		_u32	sign		:1;	// 7  Sign flag
		_u32	trap		:1;	// 8  Trap flag (single step)
		_u32	interrupt	:1;	// 9  Interrupt enable flag
		_u32	direction	:1;	// 10 Direction flag
		_u32	overflow	:1;	// 11 Overflow flag
		_u32	iopl		:2;	// 12-13 I/O privilege level (286+ only), always 11 on 8086 and 186
		_u32	nested_task	:1;	// 14 Nested task flag (286+ only), always 1 on 8086 and 186
		_u32			:1;	// 15
		_u32	resume		:1;	// 16 Resume flag (386+ only)
		_u32	v8086		:1;	// 17 Virtual 8086 mode flag (386+ only)
		_u32	align_check	:1;	// 18 Alignment check (486SX+ only)
		_u32	v_int		:1;	// 19 Virtual interrupt flag (Pentium+)
		_u32	v_int_pending	:1;	// 20 Virtual interrupt pending (Pentium+)
		_u32	id		:1;	// 21 (Able to use CPUID instruction (Pentium+))
		_u32			:10;	// 22-31
		_u32	reserved;
	};

	_cpu_flags_t() { get(); }
	void get(void) {
		__asm__ __volatile__ (
			"pushfq\n"
			"movq	(%%rsp), %%r8\n"
			"movq	%%r8, %[flags]\n"
			"addq	$8, %%rsp\n"
			:[flags] "=m" (flags)
		);
	}
};

// prototype of interrupt service routine
typedef void _isr_t(void);

typedef struct {
	_u16	addr1;		// first part of code segment offset
	_u16	cs;		// code segment selector
	_u8	ist;		// Interrupt Stack Table (3 bit)
	_u8	dtype	:5;
	_u8	dpl	:2;
	_u8	present	:1;
	_u16	addr2;		// Second patr of the offset
	_u32	addr3;		// Tird part of the offset
	_u32	reserved;

	void set_addr(_u64 addr) {
		addr1 = (_u16)addr;
		addr2 = (_u16)(addr >> 16);
		addr3 = (_u32)(addr >> 32);
	}

	_u64 _OPTIMIZE_SIZE_ get_addr(void) {
		return ((_u64)addr3 << 32) | ((_u64)addr2 << 16) | addr1;
	}

	void _OPTIMIZE_SIZE_ init(_u64 base, _u16 _cs, _u8 _ist, _u8 _type=TRAP_64, _u8 _dpl=DPL_USR) {
		dtype = _type;		// set type
		dpl = _dpl;
		present = 1;
		cs   = _cs << 3;	// set code selector
		ist  = _ist;		// set interrupt stack index
		reserved = 0;
		set_addr(base);
	}
}__attribute__((packed)) _idt_gate_x64_t; // interrupt/trap gate

union __attribute__((packed)) _sd_x86_t { // segment descriptor (legacy mode) {
	_u64 value;
	struct {
		_u16		limit1;		// segment limit (first part)
		_u16		base1;		// base (first part)
		_u8		base2;		// base (second part)
		_u8		dtype	:5;
		_u8		dpl	:2;
		_u8		present	:1;
		unsigned	limit2	:4;	// limit (second part)
		unsigned	mode	:4;
		_u8		base3;		// base (third part)
	};

	void _OPTIMIZE_SIZE_ set_limit(_u32 limit) {
		limit1 = (_u16)limit;
		limit2 = (_u8)(limit >> 16);
	}

	void _OPTIMIZE_SIZE_ set_base(_u64 base) {
		base1 = (_u16)base;
		base2 = (_u8)(base >> 16);
		base3 = (_u8)(base >> 24);
	}

	void _OPTIMIZE_SIZE_ init(_u8 type, _u8 _dpl, _u8 _mode, _u64 _base, _u32 _limit) {
		dtype = type;
		dpl = _dpl;
		present = 1;
		set_base(_base);
		set_limit(_limit);
		mode = _mode;
	}
};

typedef struct {
	_u16		limit1;		// segment limit
	_u16		base1;		// base address (first part)
	_u8		base2;		// base address (second part)
	_u8		dtype	:5;
	_u8		dpl	:2;
	_u8		present	:1;
	unsigned	limit2	:4;	// segment limit (second part)
	unsigned	mode	:4;
	_u8		base3;		// base address (third part)
	_u32		base4;		// base address (last part)
	_u32		reserved;	// reserved

	void _OPTIMIZE_SIZE_ set_limit(_u32 limit) {
		limit1 = (_u16)limit;
		limit2 = (_u8)(limit >> 16);
	}

	void _OPTIMIZE_SIZE_ set_base(_u64 base) {
		base1 = (_u16)base;
		base2 = (_u8)(base >> 16);
		base3 = (_u8)(base >> 24);
		base4 = (_u32)(base >> 32);
	}

	void _OPTIMIZE_SIZE_ init(_u8 _type, _u8 _dpl, _u8 _mode, _u64 _base, _u32 _limit) {
		dtype = _type;
		dpl = _dpl;
		present = 1;
		set_base(_base);
		set_limit(_limit);
		mode = _mode;
		reserved = 0;
	}
}__attribute__((packed)) _ssd_x64_t; // system segment descriptor (long mode)

typedef struct {
	_u32	reserved1;	// 0
	_u64	rsp[3];		// +4
	_u64	ist[8]; // WARNING: do not use ist[0]
	_u64	reserved2;	// +92
	_u16	reserved3;	// +100
	_u16	io_map_offset;	// +102
}__attribute__((packed)) _tss_x64_t; // Task State Segment

typedef struct {
	_u16	limit;               // The upper 16 bits of all limits.
	_ulong	base;                // The address of the first structure.
}__attribute__((packed)) _dt_ptr_t;

union _cr0 {
	_u64 value;
	struct {
		_u64	pe	:1;	// protection enabled  RW
		_u64	mp	:1;	// Monitor Coprocessor RW
		_u64	em	:1;	// Emulation RW
		_u64	ts	:1;	// Task Switched RW
		_u64	et	:1;	// Extension Type R
		_u64	ne	:1;	// Numeric Error RW
		_u64		:10;	// reserved
		_u64	wp	:1;	// Write Protect RW
		_u64		:1;	// reserved
		_u64	am	:1;	// Alignment Mask RW
		_u64		:10;	// reserved
		_u64	nw	:1;	// Not Writethrough RW
		_u64	cd	:1;	// Cache Disable RW (0:enable/1:disable)
		_u64	pg	:1;	// Paging RW
		_u64		:32;	// reserved
	}__attribute__((packed));

	_cr0(void) { get(); }
	void get(void) {
		__asm__ __volatile__ ("mov	%%cr0, %%rax\n"	: "=a" (value));
	}
	void set(void) {
		__asm__ __volatile__ ("mov	%%rax, %%cr0\n"	:: "a" (value));
	}
};

union _cr4 {
	_u64 value;
	struct {
		_u32	vme	:1; // Virtual-8086 Mode Extensions
		_u32	pmi	:1; // Protected-Mode Virtual Interrupts
		_u32	tsd	:1; // Time Stamp Disable
		_u32	de	:1; // Debugging Extensions
		_u32	pse	:1; // Page Size Extensions
		_u32	pae	:1; // Physical-Address Extension
		_u32	mce	:1; // Machine Check Enable
		_u32	pge	:1; // Page-Global Enable
		_u32	pce	:1; // Performance-Monitoring Counter Enable
		_u32	osfxsr	:1; // Operating System FXSAVE/FXRSTOR Support
		_u32	osx	:1; // Operating System Unmasked Exception Support
		_u32		:21;// reserved
		_u32	reserved;   // reserved
	}__attribute__((packed));

	_cr4(void) { get(); }
	void get(void) {
		__asm__ __volatile__ ("mov	%%cr4, %%rax\n"	: "=a" (value));
	}
	void set(void) {
		__asm__ __volatile__ ("mov	%%rax, %%cr4\n"	:: "a" (value));
	}
};

class cCPU:public iCPU {
public:
	cCPU() {
		m_p_mutex = 0;
		m_p_syslog = 0;
		m_p_idt = 0;
		m_is_cpu_init = false;
		m_is_idle = false;
		m_p_stack = 0;
		m_p_tss = 0;
		m_tss_limit = 0;
		m_p_lapic = 0;
		m_p_vmm = 0;
		m_p_pma = 0;
		m_p_blender = 0;
		m_new_timer_value = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}

	//virtual ~cCPU() {
	//	object_destroy();
	//}

	// iBase
	DECLARE_BASE(cCPU);
	DECLARE_INIT();
	DECLARE_DESTROY();

	bool is_boot(void);
	_u16 cpu_id(void);
	_u16 ccpu_id(void);
	void init(_cpu_init_info_t *p_cpu_info);
	void start(void);
	bool is_init(void) {
		return m_is_cpu_init;
	}
	_u32 context_size(void) { // size of CPU context
		return sizeof(_cpu_state_t);
	}
	iVMM *get_vmm(void) { // return pointer to virtual memory manager
		return m_p_vmm;
	}
	iBlender *get_blender(void) {
		return m_p_blender;
	}
	void set_blender(iBlender *p_blender) {
		m_p_blender = p_blender;
	}
	void halt(void) { HALT }
	// interrupt send
	void interrupt(_u8 in, void *p_cpu_state=0);
	// interrupt callback
	void end_of_interrupt(void);
	bool enable_interrupts(bool enable=true);
	void enable_cache(bool enable=true);
	void set_timer(_u32 countdown);
	_u32 get_timer(void);
	_u64 timestamp(void);
	// per processor initialization (called by global CPU init function)
	void init_cpu(void);
	bool send_init_ipi(_u16 cpu_id, _u32 startup_vector);
	void dump(void *state) { reg_dump(state); }
	// create new executable context in 'p_context' buffer
	void create_context(void *p_stack, _u32 stack_sz, _thread_t *p_entry,
			_u8 dpl, void *p_data,void *p_context);

	// generate interrupt to switch context (depends on scheduling)
	void switch_context(void);
	void copy_cpu_state(void *dst, void *src);
	// get fpu state in 'p_target_store' buffer if available
	// else store fpu state in to internal buffer
	void save_fpu_state(void *p_target_store=0);

	// set fpu state from 'p_fpu_state' buffer if available
	// else set fpu state from internal buffer
	void restore_fpu_state(void *p_fpu_state=0);

protected:
	iMutex		*m_p_mutex;	// local mutex
	_cpu_init_info_t m_cpu_info; 	// MPC CPU info
	iSystemLog 	*m_p_syslog;	// System Log instance
	_idt_gate_x64_t	*m_p_idt;	// Interrupt Descriptor Table
	volatile bool	m_is_cpu_init;	// initialize if true
	volatile bool	m_is_idle;
	_sd_x86_t	m_gdt[MAX_GDT];	// Global Descriptor Table
	_u8		*m_p_stack; 	// CPU stack frame
	_tss_x64_t	*m_p_tss; 	// Task State Segment
	_u32		m_tss_limit;	// TSS limit
	iLocalApic	*m_p_lapic;	// local apic instance
	iVMMx86		*m_p_vmm;	// VMM instance
	iPMA		*m_p_pma;
	_u8		m_fpu_state[512]; // fpu context backup
	iBlender	*m_p_blender;	// instance to task blender
	_u32 		m_new_timer_value;
	_u16		m_sys_code_selector;
	_u16		m_usr_code_selector;
	_u16		m_sys_data_selector;
	_u16		m_usr_data_selector;

	void init_idt(void); // initializing Interrupt Descriptor Table
	void init_lapic(void);
	void init_mm(void);
	void init_stack(void);
	void init_gdt(void);
	void init_tss(void);
	void init_cr4(void);
	void init_blender(void);
	void init_cores(void);
	bool _enable_interrupts(bool enable=true);
	void enable_lapic(bool enable=true);
	void idle(void);
	void save_context(_u8 in, void *p_state);
	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	void reg_dump(void *p_cpu_state);
	_idt_gate_x64_t *get_idt_gate(_u8 index);
	_u16 find_free_selector(void);
};

#endif

