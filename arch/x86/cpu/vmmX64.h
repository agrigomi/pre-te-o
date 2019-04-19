#ifndef __C_VMM_H__
#define __C_VMM_H__

#include "iVMMx86.h"
#include "iSyncObject.h"
#include "iSystemLog.h"
#include "iMemory.h"
#include "pgdef.h"

#ifdef _64_
#define VMM_INVALID_BASE	0xffffffffffffffffLLU
#define VMM_LAST_VADDR_USER	0x00007fffffffffffLLU
#else
#define VMM_INVALID_BASE	0xffffffff
#define VMM_LAST_VADDR_USER	0x7fffffff
#endif

class cVMMx64:public iVMMx86 {
public:
	cVMMx64() {
		m_p_mutex = 0;
		m_p_syslog = 0;
		ERROR_CODE = 0;
		m_mmap = 0;
		m_p_pma = 0;
		m_is_vmm_init = false;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}

	//virtual ~cVMMx64() {}

	// iBase
	DECLARE_BASE(cVMMx64);
	DECLARE_INIT();
	DECLARE_DESTROY();

	// init VMM unit (PER CPU)
	//  if 'remap' parameter is true, the function perform
	//  alocation of new mapping table (copy of passed 'mapping'),
	//  else the object works under map table passed in 'mapping' parameter.
	//  The 'mapping' parameter will be passed as physical address
	void init_vmm(_vaddr_t mapping, _u32 mapping_sz, bool remap=false);
	// activate mapping
	void activate(void);
	// return the virtual address (in kernel space) of mapping table
	void *get_mapping_base(void);
	// map 'npages' passed in 'page_array' at virtual address 'vaddr'
	//  and return the actual number of mapped pages
	_u32 map(_vaddr_t *page_array, _u32 npages, _vaddr_t vaddr, _u8 flags, HMUTEX hlock=0);
	// remove 'npages' from the mapping at start address 'vaddr'
	//  and return the actual number of removed pages
	_u32 unmap(_vaddr_t vaddr_start, _vaddr_t vaddr_end, HMUTEX hlock=0);
	// trace the mapping (if 'mapping' parameter is true) starts from 'vaddr_start' address and
	//  return the  last  available virtual address
	//  otherwise (if 'mapping' parameter is false) the function start trace the unmapped memory region
	_vaddr_t trace(_vaddr_t vaddr_start, bool mapping=true, HMUTEX hlock=0);
	// convert virtual address to physical address
	_vaddr_t physical(_vaddr_t va);
	void *alloc(_u32 mem_sz, _u8 flags, _vaddr_t vaddr_start, _vaddr_t vaddr_end, HMUTEX hlock=0);
	// test mapping of single virtual address
	bool test(_vaddr_t vaddr, HMUTEX hlock=0);
	void dump(_vaddr_t vaddr_start, _vaddr_t vaddr_end);
	iMutex *get_mutex(void);

protected:
	iMutex		*m_p_mutex;
	iSystemLog	*m_p_syslog;
	iPMA		*m_p_pma;
	_ulong		m_mmap; // physical address of mapping table
	bool		m_is_vmm_init;
	bool		m_own_copy;

	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);

	_lm_pml4e_t *get_pml4e(_vaddr_t va);
	_lm_pdpe_t  *get_pdpe(_vaddr_t va, _lm_pml4e_t *p_pml4e=0);
	_lm_pde_t   *get_pde(_vaddr_t va, _lm_pdpe_t *p_pdpe=0);
	_lm_pte_t   *get_pte(_vaddr_t va, _lm_pde_t *p_pde=0);

	_lm_pml4e_t *get_pml4_table(void);
	_lm_pdpe_t  *get_pdp_table(_vaddr_t va, _lm_pml4e_t *p_pml4e=0);
	_lm_pde_t   *get_pd_table(_vaddr_t va, _lm_pdpe_t *p_pdpe=0);
	_lm_pte_t   *get_pt_table(_vaddr_t va, _lm_pde_t *p_pde=0);

	_lm_pdpe_t  *alloc_pdp_table(/*in*/_lm_pml4e_t *p_pml4e);
	_lm_pde_t   *alloc_pd_table(/*in*/_lm_pdpe_t *p_pdpe);
	_lm_pte_t   *alloc_pt_table(/*in*/_lm_pde_t *p_pde);
	bool map_page(_lm_pte_t *p_pte, _vaddr_t page_kva, _u8 flags);
	_u32 _map(_vaddr_t *page_array, _u32 npages, _vaddr_t vaddr, _u8 flags);
	_u32 _unmap(_vaddr_t vaddr_start, _vaddr_t vaddr_end);
	_vaddr_t trace_mapping(_vaddr_t vaddr);
	_vaddr_t trace_hole(_vaddr_t vaddr);
	bool _test(_vaddr_t vaddr);
};

#endif

