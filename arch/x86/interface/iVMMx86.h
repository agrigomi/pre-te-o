#ifndef __iVMM_X86__
#define __iVMM_X86__

#include "arch_ifclist.h"
#include "iMemory.h"

class iVMMx86:public iVMM {
public:
	DECLARE_INTERFACE(I_VMMX86, iVMMx86, iVMM);

	// init VMM unit (PER CPU)
	//  if 'remap' parameter is true, the function perform 
	//  alocation of new mapping table (copy of passed 'mapping'),
	//  else the object works under map table passed in 'mapping' parameter.
	//  The 'mapping' parameter will be passed as physical address
	virtual void init_vmm(_vaddr_t mapping, _u32 mapping_sz, bool remap=false)=0;
	// activate mapping
	virtual void activate(void)=0;
	// return the virtual address (in kernel space) of mapping table
	virtual void *get_mapping_base(void)=0;
};

#endif
