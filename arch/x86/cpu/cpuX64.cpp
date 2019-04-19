#include "cpuX64.h"
#include "startup_context.h"
#include "iMemory.h"
#include "str.h"
#include "arch_ifclist.h"
#include "io_port.h"
#include "cpu.h"
#include "intdef.h"
#include "lapic.h"

//#define _DEBUG_
#include "debug.h"

#ifdef _DEBUG_
#define	LOG(_fmt_, ...) \
	DBG(_fmt_, __VA_ARGS__)
#else
#define LOG(_fmt_, ...) \
	if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__)
#endif

extern _startup_context_t *__g_p_startup_context__;
extern void smp_init_cpu(iCPU *p_cpu_connext);

// system log instance, used by exception handlers
static iSystemLog *_g_p_syslog = 0;
// global memory mapping object
static iVMMx86 *_g_p_vmm = 0;

__cdecl _u64 _get_code_selector_(void) {
	return (_u64)(__g_p_startup_context__->cpu_info._cpu._x86.code_selector);
}

IMPLEMENT_BASE(cCPU, "cCPU", 0x0001)

IMPLEMENT_INIT(cCPU, p_repository) {
	bool r = false;

	REPOSITORY = p_repository;

	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_lapic = (iLocalApic *)OBJECT_BY_INTERFACE(I_LOCAL_APIC, RY_STANDALONE);
	if(!_g_p_vmm)
		_g_p_vmm = (iVMMx86 *)OBJECT_BY_INTERFACE(I_VMMX86, RY_CLONE);
	m_p_vmm = _g_p_vmm;
	m_p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
	m_p_blender = (iBlender *)OBJECT_BY_INTERFACE(I_BLENDER, RY_CLONE|RY_STANDALONE);

	m_is_cpu_init = false;
	if(m_p_mutex && m_p_syslog && m_p_lapic && m_p_vmm && m_p_pma) {
		if(!_g_p_syslog)
			_g_p_syslog = m_p_syslog;
		r = true;
	}

	return r;
}

IMPLEMENT_DESTROY(cCPU) {
	RELEASE_OBJECT(m_p_mutex);
	RELEASE_OBJECT(m_p_lapic);
	RELEASE_OBJECT(m_p_syslog);
	RELEASE_OBJECT(m_p_blender);

	if(m_p_pma) {
		// release the stack frame
		m_p_pma->free((void *)m_p_tss->ist[DEFAULT_IST_INDEX], INT_STACK_PAGES);
		// release TSS
		m_p_pma->free(m_p_tss, 2);
		// release RSF
		m_p_pma->free(m_p_stack, STACK_PAGES);
	}
}

void _OPTIMIZE_SIZE_ cCPU::init(_cpu_init_info_t *p_cpu_info) {
	m_cpu_info.id = p_cpu_info->id;
	m_cpu_info.flags = p_cpu_info->flags;
}

bool _OPTIMIZE_SIZE_ cCPU::is_boot(void) {
	bool r = false;

	if((m_cpu_info.flags & CCPUF_BOOT))
		r = true;
	else
		r = false;

	return r;
}

_u16 _OPTIMIZE_SIZE_ cCPU::ccpu_id(void) {
	return (m_p_lapic) ? m_p_lapic->get_id() : 0;
}

_u16 _OPTIMIZE_SIZE_ cCPU::cpu_id(void) {
	return m_cpu_info.id;
}

void _OPTIMIZE_SIZE_ cCPU::start(void) {
	if(!m_is_cpu_init) {
		_enable_interrupts(false);
		init_stack();
		init_mm();
		init_tss();
		init_gdt();
		init_cr4();
		init_idt();
		init_cores();
		init_lapic();
		init_blender();
		m_is_cpu_init = true;// disable reinitialization
		if(!is_boot())
			// the application CPU can enter in idle emediately
			idle();
	}
}

__cdecl void _int_0(void);
__cdecl void _int_1(void);
static _idt_gate_x64_t	*_g_p_idt_ = 0;
static _u16 _g_page_size_ = 0;

#define RAM_LIMIT	p_to_k(0xffffffff)

void _OPTIMIZE_SIZE_ cCPU::init_idt(void) {
	if(!(m_p_idt = _g_p_idt_)) { // this is a first IDT
		if(m_p_pma) {
			// try to allocate one page for IDT
			m_p_pma->alloc(1, (_u64 *)&m_p_idt, RT_SYSTEM, RAM_LIMIT);
			_g_page_size_ = m_p_pma->get_page_size();
		} else {
			LOG("ERROR: No PMA object found\r\n", "");
		}
	}

	if(m_p_idt) {
		DBG("CPU(%d): IDT: 0x%h\r\n", cpu_id(), m_p_idt);
		_u16 idt_sz = _g_page_size_ / sizeof(_idt_gate_x64_t);
		_u16 idt_limit = _g_page_size_ - 1;

		if(!_g_p_idt_) { // first allocation of IDT
			// set interrupt handlers
			_isr_t *int0 = _int_0;
			_isr_t *int1 = _int_1;

			_u8 *i0 = (_u8 *)int0;
			_u8 *i1 = (_u8 *)int1;

			_u32 handler_sz = i1 - i0;
			for(_u32 i = 0; i < idt_sz; i++) {
				_u8 type = INT_64;
				m_p_idt[i].init((_u64)(i0 + (i * handler_sz)),
						__g_p_startup_context__->cpu_info._cpu._x86.code_selector,
						DEFAULT_IST_INDEX, type, DPL_USR);
			}

			// store the alocated IDT pointer to prevent new allocation
		 	_g_p_idt_ = m_p_idt;
		}

		// Load IDTR
		_dt_ptr_t _lidt;
		_lidt.limit = idt_limit;
		_lidt.base = CPU_ADDRESS(m_p_idt);
		__asm__ __volatile__ ("lidt %0" : :"m"(_lidt) );
	} else {
		LOG("ERROR: Can't allocate memory for IDT by CPU(%d)\r\n", cpu_id());
	}
}

_idt_gate_x64_t * _OPTIMIZE_SIZE_ cCPU::get_idt_gate(_u8 index) {
	_idt_gate_x64_t *r = 0;

	if(m_p_idt)
		r = &m_p_idt[index];

	return r;
}

void _OPTIMIZE_SIZE_ cCPU::init_lapic(void) {
	if(m_p_lapic) {
		_u64 apic_base = (_u64)m_p_lapic->get_base();
		if(!m_p_vmm->test(apic_base)) {
			if(m_p_vmm->map(&apic_base, 1, apic_base, VMMF_NON_CACHEABLE) == 1) {
				DBG("CPU(%d): Map local apic at 0x%h\r\n",cpu_id(), apic_base);
			} else {
				DBG("CPU(%d): Failed to map local apic!\r\n",cpu_id());
				return;
			}
		} else {
			DBG("CPU(%d): local apic found at 0x%h\r\n", cpu_id(), apic_base);
		}

		DBG("CPU(%d): APIC id: 0x%x\r\n", cpu_id(), m_p_lapic->get_id());
		DBG("CPU(%d): APIC version: 0x%x\r\n", cpu_id(), m_p_lapic->get_version());

		// initialize local apic registers
		m_p_lapic->init();

		// init spurious interrupt vector
		m_p_lapic->set_sri(INT_LAPIC_SRI);
	}
}

void _OPTIMIZE_NONE_ cCPU::init_stack(void) {
	if(m_p_pma) {
		_ulong r_stack;

		// get real stack pointer
		__asm__ __volatile__ ("movq %%rsp, %0" :"=m"(r_stack):);

		// virtualize
		_u32 nb = __g_p_startup_context__->stack_ptr - r_stack;
		r_stack = p_to_k(r_stack);

		// alloc CPU system stack
		m_p_stack = (_u8 *)m_p_pma->alloc(STACK_PAGES, RT_SYSTEM, RAM_LIMIT);
		if(m_p_stack) {
			// copy from boot stack
			_u8 *stack_ptr = m_p_stack;
			stack_ptr += (STACK_PAGES * m_p_pma->get_page_size()) - 0x10;
			DBG("CPU(%d): RSF: 0x%h\r\n", cpu_id(), stack_ptr);
			stack_ptr -= nb;
			mem_cpy(stack_ptr, (_u8 *)r_stack, nb);
			// activate the new stack
			__asm__ __volatile__ ("movq %0, %%rsp": :"m"(stack_ptr));
		} else {
			LOG("ERROR: Failed to allocate stack frame for CPU(%d)\r\n", cpu_id());
		}
	}
}

void _OPTIMIZE_SIZE_ cCPU::init_tss(void) { // setup Task State Segment
	if(m_p_pma) {
		// allocate memory for TSS
		_u8 *p_tss = (_u8 *)m_p_pma->alloc(2, RT_SYSTEM, RAM_LIMIT);
		if(p_tss) {
			DBG("CPU(%d): TSS: 0x%h\r\n", cpu_id(), p_tss);
			m_tss_limit = (m_p_pma->get_page_size() * 2) - 1;
			mem_set(p_tss, 0, m_p_pma->get_page_size());

			m_p_tss = (_tss_x64_t *)p_tss;

			// allocate interrupt stack frame
			_u64 isf = 0;
			isf = (_u64)m_p_pma->alloc(INT_STACK_PAGES, RT_SYSTEM, RAM_LIMIT);
			if(isf) {
				isf += (INT_STACK_PAGES * m_p_pma->get_page_size()) - 0x10;

				DBG("CPU(%d): ISF: 0x%h\r\n", cpu_id(), isf);
				// assing interrupt stack frame
				m_p_tss->ist[DEFAULT_IST_INDEX] = isf;
				// setting the offset for iomap (behind TSS)
				m_p_tss->io_map_offset = (_u16)(sizeof(_tss_x64_t) + 4);
			} else {
				LOG("ERROR: Failed to allocate interrupt stack frame for CPU(%d)\r\n", cpu_id());
			}
		} else {
			LOG("ERROR: Failed to allocate TSS for CPU(%d)\r\n", cpu_id());
		}
	}
}

_u16 _OPTIMIZE_SIZE_ cCPU::find_free_selector(void) {
	_u16 r = 0;

	for(_u8 i = 1; i < MAX_GDT; i++) {
		if(m_gdt[i].value == 0) {
			r = i;
			break;
		}
	}

	return r;
}

void _OPTIMIZE_SIZE_ cCPU::init_gdt(void) {
	DBG("CPU(%d): GDT: 0x%h\r\n", cpu_id(), m_gdt);
	mem_set((_u8 *)m_gdt, 0, sizeof(m_gdt));

	// init code 64 segment descriptor
	_sd_x86_t *p_cs = &m_gdt[__g_p_startup_context__->cpu_info._cpu._x86.code_selector];
	p_cs->init(CODE_DESC_LM, DPL_SYS, SM_64_G0, 0, 0xffffffff);
	m_sys_code_selector = __g_p_startup_context__->cpu_info._cpu._x86.code_selector;

	// init data 64 segment descriptor
	_sd_x86_t *p_ds = &m_gdt[__g_p_startup_context__->cpu_info._cpu._x86.data_selector];
	p_ds->init(DATA_DESC_LM, DPL_SYS, SM_64_G0, 0, 0xffffffff);
	m_sys_data_selector = __g_p_startup_context__->cpu_info._cpu._x86.data_selector;

	// init TSS record in GDT
	_ssd_x64_t *p_tss = (_ssd_x64_t *)&m_gdt[GDT_TSS_64];
	p_tss->init(TSS_64, DPL_SYS, SM_64_G0, CPU_ADDRESS(m_p_tss), m_tss_limit);

	if((m_usr_code_selector = find_free_selector()))
		m_gdt[m_usr_code_selector].init(CODE_DESC_LM, DPL_USR, SM_64_G0, 0,0xffffffff);
	else
		LOG("No free selector for user code !\r\n","");

	if((m_usr_data_selector = find_free_selector()))
		m_gdt[m_usr_data_selector].init(DATA_DESC_LM, DPL_USR, SM_64_G0, 0,0xffffffff);
	else
		LOG("No free selector for user data !\r\n","");

	// Load GDTR
	_dt_ptr_t _lgdt;
	_lgdt.limit = sizeof(m_gdt)-1;
	_lgdt.base = CPU_ADDRESS(&m_gdt[0]);
	__asm__ __volatile__ ( "lgdt %0" : :"m"(_lgdt) );

	// load task register
	__asm__ __volatile__ (
		"movw	%0, %%ax\n"
		"shlw	$3, %%ax\n"
		"ltr	%%ax\n"
		: :"O"(GDT_TSS_64)
	);
}

void _OPTIMIZE_SIZE_ cCPU::init_cores(void) {
	if(!(m_cpu_info.flags & CCPUF_CORE)) {
		_u8 ncores = cpu_get_cores();
		iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
		if(p_cpu_host) {
			for(_u8 i = 1; i < ncores; i++) {
				p_cpu_host->create_cpu(m_cpu_info.signature, m_cpu_info.id + i,
							CCPUF_ENABLED|CCPUF_CORE);
			}
		}
	}
}

void _OPTIMIZE_SIZE_ cCPU::enable_cache(bool enable) {
	if(cpu_id() == ccpu_id() && is_boot()) {
		HMUTEX hm = lock();

		_cr0 cr0;

		if(enable)
			cr0.cd = 0; // enable
		else
			cr0.cd = 1; // disable

		cr0.set();
		// invalidate cache
		__asm__ __volatile__ ("wbinvd\n");
		m_p_vmm->activate();

		unlock(hm);
	}
}

void _OPTIMIZE_SIZE_ cCPU::init_cr4(void) {
	_cr4 cr4;
	cr4.osfxsr = 1; // enable FPU context save (fxsave/fxrstor)
	cr4.set();
}

void _OPTIMIZE_SIZE_ cCPU::enable_lapic(bool enable) {
	if(m_p_lapic) {
		if(enable)
			m_p_lapic->enable();
		else // disable local apic
			m_p_lapic->disable();
	}
}

bool _OPTIMIZE_NONE_ cCPU::_enable_interrupts(bool enable) {
	_cpu_flags_t flags;

	if(enable) {
		asm volatile("sti": : :"memory");
	} else {
		asm volatile("cli": : :"memory");
	}

	// return prev. int. state
	return (bool)(flags.interrupt);
}

bool _OPTIMIZE_SIZE_ cCPU::enable_interrupts(bool enable) {
	bool r = false;

	if(ccpu_id() == cpu_id())
		r = _enable_interrupts(enable);

	return r;
}

void _OPTIMIZE_SIZE_ cCPU::init_mm(void) { // initialize memory mapping
	if(m_p_vmm) {
		// use the current mapping, only for boot CPU
		// application CPU-s must create own mapping
		m_p_vmm->init_vmm(__g_p_startup_context__->pt_address,
				__g_p_startup_context__->pt_table_size, !is_boot());
		m_p_vmm->activate();
		DBG("CPU(%d): VMM: 0x%h\r\n", cpu_id(), m_p_vmm->get_mapping_base());
	}
}

void _OPTIMIZE_SIZE_ cCPU::init_blender(void) {
	if(!m_p_blender) {
		if((m_p_blender = (iBlender *)OBJECT_BY_INTERFACE(I_BLENDER, RY_CLONE|RY_STANDALONE)))
			m_p_blender->init(cpu_id());
	} else
		m_p_blender->init(cpu_id());
}

HMUTEX _OPTIMIZE_ALL_ cCPU::lock(HMUTEX hlock) {
	HMUTEX r = 0;

	if(m_p_mutex)
		r = m_p_mutex->lock(hlock);

	return r;
}

void _OPTIMIZE_ALL_ cCPU::unlock(HMUTEX hlock) {
	if(m_p_mutex)
		m_p_mutex->unlock(hlock);
}

void _OPTIMIZE_ALL_ cCPU::save_fpu_state(void *p_target_store) {
	// save FPU state
	_u8 fpu[512] __attribute__((aligned(16)));

	__asm__ __volatile__("fxsaveq %0" : "=m" (fpu): : "memory");

	if(p_target_store)
		mem_cpy((_u8 *)p_target_store, (_u8 *)fpu, sizeof(m_fpu_state));
	else
		mem_cpy((_u8 *)m_fpu_state, (_u8 *)fpu, sizeof(m_fpu_state));
}

void _OPTIMIZE_ALL_ cCPU::copy_cpu_state(void *dst, void *src) {
	mem_cpy((_u8 *)dst, (_u8 *)src, sizeof(_cpu_state_t));
}

void  _OPTIMIZE_ALL_ cCPU::restore_fpu_state(void *p_fpu_state) {
	// save FPU state
	_u8 fpu[512] __attribute__((aligned(16)));

	if(p_fpu_state)
		mem_cpy(fpu, (_u8 *)p_fpu_state, sizeof(fpu));
	else
		mem_cpy(fpu, m_fpu_state, sizeof(fpu));

	__asm__ __volatile__("fxrstorq %0" :: "m" (*(const _u8 *)fpu) : "memory");
}

#define SHD_EXCEPTION	1
#define SHD_CTX_SWITCH	2
#define SHD_DEBUG	3
#define SHD_BREAKPOINT	4
#define SHD_INTERRUPT	5
#define SHD_MEMORY	6
#define SHD_TIMER	7

void _OPTIMIZE_ALL_ cCPU::interrupt(_u8 in, void *p_cpu_state) {
	_u8 type = 0;

	if((cpu_id() != ccpu_id()) || p_cpu_state == 0)
		m_p_lapic->send_ipi(cpu_id(), in);
	else {
#ifdef _DEBUG_
		if(!((_cpu_state_t *)p_cpu_state)->rflags & FLAG_I)
			DBG("Nested interrupt: %d\r\n", in);
#endif

		switch(in) {
			case EX_DE: //Divide-by-Zero-Error
				DBG("CPU(%d): [#DE-0]\r\n",cpu_id());
#ifdef _DEBUG_
				reg_dump(p_cpu_state);
#endif
				type = SHD_EXCEPTION;
				break;
			case EX_DB:// Debug
				DBG("CPU(%d): [#DB-1]\r\n",cpu_id());
				type = SHD_DEBUG;
				break;
			case EX_NMI:// Non-Maskable-Interrupt
				DBG("CPU(%d): [#NMI-2]\r\n",cpu_id());
				type = SHD_EXCEPTION;
				break;
			case EX_BP:// Breakpoint
				DBG("CPU(%d): [#BP-3]\r\n",cpu_id());
				type = SHD_BREAKPOINT;
				break;
			case EX_OF:// Overflow
				DBG("CPU(%d): [#OF-4]\r\n",cpu_id());
#ifdef _DEBUG_

				reg_dump(p_cpu_state);
#endif
				type = SHD_EXCEPTION;
				break;
			case EX_BR:// Bound-Range
				DBG("CPU(%d): [#BR-5]\r\n",cpu_id());
				type = SHD_EXCEPTION;
				break;
			case EX_DU:// Invalid-Opcode
				DBG("CPU(%d): [#DU-6]\r\n",cpu_id());
#ifdef _DEBUG_
				reg_dump(p_cpu_state);
#endif
				type = SHD_EXCEPTION;
				break;
			case EX_NM:// Device-Not-Available
				DBG("CPU(%d): [#NM-7]\r\n",cpu_id());
				type = SHD_EXCEPTION;
				break;
			case EX_DF:// Double-Fault
				DBG("CPU(%d): [#DF-8]\r\n",cpu_id());
				type = SHD_CTX_SWITCH;
				break;
			case EX_TS:// Invalid-TSS
				DBG("CPU(%d): [#TS-10]\r\n",cpu_id());
#ifdef _DEBUG_
				reg_dump(p_cpu_state);
#endif
				type = SHD_EXCEPTION;
				break;
			case EX_NP:// Segment-Not-Present
				DBG("CPU(%d): [#NP-11]\r\n",cpu_id());
				type = SHD_MEMORY;
				break;
			case EX_SS:// Stack
				DBG("CPU(%d): [#SS-12]\r\n",cpu_id());
				type = SHD_EXCEPTION;
				break;
			case EX_GP:// General-Protection
				DBG("CPU(%d): [#GP-13]\r\n",cpu_id());
#ifdef _DEBUG_
				reg_dump(p_cpu_state);
				HALT
#endif
				type = SHD_EXCEPTION;
				break;
			case EX_PF: {// Page-Fault
					DBG("CPU(%d): [#PF-14]\r\n",cpu_id());
					DBG("VBASE: 0x%h\r\n", m_p_vmm->get_mapping_base());
#ifdef _DEBUG_
					reg_dump(p_cpu_state);
					m_p_vmm->dump(0xffffff8000000000, 0xffffffffffffffff);
					HALT
#endif
					type = SHD_MEMORY;
				}
				break;
			case EX_MF:// x87 Floating-Point Exception
				DBG("CPU(%d): [#MF-16]\r\n",cpu_id());
				type = SHD_EXCEPTION;
				break;
			case EX_AC:// Alignment-Check
				DBG("CPU(%d): [#AC-17]\r\n",cpu_id());
				type = SHD_EXCEPTION;
				break;
			case EX_MC:// Machine-Check
				DBG("CPU(%d): [#MC-18]\r\n",cpu_id());
				break;
			case EX_XF:// SIMD Floating-Point
				DBG("CPU(%d): [#XF-19]\r\n",cpu_id());
				type = SHD_EXCEPTION;
				break;
			case EX_SX:// Security Exception
				DBG("CPU(%d): [#SX-30]\r\n",cpu_id());
				type = SHD_EXCEPTION;
				break;
			case INT_LAPIC_TIMER: //local apic timer
				type = SHD_TIMER;
				break;
			case INT_LAPIC_SRI:
				DBG("CPU(%d): [APIC SRI-%d]\r\n", cpu_id(), LAPIC_SRI);
				type = SHD_EXCEPTION;
				break;
			case INT_SYSCALL:
				DBG("CPU(%d): [SYSCALL-128]\r\n",cpu_id());
				type = SHD_CTX_SWITCH; // TEMPORARY
				break;
			case INT_TASK_SWITCH:
				type = SHD_CTX_SWITCH;
				break;
			default:
				DBG("CPU(%d): [Unhandled interrupt-%d]\r\n", cpu_id(), in);
				type = SHD_CTX_SWITCH;
		}

		if((m_p_blender) && m_p_blender->is_running()) {
			switch(type) {
				case SHD_CTX_SWITCH:
					m_p_blender->switch_context(p_cpu_state);
					break;
				case SHD_EXCEPTION:
					m_p_blender->exception(in, p_cpu_state);
					break;
				case SHD_MEMORY: {
						_ulong ex_point;
						__asm__ __volatile__ (
							"movq	%%cr2, %%rax\n"
							:"=a"(ex_point) :
						);
						m_p_blender->memory_exception(ex_point, p_cpu_state);
					}
					break;
				case SHD_BREAKPOINT:
					m_p_blender->breakpoint(p_cpu_state);
					break;
				case SHD_DEBUG:
					m_p_blender->debug(p_cpu_state);
					break;
				case SHD_TIMER:
					m_p_blender->timer(p_cpu_state);
					break;
			}
		}
	}
}

void _OPTIMIZE_SIZE_ cCPU::end_of_interrupt(void) {
	m_p_lapic->end_of_interrupt();
}

bool _OPTIMIZE_SIZE_ cCPU::send_init_ipi(_u16 cpu_id, _u32 startup_vector) {
	bool r = false;

	if(m_p_lapic)
		r = m_p_lapic->send_init_ipi(cpu_id, startup_vector);

	return r;
}

void _OPTIMIZE_SIZE_ cCPU::set_timer(_u32 countdown) {
	if(m_p_lapic->get_id() == cpu_id()) {
		m_p_lapic->set_timer(INT_LAPIC_TIMER, countdown, APIC_TIMER_PERIODIC);
	} else
		m_new_timer_value = countdown;
}

_u32 _OPTIMIZE_ALL_ cCPU::get_timer(void) {
	_u32 r = 0;
	if(m_p_lapic->get_id() == cpu_id())
		r = m_p_lapic->get_timer_count();
	return r;
}

_u64 _OPTIMIZE_NONE_ cCPU::timestamp(void) {
	_u64 r = 0;
	bool irqs = _enable_interrupts(false);
	__asm__ __volatile__ (
		"xorq	%%rdx, %%rdx\n"
		"xorq	%%rax, %%rax\n"
		"rdtsc\n"
		"shlq	$32, %%rdx\n"
		"orq	%%rdx, %%rax\n"
		"movq	%%rax, %[out]\n"
		:[out] "=m" (r)
	);
	_enable_interrupts(irqs);
	return r;
}

void _OPTIMIZE_ALL_ cCPU::idle(void) {
	if(m_p_lapic->get_id() == cpu_id() && !m_is_idle) {
		DBG("CPU(%d): enter idle\r\n", cpu_id());

		m_is_idle = true;
		enable_cache(true);
		enable_lapic(true);
		_enable_interrupts(true);
		DBG("CPU(%d)---\r\n",cpu_id());

		while(1) {
			bool irqs = _enable_interrupts(false);

			if(m_new_timer_value) {
				DBG("CPU(%d) adjust timer with 0x%x\r\n",  cpu_id(), m_new_timer_value);
				set_timer(m_new_timer_value);
				m_new_timer_value = 0;
			}

			if(m_p_blender) {
				if(!m_p_blender->is_running())
					m_p_blender->start();
				else
					m_p_blender->idle();
			} else
				init_blender();

			_enable_interrupts(irqs);

			HALT
		}
	}
}

void _OPTIMIZE_SIZE_ cCPU::create_context(void *p_stack, _u32 stack_sz, _thread_t *p_entry,
				_u8 dpl, void *p_data,void *p_context) {
	_cpu_state_t *cpu_state = (_cpu_state_t *)p_context;

	// set entry point
	cpu_state->rip = (_u64)p_entry;
	cpu_state->rip_frame = (_u64)p_entry;
	////////////////////

	// align stack
	_u64 stack = (_u64)p_stack;
	stack += stack_sz-1;
	while(stack % 16)
		stack--;

	_u64 *_stack = (_u64 *)stack;

	// push data pointer
	_stack--;
	*_stack = (_u64)p_data;
	cpu_state->rdi = (_u64)p_data; // according fast call
	//////////////////////

	// set stack frame
	cpu_state->rsp = (_u64)_stack;
	cpu_state->ss = 0;

	if(dpl == DPL_SYS)
		cpu_state->cs = m_sys_code_selector << 3;
	else
		cpu_state->cs = m_usr_code_selector << 3;

	cpu_state->rflags = FLAG_I;
}

void _OPTIMIZE_NONE_ cCPU::switch_context(void) {
	__asm__ __volatile__ ("int $0xff");
}

void _OPTIMIZE_SIZE_ cCPU::reg_dump(void *cpu_state) {
	if(_g_p_syslog) {
		_cpu_state_t *p_cpu_state = (_cpu_state_t *)cpu_state;

		HMUTEX hm = _g_p_syslog->lock();
		_g_p_syslog->fwrite(hm, "CPU: %d  REGISTERS:\r\n", cpu_id());
		_g_p_syslog->fwrite(hm, "RF : %b\r\n", p_cpu_state->rflags);
		_g_p_syslog->fwrite(hm, "RIP: 0x%H\r\n", p_cpu_state->rip);
		_g_p_syslog->fwrite(hm, "CS : 0x%H\r\n", p_cpu_state->cs);
		_g_p_syslog->fwrite(hm, "RSP: 0x%H\r\n", p_cpu_state->rsp);
		_g_p_syslog->fwrite(hm, "RBP: 0x%H\r\n", p_cpu_state->rbp);
		_g_p_syslog->fwrite(hm, "RAX: 0x%H\r\n", p_cpu_state->rax);
		_g_p_syslog->fwrite(hm, "RBX: 0x%H\r\n", p_cpu_state->rbx);
		_g_p_syslog->fwrite(hm, "RCX: 0x%H\r\n", p_cpu_state->rcx);
		_g_p_syslog->fwrite(hm, "RDX: 0x%H\r\n", p_cpu_state->rdx);
		_g_p_syslog->fwrite(hm, "RSI: 0x%H\r\n", p_cpu_state->rsi);
		_g_p_syslog->fwrite(hm, "RDI: 0x%H\r\n", p_cpu_state->rdi);
		_g_p_syslog->unlock(hm);
	}
}

