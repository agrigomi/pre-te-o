#include "vmmX64.h"
#include "addr.h"
#include "startup_context.h"
#include "str.h"


#define MAX_1G_OFFSET			 0x000000003fffffffLLU
#define MAX_2M_OFFSET			 0x00000000001fffffLLU
#define MAX_4K_OFFSET			 0x0000000000000fffLLU

#define PML4_INDEX(addr)  (_u16)((addr & 0x0000ff8000000000LLU) >> 39)
#define PDP_INDEX(addr)   (_u16)((addr & 0x0000007fc0000000LLU) >> 30)
#define PD_INDEX(addr)    (_u16)((addr & 0x000000003fe00000LLU) >> 21)
#define PT_INDEX(addr)    (_u16)((addr & 0x00000000001ff000LLU) >> 12)
#define PT4K_OFFSET(addr) (_u32)( addr & MAX_4K_OFFSET)
#define PT2M_OFFSET(addr) (_u32)( addr & MAX_2M_OFFSET)
#define PT1G_OFFSET(addr) (_u32)( addr & MAX_1G_OFFSET)

#define EPT_BIT(entry)    	(entry & 0x0000000000000080LLU) // end page translation bit (7)

#define MAX_PT_INDEX			 0x00000000000001ffLLU
#define CANONICAL_MASK			 0xffff000000000000LLU
#define CANONICAL_SIGN			 0x0000800000000000LLU

#define PML4_ADDR(index)  (((_u64)index & MAX_PT_INDEX) << 39)
#define PDP_ADDR(index)   (((_u64)index & MAX_PT_INDEX) << 30)
#define PD_ADDR(index)    (((_u64)index & MAX_PT_INDEX) << 21)
#define PT_ADDR(index)    (((_u64)index & MAX_PT_INDEX) << 12)

// page number to kernel virtual address
#define PN_TO_KVA(npage) p_to_k(PML4_4KNUM2PAGE(npage))

#define MK_VADDR_1G(ipml4, ipdp, offset) ( \
	((PML4_ADDR(ipml4) & CANONICAL_SIGN) ? (PML4_ADDR(ipml4) | CANONICAL_MASK) : PML4_ADDR(ipml4)) | \
	PDP_ADDR(ipdp) | ((_u64)offset & MAX_1G_OFFSET) )

#define MK_VADDR_2M(ipml4, ipdp, ipd, offset) ( \
	((PML4_ADDR(ipml4) & CANONICAL_SIGN) ? (PML4_ADDR(ipml4) | CANONICAL_MASK) : PML4_ADDR(ipml4)) | \
	PDP_ADDR(ipdp) | PD_ADDR(ipd) | ((_u64)offset & MAX_2M_OFFSET) )

#define MK_VADDR_4K(ipml4, ipdp, ipd, ipt, offset) ( \
	((PML4_ADDR(ipml4) & CANONICAL_SIGN) ? (PML4_ADDR(ipml4) | CANONICAL_MASK) : PML4_ADDR(ipml4)) | \
	PDP_ADDR(ipdp) | PD_ADDR(ipd) | PT_ADDR(ipt) | ((_u64)offset & MAX_4K_OFFSET) )

//#define _DEBUG_
#include "debug.h"

#define LOG(_fmt_, ...) \
	  if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__)

#define VMM_INVALID_PT_INDEX	0xffff
#define VMM_ALLOC_PORTION	128

IMPLEMENT_BASE(cVMMx64, "cVMMx86", 0x0001);
IMPLEMENT_INIT(cVMMx64, p_repository) {
	bool r = false;

	REPOSITORY = p_repository;

	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);

	if(m_p_mutex && m_p_pma) {
		r = true;
	}

	return r;
}
IMPLEMENT_DESTROY(cVMMx64) {
	RELEASE_OBJECT(m_p_mutex);
	RELEASE_OBJECT(m_p_syslog);
	RELEASE_OBJECT(m_p_pma);
}

// init VMM unit (PER CPU)
//  if 'remap' parameter is true, the function perform
//  alocation of new mapping table (copy of passed 'mapping'),
//  else the object works under map table passed in 'mapping' parameter.
// WARNING: The 'mapping' parameter must be passed as physical address
void cVMMx64::init_vmm(_vaddr_t mapping, _u32 _UNUSED_ mapping_sz, bool remap) {
	//if(remap) {
		// create new mapping table (copy of boot mapping)
		//???
	//} else
		// use the current mapping
		m_mmap = mapping;

	// disable reinitialization
	m_is_vmm_init = true;
	m_own_copy = remap;
}

void cVMMx64::activate(void) {
	// activate mapping table
	__asm__ __volatile__ (
		"mov	%%rax, %%cr3\n"
		:
		:"a"(m_mmap)
	);
}

void *cVMMx64::get_mapping_base(void) {
	return (void *)p_to_k(m_mmap);
}

HMUTEX cVMMx64::lock(HMUTEX hlock) {
	HMUTEX r = 0;

	if(m_p_mutex)
		r = m_p_mutex->lock(hlock);

	return r;
}

void cVMMx64::unlock(HMUTEX hlock) {
	if(m_p_mutex)
		m_p_mutex->unlock(hlock);
}

iMutex *cVMMx64::get_mutex(void) {
	return m_p_mutex;
}

_lm_pml4e_t *cVMMx64::get_pml4e(_vaddr_t va) {
	_lm_pml4e_t *r = 0;

	if(m_is_vmm_init && m_mmap) {
		r = (_lm_pml4e_t *)p_to_k(m_mmap);
		r += PML4_INDEX(va);
	}

	return r;
}

_lm_pdpe_t *cVMMx64::get_pdpe(_vaddr_t va, _lm_pml4e_t *p_pml4e) {
	_lm_pdpe_t *r = 0;

	_lm_pml4e_t *_p_pml4e = (p_pml4e) ? p_pml4e : get_pml4e(va);
	if(_p_pml4e) {
		r = (_lm_pdpe_t *)p_to_k(PML4_4KNUM2PAGE(_p_pml4e->f.base));
		r += PDP_INDEX(va);
	}

	return r;
}

_lm_pde_t *cVMMx64::get_pde(_vaddr_t va, _lm_pdpe_t *p_pdpe) {
	_lm_pde_t *r = 0;

	_lm_pdpe_t *_p_pdpe = (p_pdpe) ? p_pdpe : get_pdpe(va);
	if(_p_pdpe) {
		if(_p_pdpe->f.ps == 0) {
			r = (_lm_pde_t *)p_to_k(PML4_4KNUM2PAGE(_p_pdpe->f.base));
			r += PD_INDEX(va);
		}
	}

	return r;
}

_lm_pte_t *cVMMx64::get_pte(_vaddr_t va, _lm_pde_t *p_pde) {
	_lm_pte_t *r = 0;

	_lm_pde_t *_p_pde = (p_pde) ? p_pde : get_pde(va);
	if(_p_pde) {
		if(_p_pde->f.ps == 0) { // have PTE because the page size is 4K
			r = (_lm_pte_t *)p_to_k(PML4_4KNUM2PAGE(_p_pde->f.t._4k.base));
			r += PT_INDEX(va);
		}
	}

	return r;
}

_lm_pml4e_t *cVMMx64::get_pml4_table(void) {
	return (_lm_pml4e_t *)p_to_k(m_mmap);
}

_lm_pdpe_t *cVMMx64::get_pdp_table(_vaddr_t va, _lm_pml4e_t *p_pml4e) {
	_lm_pdpe_t *p_pdp_base = 0;

	_lm_pml4e_t *_p_pml4e = (p_pml4e) ? p_pml4e : get_pml4e(va);
	if(_p_pml4e)
		p_pdp_base = (_lm_pdpe_t *)p_to_k(PML4_4KNUM2PAGE(_p_pml4e->f.base));

	return p_pdp_base;
}

_lm_pde_t *cVMMx64::get_pd_table(_vaddr_t va, _lm_pdpe_t *p_pdpe) {
	_lm_pde_t *p_pd_base = 0;

	_lm_pdpe_t *_p_pdpe = (p_pdpe) ? p_pdpe : get_pdpe(va);
	if(_p_pdpe)
		p_pd_base = (_lm_pde_t *)p_to_k(PML4_4KNUM2PAGE(_p_pdpe->f.base));

	return p_pd_base;
}

_lm_pte_t *cVMMx64::get_pt_table(_vaddr_t va, _lm_pde_t *p_pde) {
	_lm_pte_t *p_pt_base = 0;

	_lm_pde_t *_p_pde = (p_pde) ? p_pde : get_pde(va);
	if(_p_pde) {
		if(_p_pde->f.ps == 0)
			p_pt_base = (_lm_pte_t *)p_to_k(PML4_4KNUM2PAGE(_p_pde->f.t._4k.base));
	}

	return p_pt_base;
}

_lm_pdpe_t *cVMMx64::alloc_pdp_table(/*in*/_lm_pml4e_t *p_pml4e) {
	_lm_pdpe_t *p_pdp_base = 0;

	if(p_pml4e->e == 0) {
		p_pdp_base = (_lm_pdpe_t *)m_p_pma->alloc(1);
		if(p_pdp_base) {
			mem_set((_u8 *)p_pdp_base, 0, m_p_pma->get_page_size());

			p_pml4e->f.p = 1;
			p_pml4e->f.rw = 1;
			p_pml4e->f.base = PML4_4KPAGE2NUM(k_to_p((_vaddr_t)p_pdp_base));
		}
	}

	return p_pdp_base;
}

_lm_pde_t *cVMMx64::alloc_pd_table(/*in*/_lm_pdpe_t *p_pdpe) {
	_lm_pde_t *p_pd_base = 0;

	if(p_pdpe->e == 0) {
		p_pd_base = (_lm_pde_t *)m_p_pma->alloc(1);
		if(p_pd_base) {
			mem_set((_u8 *)p_pd_base, 0, m_p_pma->get_page_size());

			p_pdpe->f.p = 1;
			p_pdpe->f.rw = 1;
			p_pdpe->f.base = PML4_4KPAGE2NUM(k_to_p((_vaddr_t)p_pd_base));
		}
	}

	return p_pd_base;
}

_lm_pte_t *cVMMx64::alloc_pt_table(/*in*/_lm_pde_t *p_pde) {
	_lm_pte_t *p_pt_base = 0;

	if(p_pde->e == 0) {
		p_pt_base = (_lm_pte_t *)m_p_pma->alloc(1);
		if(p_pt_base) {
			mem_set((_u8 *)p_pt_base, 0, m_p_pma->get_page_size());

			p_pde->f.p = 1;
			p_pde->f.rw = 1;
			p_pde->f.t._4k.base = PML4_4KPAGE2NUM(k_to_p((_vaddr_t)p_pt_base));
		}
	}

	return p_pt_base;
}

bool cVMMx64::map_page(_lm_pte_t *p_pte, _vaddr_t page_kva, _u8 flags) {
	bool r = 0;

	if(p_pte->e == 0) {
		p_pte->f.p = 1;
		p_pte->f.rw = !(flags & VMMF_READONLY);
		p_pte->f.us = (flags & VMMF_USER);
		p_pte->f.pcd = (flags & VMMF_NON_CACHEABLE);
		p_pte->f.base = PML4_4KPAGE2NUM(k_to_p(page_kva));
		r = true;
	}

	return r;
}

_u32 cVMMx64::map(_vaddr_t *page_array, _u32 npages, _vaddr_t vaddr, _u8 flags, HMUTEX hlock) {
	_u32 r = 0;

	HMUTEX hm = lock(hlock);
	r = _map(page_array, npages, vaddr, flags);
	unlock(hm);

	return r;
}

// map 'npages' passed in 'page_array' at virtual address 'vaddr'
//  and return the actual number of mapped pages
_u32 cVMMx64::_map(_vaddr_t *page_array, _u32 npages, _vaddr_t vaddr, _u8 flags) {
	_u32 r = 0;

	_lm_pml4e_t *p_pml4 = get_pml4_table();

	_u16 ipml4 = PML4_INDEX(vaddr);
	_u16 ipdp = PDP_INDEX(vaddr);
	_u16 ipd = PD_INDEX(vaddr);
	_u16 ipt = PT_INDEX(vaddr);

	if(!p_pml4)
		goto _vmm_map_done_;

	while(ipml4 <= MAX_PT_INDEX && r < npages) {
		if((p_pml4 + ipml4)->e) {
			// explore PDP
			_lm_pdpe_t *p_pdp = (_lm_pdpe_t *)PN_TO_KVA((p_pml4 + ipml4)->f.base);

			if(!p_pdp)
				break;

			while(ipdp <= MAX_PT_INDEX && r < npages) {
				if((p_pdp + ipdp)->e) {
					if((p_pdp + ipdp)->f.ps == 1)
						goto _vmm_map_done_;

					// explore PD
					_lm_pde_t *p_pd = (_lm_pde_t *)PN_TO_KVA((p_pdp + ipdp)->f.base);

					if(!p_pd)
						goto _vmm_map_done_;

					while(ipd <= MAX_PT_INDEX && r < npages) {
						if((p_pd + ipd)->e) {
							if((p_pd + ipd)->f.ps == 1)
								goto _vmm_map_done_;

							// explore PT
							_lm_pte_t *p_pt = (_lm_pte_t *)PN_TO_KVA(
										(p_pd + ipd)->f.t._4k.base);

							if(!p_pt)
								goto _vmm_map_done_;

							while(ipt <= MAX_PT_INDEX && r < npages) {
								if(map_page((p_pt + ipt), page_array[r], flags))  {
									//DBG("VMM map page 0x%h at 0x%h\r\n",
									//		page_array[r], MK_VADDR_4K(
									//		ipml4, ipdp, ipd, ipt, 0));

									r++;
								} else {
									LOG("VMM ERRPR: _map at 0x%h\r\n", MK_VADDR_4K(
											ipml4, ipdp, ipd, ipt, 0));
									goto _vmm_map_done_;
								}

								ipt++;
							}
						} else {
							// alloc new PT
							if(alloc_pt_table((p_pd + ipd)))
								continue;
							else
								goto _vmm_map_done_;
						}

						ipd++;
						ipt = 0;
					}
				} else {
					// alloc new PD
					if(alloc_pd_table((p_pdp + ipdp)))
						continue;
					else
						goto _vmm_map_done_;
				}

				ipdp++;
				ipd = 0;
			}
		} else {
			// alloc new PDP
			if(alloc_pdp_table((p_pml4 + ipml4)))
				continue;
			else
				break;
		}

		ipml4++;
		ipdp = 0;
	}

_vmm_map_done_:
	return r;
}

_u32 cVMMx64::unmap(_vaddr_t vaddr_start, _vaddr_t vaddr_end, HMUTEX hlock) {
	_u32 r = 0;

	HMUTEX hm = lock(hlock);
	r = _unmap(vaddr_start, vaddr_end);
	unlock(hm);

	return r;
}

// remove 'npages' from the mapping at start address 'vaddr'
//  and return the actual number of removed pages
_u32 cVMMx64::_unmap(_vaddr_t vaddr_start, _vaddr_t vaddr_end) {
	_u32 r = 0;

	_u16 ipml4 = PML4_INDEX(vaddr_start);
	_u16 ipdp  = PDP_INDEX(vaddr_start);
	_u16 ipd   = PD_INDEX(vaddr_start);
	_u16 ipt   = PT_INDEX(vaddr_start);

	_lm_pml4e_t *p_pml4 = get_pml4_table();
	_lm_pdpe_t *p_pdp = 0;
	_lm_pde_t *p_pd = 0;
	_lm_pte_t *p_pt = 0;

	_u16 npdp,npd,npt;

	if(!p_pml4)
		goto _vmm_unmap_done_;

	while(ipml4 <= MAX_PT_INDEX) {
		if((p_pml4 + ipml4)->e == 0)
			break;

		p_pdp = (_lm_pdpe_t *)PN_TO_KVA((p_pml4 + ipml4)->f.base);
		while(ipdp <= MAX_PT_INDEX) {
			if((p_pdp + ipdp)->e) {
				if((p_pdp + ipdp)->f.ps == 0) {
					p_pd = (_lm_pde_t *)PN_TO_KVA((p_pdp + ipdp)->f.base);
					while(ipd <= MAX_PT_INDEX) {
						if((p_pd + ipd)->e) {
							if((p_pd + ipd)->f.ps == 0) {
								p_pt = (_lm_pte_t *)PN_TO_KVA(
											(p_pd + ipd)->f.t._4k.base);
								while(ipt <= MAX_PT_INDEX) {
									if((p_pt + ipt)->e) {
										// deallocate DATA page
										m_p_pma->free((void *)PN_TO_KVA(
												(p_pt + ipt)->f.base), 1);

										// clear PT entry
										(p_pt + ipt)->e = 0;
									}

									r++;

									ipt++;
									if(MK_VADDR_4K(ipml4, ipdp, ipd, ipt, 0) >= vaddr_end)
										break;
								}

								// check for empty PT table
								npt = 0;
								while((p_pt + npt)->e == 0 && npt <= MAX_PT_INDEX)
									npt++;
								////////////////////////////

								if(npt > MAX_PT_INDEX) {
									if((p_pd + ipd)->e) {
										// deallocate PT table
										m_p_pma->free((void *)PN_TO_KVA(
												(p_pd + ipd)->f.t._4k.base), 1);

										// clear PD entry
										(p_pd + ipd)->e = 0;
									}
								}
							} else {
								LOG("Can't remove 2M page (unsupported format) !\r\n","");
								goto _vmm_unmap_done_;
							}
						}

						ipd++;
						ipt = 0;
						if(MK_VADDR_4K(ipml4, ipdp, ipd, ipt, 0) >= vaddr_end)
							break;
					}

					// check for empty PD table
					npd = 0;
					while((p_pd + npd)->e == 0 && npd <= MAX_PT_INDEX)
						npd++;
					//////////////////////////

					if(npd > MAX_PT_INDEX) {
						if((p_pdp + ipdp)->e) {
							// deallocate PD table
							m_p_pma->free((void *)PN_TO_KVA((p_pdp + ipdp)->f.base), 1);

							// clear PDP entry
							(p_pdp + ipdp)->e = 0;
						}
					}
				} else {
					LOG("Can't remove 1G page (unsupported paging format) !\r\n","");
					goto _vmm_unmap_done_;
				}
			}

			ipdp++;
			ipd = 0;
			if(MK_VADDR_4K(ipml4, ipdp, ipd, ipt, 0) >= vaddr_end)
				break;
		}

		// check for empty PDP table
		npdp = 0;
		while((p_pdp + npdp)->e == 0 && npdp <= MAX_PT_INDEX)
			npdp++;
		//////////////////////////

		if(npdp > MAX_PT_INDEX) {
			if((p_pml4 + ipml4)->e) {
				// deallocate PDP table
				m_p_pma->free((void *)PN_TO_KVA((p_pml4 + ipml4)->f.base), 1);

				// clear PML4 entry
				(p_pml4 + ipml4)->e = 0;
			}
		}

		ipml4++;
		ipdp = 0;
		if(MK_VADDR_4K(ipml4, ipdp, ipd, ipt, 0) >= vaddr_end)
			break;
	}

_vmm_unmap_done_:
	return r;
}

// trace the mapping (if 'mapping' parameter is true) starts from 'vaddr' address and
//  return the  last  available virtual address
//  otherwise (if 'mapping' parameter is false) the function trace the empty space
_vaddr_t cVMMx64::trace(_vaddr_t vaddr, bool mapping, HMUTEX hlock) {
	_vaddr_t r =  0;

	HMUTEX hm = lock(hlock);
	r = ((mapping) ? trace_mapping(vaddr) : trace_hole(vaddr));
	unlock(hm);

	return r;
}

// trace the mapping from 'vaddr' address and
//  return the  last  available virtual address
_vaddr_t cVMMx64::trace_mapping(_vaddr_t vaddr) {
	_u16 ipml4 = PML4_INDEX(vaddr);
	_lm_pml4e_t *p_pml4_table = get_pml4_table();

	_u16 ipdp = PDP_INDEX(vaddr);
	_u16 ipd = PD_INDEX(vaddr);
	_u16 ipt = PT_INDEX(vaddr);

	_vaddr_t va = 0;

	while(p_pml4_table && (p_pml4_table + ipml4)->e) {
		_lm_pdpe_t *p_pdp_table = (_lm_pdpe_t *)PN_TO_KVA((p_pml4_table + ipml4)->f.base);

		while(p_pdp_table && (p_pdp_table + ipdp)->e) {
			if((p_pdp_table + ipdp)->f.ps == 0) { // 2m or 4k page size
				_lm_pde_t *p_pd_table = (_lm_pde_t *)PN_TO_KVA((p_pdp_table + ipdp)->f.base);

				while(p_pd_table && (p_pd_table + ipd)->e) {
					if((p_pd_table + ipd)->f.ps == 0) { // 4k page size
						_lm_pte_t *p_pt_table = (_lm_pte_t *)PN_TO_KVA(
									(p_pd_table+ipd)->f.t._4k.base);

						while(p_pt_table && (p_pt_table + ipt)->e) {
							va = MK_VADDR_4K(ipml4, ipdp, ipd, ipt, MAX_4K_OFFSET);
							ipt++;
							if(ipt > MAX_PT_INDEX)
								break;
						}

						if(p_pt_table && (p_pt_table + ipt)->e == 0)
							goto _vmm_trace_done_;
					} else
						va = MK_VADDR_2M(ipml4, ipdp, ipd, MAX_2M_OFFSET);

					ipd++;
					ipt = 0;
					if(ipd > MAX_PT_INDEX)
						break;

				}

				if(p_pd_table && (p_pd_table + ipd)->e == 0)
					goto _vmm_trace_done_;
			} else
				va = MK_VADDR_1G(ipml4, ipdp, MAX_1G_OFFSET);

			ipdp++;
			ipd = 0;
			if(ipdp > MAX_PT_INDEX)
				break;
		}

		if(p_pdp_table && (p_pdp_table + ipd)->e == 0)
			break;

		ipml4++;
		ipdp = 0;
		if(ipml4 > MAX_PT_INDEX)
			break;
	}

_vmm_trace_done_:
	return va;
}

// trace the mapping hole from 'vaddr' address and
//  return the  last  invalid virtual address
_vaddr_t cVMMx64::trace_hole(_vaddr_t vaddr) {
	_u16 ipml4 = PML4_INDEX(vaddr);
	_lm_pml4e_t *p_pml4_table = get_pml4_table();

	_u16 ipdp = PDP_INDEX(vaddr);
	_u16 ipd = PD_INDEX(vaddr);
	_u16 ipt = PT_INDEX(vaddr);

	_vaddr_t va = 0;

	while(p_pml4_table && ipml4 <= MAX_PT_INDEX) {
		if((p_pml4_table + ipml4)->e) {
			_lm_pdpe_t *p_pdp_table = (_lm_pdpe_t *)PN_TO_KVA((p_pml4_table + ipml4)->f.base);

			while(p_pdp_table && ipdp <= MAX_PT_INDEX) {
				if((p_pdp_table + ipdp)->e) {
					_lm_pde_t *p_pd_table = (_lm_pde_t *)PN_TO_KVA(
								(p_pdp_table + ipdp)->f.base);

					while(p_pd_table && ipd <= MAX_PT_INDEX) {
						if((p_pd_table + ipd)->e) {
							if((p_pd_table + ipd)->f.ps == 0) {
								_lm_pte_t *p_pt_table = (_lm_pte_t *)PN_TO_KVA(
										(p_pd_table+ipd)->f.t._4k.base);

								while(p_pt_table && ipt <= MAX_PT_INDEX) {
									if((p_pt_table + ipt)->e)
										goto _vmm_trace_hole_done_;
									else
										va = MK_VADDR_4K(ipml4,	ipdp,
												ipd, ipt,
												MAX_4K_OFFSET);

									ipt++;
								}
							} else
								goto _vmm_trace_hole_done_;
						} else
							va = MK_VADDR_4K(ipml4, ipdp, ipd,
									MAX_PT_INDEX, MAX_4K_OFFSET);

						ipd++;
						ipt = 0;
					}
				} else
					va = MK_VADDR_4K(ipml4, ipdp, MAX_PT_INDEX, // pd index
								      MAX_PT_INDEX, // pt index
								      MAX_4K_OFFSET);

				ipdp++;
				ipd = 0;
			}
		} else
			va = MK_VADDR_4K(ipml4,
					MAX_PT_INDEX, // pdp index
					MAX_PT_INDEX, // pd index
					MAX_PT_INDEX, // pt index
					MAX_4K_OFFSET);

		ipml4++;
		ipdp = 0;
	}

_vmm_trace_hole_done_:
	return va;
}

// convert virtual address to physical address
_vaddr_t cVMMx64::physical(_vaddr_t va) {
	_vaddr_t r = 0;

	_lm_pde_t *p_pde = get_pde(va);
	if(p_pde) {
		if(p_pde->f.ps == 0) {
			// 4k page size
			_lm_pte_t *p_pte = get_pte(va, p_pde);
			if(p_pte)
				r = PML4_4KNUM2PAGE(p_pte->f.base) + PT4K_OFFSET(va);
		} else
			// 2m page size
			r = PML4_2MNUM2PAGE(p_pde->f.t._2m.base) + PT2M_OFFSET(va);
	}

	return r;
}

bool cVMMx64::test(_vaddr_t vaddr, HMUTEX hlock) {
	bool r = false;

	HMUTEX hm = lock(hlock);
	r = _test(vaddr);
	unlock(hm);

	return r;
}

bool cVMMx64::_test(_vaddr_t vaddr) {
	bool r = false;

	_lm_pml4e_t *p_pml4e = get_pml4e(vaddr);
	if(p_pml4e && p_pml4e->e) {
		_lm_pdpe_t *p_pdpe = get_pdpe(vaddr, p_pml4e);
		if(p_pdpe && p_pdpe->e) {
			if(p_pdpe->f.ps == 0) {
				_lm_pde_t *p_pde = get_pde(vaddr, p_pdpe);
				if(p_pde && p_pde->e) {
					if(p_pde->f.ps == 0) {
						_lm_pte_t *p_pte = get_pte(vaddr, p_pde);
						if(p_pte && p_pte->e)
							// 4K page size
							r = true;
					} else
						// 2M page size
						r = true;
				}
			} else
				// 1G page size
				r = true;
		}
	}

	return r;
}

void *cVMMx64::alloc(_u32 mem_sz, _u8 flags, _vaddr_t vaddr_start,_vaddr_t vaddr_end, HMUTEX hlock) {
	void *r = 0;

	HMUTEX hm = lock(hlock);

	_ram_info_t ram;
	m_p_pma->info(&ram);

	_u32 page_size = m_p_pma->get_page_size();
	_u32 npages = mem_sz / page_size;
	npages += (mem_sz % page_size) ? 1 : 0;

	if((ram.rp - ram.pu - ram.sp) > npages) {
		// find mapping hole to fit 'npages'
		bool btmode = false;
		bool balloc = false;
		_vaddr_t start_va = vaddr_start;
		_vaddr_t end_va = 0;

		// verify start address
		if(_test(start_va))
			btmode = true;

		while(start_va < vaddr_end) {
			if(btmode) {
				end_va = trace_mapping(start_va);
				if(end_va)
					btmode = false;
				else
					break;
			} else {
				end_va = trace_hole(start_va);
				if(end_va) {
					if(((end_va + 1 - start_va) / page_size) >= npages) {
						balloc = true;
						break;
					}

					btmode = true;
				} else
					break;
			}

			start_va = end_va + 1;
		}

		if(balloc) {
			DBG("VMM BASE:0x%h ALC%dPG 0x%h--0x%h\r\n", p_to_k(m_mmap), npages,
						start_va, start_va + (npages * page_size));

			_vaddr_t page_array[VMM_ALLOC_PORTION];
#ifdef _DEBUG_
			static _vaddr_t _page_array[VMM_ALLOC_PORTION];
#endif
			_u32 _np = 0;
			_vaddr_t sva = start_va;
			r = (void *)sva;

			while(_np < npages) {
				mem_set((_u8 *)page_array, 0, sizeof(page_array));
				_u32 nalloc = ((npages - _np) < VMM_ALLOC_PORTION) ? (npages - _np) : VMM_ALLOC_PORTION;
				nalloc = m_p_pma->alloc(nalloc, page_array);
				if(nalloc) {
#ifdef _DEBUG_
					for(_u32 i = 0; i < nalloc; i++) {
						for(_u32 j = 0; j < VMM_ALLOC_PORTION; j++) {
							if(page_array[i] == _page_array[j])
								DBG("VMM ERROR: duplicat page 0x%h\r\n", _page_array[j]);
						}
					}
					for(_u32 i = 0; i < nalloc; i++)
						_page_array[i] = page_array[i];
#endif
					_u32 nmap = _map(page_array, nalloc, sva, flags);
					if(nmap != nalloc)
						break;
					sva += nmap * page_size;
					_np += nmap;
				} else
					break;
			}

			if(_np != npages) {
				LOG("VMM alloc ERROR: unmap 0x%h -- 0x%h\r\n", start_va, end_va);
				_unmap(start_va, end_va);
				r = 0;
			}
		}
	} else
		LOG("VMM ERRPR: Can't alloc more than %d pages\r\n", (ram.rp - ram.pu - ram.sp));

	unlock(hm);

	return r;
}

void cVMMx64::dump(_vaddr_t vaddr_start, _vaddr_t vaddr_end) {
	bool map=true;
	_vaddr_t va1 = vaddr_start;
	_vaddr_t va2 = 0;

	if(_test(va1))
		map = true;
	else
		map = false;

	while(va1 < vaddr_end) {
		va2 = trace(va1, map);
		if(va2) {
			if(map) {
				LOG("MAPPING: 0x%h --- 0x%h\r\n", va1, va2);
			} else {
				LOG("HOLE   : 0x%h --- 0x%h\r\n", va1, va2);
			}

			map = !map;
			va1 = va2+1;
			if(va1 == 0 || va1 > vaddr_end)
				break;
		} else
			break;
	}
}

