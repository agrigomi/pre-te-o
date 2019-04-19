#ifndef __CPU_HOST_H__
#define __CPU_HOST_H__

#include "iCPUHost.h"
#include "iMemory.h"
#include "iSystemLog.h"
#include "mp_scan.h"
#include "iIOApic.h"
#include "str.h"

#define MAX_CORES_PER_CPU	16


__cdecl void cmp_init_cpu(iCPU *p_cpu);
__cdecl void interrupt_dispatcher(_u64 inum, void *p_cpu_state);

typedef struct {
	_int_callback_t *p_isr;
	void *p_udata;
}_isr_info_t;

class cCPUHost:public iCPUHost {
public:
	cCPUHost() {
		m_cpu_count = 0;
		m_p_heap = 0;
		m_p_cpu_list = 0;
		m_p_mpf = 0;
		m_p_ioapic = 0;
		m_ready = false;
		str_cpy((_str_t)_d_ident_, (_str_t)"CPU Host", sizeof(_d_ident_));
		DRIVER_TYPE = DTYPE_VBUS;
		DRIVER_CLASS = DCLASS_SYS;

		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}

	//virtual ~cCPUHost() {
	//	CALL_DESTROY();
	//}

	// iBase
	DECLARE_BASE(cCPUHost);
	DECLARE_INIT();
	DECLARE_DESTROY();

	// iDriver
	DECLARE_DRIVER_INIT();

	iCPU *create_cpu(_u32 signature, _u16 id, _u8 flags);
	iCPU *get_cpu(_u16 id);
	iCPU *get_cpu(void);
	iCPU *get_boot_cpu(void);
	_u16 get_cpu_count(void) {
		return m_cpu_count;
	}
	bool is_running(void) {
		return m_ready;
	}
	void start(void);
	_int_callback_t *get_isr(_u8 num);
	void *get_isr_data(_u8 num);
	void set_isr(_u8 num, _int_callback_t *p_isr, void *p_data=0);
	bool set_irq(_u8 irq, _u16 dst, _u8 vector, _u32 flags=0);

protected:
	iHeap	*m_p_heap;
	iSystemLog *m_p_syslog;
	_mp_t	*m_p_mpf;
	_u16	m_cpu_count;
	_u64	*m_p_cpu_list;
	_u32 	m_sz_cpu_list;
	iIOApic	*m_p_ioapic;	// IO APIC instance
	bool	m_ready;
	_isr_info_t m_isr_map[256];

	void init_ioapic(void);
	iCPU *get_cpu_by_index(_u16 idx);
	void driver_uninit(void) {}
};

extern iCPUHost *_g_p_cpu_host_;

#endif

