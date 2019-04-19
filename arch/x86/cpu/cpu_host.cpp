#include "cpu_host.h"
#include "msr.h"
#include "startup_context.h"
#include "str.h"
#include "io_port.h"
#include "cpu.h"
#include "iDisplay.h"
#include "iCMOS.h"

IMPLEMENT_BASE(cCPUHost, "cCPUHost", 0x0001)

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

#define RAM_LIMIT	p_to_k(0xffffffff)

extern _startup_context_t *__g_p_startup_context__;
static _startup_context_t __g_startup_context__;

// global pointer to CPU Host (very important)
iCPUHost *_g_p_cpu_host_ = 0;
// system log instance, used  in interrupt context
static iSystemLog *_g_p_syslog = 0;


IMPLEMENT_INIT(cCPUHost, p_repository) {
	bool r = false;

	REPOSITORY = p_repository; //store the repository pointer

	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
	_g_p_cpu_host_ = this;
	_g_p_syslog = m_p_syslog;
	m_cpu_count = 0;
	mem_set((_u8 *)&m_isr_map, 0, sizeof(m_isr_map));
	INIT_MUTEX();
	if(m_p_syslog && m_p_heap) {
		REGISTER_DRIVER();
		r = true;
	}

	return r;
}

IMPLEMENT_DRIVER_INIT(cCPUHost, host) {
	bool r = false;

	DRIVER_HOST = host;

	iDriverRoot *p_dh = GET_DRIVER_ROOT();
	if(p_dh) {
		_driver_request_t dr;
		dr.flags = DRF_INTERFACE;
		dr._d_interface_ = I_IO_APIC;

		m_p_ioapic = (iIOApic *)p_dh->driver_request(&dr);
		if(!m_p_ioapic)
			m_p_ioapic = (iIOApic *)OBJECT_BY_INTERFACE(I_IO_APIC, RY_STANDALONE);

		if(!DRIVER_HOST)
			DRIVER_HOST = m_p_ioapic;

		RELEASE_OBJECT(p_dh);
	}

	if(DRIVER_HOST && m_p_ioapic) {
		start();
		r = true;
	}

	return r;
}

IMPLEMENT_DESTROY(cCPUHost) {
	for(_u16 i = 0; i < m_cpu_count; i++) {
		iCPU *p_cpu = (iCPU *)m_p_cpu_list[i];
		RELEASE_OBJECT(p_cpu);
		m_cpu_count--;
	}

	m_p_heap->free(m_p_cpu_list, m_sz_cpu_list);
	RELEASE_MUTEX();
	RELEASE_OBJECT(m_p_syslog);
	RELEASE_OBJECT(m_p_heap);
}

iCPU * _OPTIMIZE_SIZE_ cCPUHost::create_cpu(_u32 signature, _u16 id, _u8 flags) {
	iCPU *r	= 0;
	if(!get_cpu(id)) {
		if((r = (iCPU *)RAM_LIMITED_OBJECT_BY_INTERFACE(I_CPU, RY_CLONE, RAM_LIMIT))) {
			DBG("create CPU(%d) at 0x%h; init_flags=0x%x\r\n", id, r, flags);
			_cpu_init_info_t cinfo;
			cinfo.signature = signature;
			cinfo.id = id;
			cinfo.flags = flags;
			r->init(&cinfo);
			m_p_cpu_list[m_cpu_count] = (_u64)r;
			m_cpu_count++;
		}
	}

	return r;
}

iCPU * _OPTIMIZE_ALL_ cCPUHost::get_cpu(_u16 id) {
	iCPU *r = 0;

	if(m_p_cpu_list) {
		for(_u16 i = 0; i < m_cpu_count; i++) {
			iCPU *p_cpu = (iCPU *)m_p_cpu_list[i];
			if(p_cpu) {
				if(p_cpu->cpu_id() == id) {
					r = (iCPU *)m_p_cpu_list[i];
					break;
				}
			}
		}
	}

	return r;
}

iCPU * _OPTIMIZE_ALL_ cCPUHost::get_cpu(void) {
	iCPU *r = 0;

	if(m_p_cpu_list) {
		_u16 ccpu_id = 0;
		iCPU *p_cpu = (iCPU *)m_p_cpu_list[0];
		if(p_cpu)
			ccpu_id = p_cpu->ccpu_id();

		r = get_cpu(ccpu_id);
	}

	return r;
}

iCPU *_OPTIMIZE_ALL_ cCPUHost::get_cpu_by_index(_u16 idx) {
	iCPU *r = 0;

	if(idx < m_cpu_count)
		r = (iCPU *)m_p_cpu_list[idx];

	return r;
}

iCPU *_OPTIMIZE_ALL_ cCPUHost::get_boot_cpu(void) {
	iCPU *r = 0;

	for(_u16 i = 0; i < m_cpu_count; i++) {
		iCPU *_r = (iCPU *)m_p_cpu_list[i];
		if(_r) {
			if(_r->is_boot()) {
				r = _r;
				break;
			}
		}
	}

	return r;
}

_int_callback_t *_OPTIMIZE_ALL_ cCPUHost::get_isr(_u8 num) {
	return m_isr_map[num].p_isr;
}

void *_OPTIMIZE_ALL_ cCPUHost::get_isr_data(_u8 num) {
	return m_isr_map[num].p_udata;
}

void _OPTIMIZE_ALL_ cCPUHost::set_isr(_u8 num, _int_callback_t *p_isr, void *p_data) {
	m_isr_map[num].p_isr = p_isr;
	m_isr_map[num].p_udata = p_data;
}

bool _OPTIMIZE_SIZE_ cCPUHost::set_irq(_u8 irq, _u16 dst, _u8 vector, _u32 flags) {
	bool r = false;
	if(m_p_ioapic && m_ready) {
		m_p_ioapic->set_irq(irq, dst, vector, flags);
		r = true;
	}
	return r;
}

__cdecl void _OPTIMIZE_ALL_ interrupt_dispatcher(_u64 inum, void *p_cpu_state) {
	iCPU *p_cpu = _g_p_cpu_host_->get_cpu();
	if(p_cpu) {
		_int_callback_t *p_isr = _g_p_cpu_host_->get_isr((_u8)inum);
		if(p_isr) // call interrupt handler
			p_isr(p_cpu, p_cpu_state, _g_p_cpu_host_->get_isr_data((_u8)inum));
		else // call the default handler
			p_cpu->interrupt((_u8)inum, p_cpu_state);

		p_cpu->end_of_interrupt();
	}
}


void _OPTIMIZE_SIZE_ cCPUHost::init_ioapic(void) {
	if(m_p_ioapic) {
		DBG("Init IO APIC\r\n","");
		if(m_p_ioapic->get_flags() & 1) {
			_u64 ioapic_base = (_u64)m_p_ioapic->get_base();
			iVMM *p_vmm = get_boot_cpu()->get_vmm();
			if(p_vmm) {
				if(!p_vmm->test(ioapic_base)) {
					if(p_vmm->map(&ioapic_base, 1, ioapic_base, VMMF_NON_CACHEABLE) == 1) {
						DBG("map IOAPIC at 0x%h\r\n", ioapic_base);
					} else {
						LOG("Failed to map IOAPIC\r\n","");
					}
				} else {
					DBG("IOAPIC base: 0x%h\r\n", ioapic_base);
				}
			}

			DBG("IOAPIC id: %d\r\nIOAPIC version: 0x%x\r\n",
					m_p_ioapic->get_id(), m_p_ioapic->get_version());

		} else {
			LOG("IOAPIC not enabled\r\n", "");
		}
	}
}

__cdecl void _OPTIMIZE_SIZE_ smp_init_cpu(iCPU *p_cpu_context) {
	iCPU *p_cpu = ((_u64)p_cpu_context & __g_p_startup_context__->vbase) ?
			(iCPU *)p_cpu_context :
			(iCPU *)p_to_k(p_cpu_context);

	p_cpu->start();
}

void _OPTIMIZE_SIZE_ cCPUHost::start(void) {
	LOG("Start CPU host ...\r\n","");

	// number of cores per system
	_u16 ncpu = 0;
	iTextDisplay *p_disp = (iTextDisplay *)OBJECT_BY_INTERFACE(I_TEXT_DISPLAY, RY_STANDALONE);

	if((m_p_mpf = mp_find_table())) {
		DBG("Found MP table at 0x%h\r\n", m_p_mpf);
		// enumerating CPU-s
		ncpu = mp_cpu_count(m_p_mpf);
		LOG("(%d) CPU sockets found\r\n", ncpu);
		if(ncpu) {
			_u8 ncores = cpu_get_cores();
			m_sz_cpu_list = ncpu * ncores * sizeof(_u64);
			m_p_cpu_list = (_u64 *)m_p_heap->alloc(m_sz_cpu_list);
			mem_set((_u8 *)m_p_cpu_list, 0, m_sz_cpu_list);
			_mpc_cpu_t *p_cpu_info = 0;
			_u16 cpu_idx = 0;
			LOG("(%d) Cores per CPU found\r\n", ncores);

			while((p_cpu_info = mp_get_cpu(m_p_mpf, cpu_idx))) {
				if(p_cpu_info->flags & CPU_ENABLED) { // use only enabled CPU-s
					LOG("CPU(%d) sign=%X flags=%X apic_id=%d\r\n", (_u32)p_cpu_info->lapic_id,
						(_u32)p_cpu_info->sign, (_u32)p_cpu_info->flags,
						(_u32)p_cpu_info->lapic_id);
					if(ncores > 1) {
						_u16 apic = p_cpu_info->lapic_id;
						_u8 flags = p_cpu_info->flags|CCPUF_CORE;

						for(; apic < p_cpu_info->lapic_id + ncores; apic++) {
							create_cpu(p_cpu_info->sign, apic, flags);
							// remove boot flag
							flags &= ~CCPUF_BOOT;
						}
					} else
						create_cpu(p_cpu_info->sign, p_cpu_info->lapic_id,
								p_cpu_info->flags);
				} else {
					LOG("Skip not enabled CPU(%d)\r\n", p_cpu_info->lapic_id);
				}

				cpu_idx++;
			}
			DBG("%d CPU created\r\n", m_cpu_count);
		} else {
			LOG("Can't find CPU record in MPF\r\n", "");
		}
	} else { // perform initialization of single processor system
		DBG("MPF table not found\r\n", "");
		_u8 ncores = cpu_get_cores();
		m_sz_cpu_list = ncores * sizeof(_u64);
		m_p_cpu_list = (_u64 *)m_p_heap->alloc(m_sz_cpu_list);
		mem_set((_u8 *)m_p_cpu_list, 0, m_sz_cpu_list);
		create_cpu(0, 0, CCPUF_BOOT|CCPUF_ENABLED);
		ncpu = 1;
	}

	DBG("(%d) Work threads\r\n", m_cpu_count);

	iCPU *p_boot_cpu = get_boot_cpu();
	if(p_boot_cpu) {
		LOG("Init boot CPU(%d)\r\n", p_boot_cpu->cpu_id());
		smp_init_cpu(p_boot_cpu);
	} else {
		p_disp->fwrite("No boot CPU !!!\r\n","");
		__asm__ __volatile__("hlt");
	}

	init_ioapic();

	iCMOS *p_cmos = (iCMOS *)OBJECT_BY_INTERFACE(I_CMOS, RY_STANDALONE);
	if(p_cmos)
		p_cmos->write(0x0f, 0x0a); // JMP double word pointer without EOI

	// Set warm-boot vector segment
	*(_u16 *)(p_to_k((0x467 + 0))) = 0;
	// Set warm-boot vector offset
	*(_u16 *)(p_to_k((0x467 + 2))) = (_u16)__g_p_startup_context__->cpu_info._cpu._x86.cpu_init_vector_rm;

	// set core CPU init vector
	__g_p_startup_context__->cpu_info._cpu._x86.core_cpu_init_vector = (_u32)k_to_p((_u64)smp_init_cpu);


	_u16 cpu_count = m_cpu_count;
	_u16 _cpu_count = 1;
	for(_u16 icpu = 0; icpu < cpu_count; icpu++) {
		iCPU *p_cpu = get_cpu_by_index(icpu);
		if(p_cpu) {
			if(!p_cpu->is_boot()) { // skip the boot CPU
				// set core CPU init parameter
				p_disp->fwrite("wake up CPU %d", p_cpu->cpu_id());
				__g_p_startup_context__->cpu_info._cpu._x86.core_cpu_init_data =
							(_u32)k_to_p((_u64)p_cpu);
				if(p_boot_cpu->send_init_ipi(p_cpu->cpu_id(),
						__g_p_startup_context__->cpu_info._cpu._x86.cpu_init_vector_rm)) {
#ifdef _DEBUG_
					_u32 tmout = 0x04c00000;
#else
					_u32 tmout = 0x02000000;
#endif
					while(p_cpu->is_init() == false && tmout)
						tmout--;
					if(tmout == 0 && p_cpu->is_init() == false) {
						DBG("  Release CPU(%d)", p_cpu->cpu_id());
						RELEASE_OBJECT(p_cpu);
						m_p_cpu_list[icpu] = 0;
						p_disp->write((_u8 *)" No",3);
					} else {
						_cpu_count++;
						p_disp->write((_u8 *)" Yes", 4);
					}
				} else {
					p_disp->write((_u8 *)" Failed send IPI", 16);
				}

				p_disp->write((_u8 *)"\r\n", 2);
			}
		}
	}
	p_disp->write((_u8 *)"\r\n",2);

	LOG("CPU ","");
	for(_u16 icpu = 0; icpu < cpu_count; icpu++) {
		iCPU *p_cpu = get_cpu_by_index(icpu);
		if(p_cpu) {
			if(p_cpu->is_init() == false) {
				LOG("%d-", p_cpu->cpu_id());
				DBG("Release CPU(%d)\r\n", p_cpu->cpu_id());
				RELEASE_OBJECT(p_cpu);
				m_p_cpu_list[icpu] = 0;
				m_cpu_count--;
			} else
				LOG("%d.", p_cpu->cpu_id());
		}
	}
	LOG("\r\n","");
	m_cpu_count = _cpu_count;
	// copy startup context
	_u8 *sc_src = (_u8 *)__g_p_startup_context__;
	_u8 *sc_dst = (_u8 *)&__g_startup_context__;
	mem_cpy(sc_dst, sc_src, sizeof(_startup_context_t));
	__g_p_startup_context__ = &__g_startup_context__;
	DBG("CPU Host done\r\n", "");
	m_ready = true;
}


