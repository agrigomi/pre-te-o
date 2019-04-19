#ifndef __I_MEMORY_H__
#define __I_MEMORY_H__


#define I_PMA		"iPMA" // Physical Memory Allocator
#define I_VMM		"iVMM" // Virtual Memory Manager
#define I_HEAP		"iHeap"
#define I_DMA_MEMORY	"iDMAMemory"

#include "iBase.h"
#include "iSyncObject.h"

typedef struct {
	_u64	addr;
	_u64	size;
}_ram_part_t;

typedef struct {
	_u16 	ps;	// page size
	_u32 	rp;	// ram pages (all available pages)
	_u32 	pu;	// pages in use
	_u32	sp;	// system pages (reserved by PMA)
}_ram_info_t;

// ram type
#define RT_SYSTEM	0x01
#define RT_USER		0x03

#define DEFAULT_PMA_LIMIT	DEFAULT_RAM_LIMIT

class iPMA:public iBase {
public:
	DECLARE_INTERFACE(I_PMA, iPMA, iBase);
	
	virtual bool init(_ram_part_t *p_ram_array, // pointer to ram parts array
			  _u16 nparts, // number of ram parts
			  _u16 page_size // page size in bytes
			)=0;

	virtual void info(_ram_info_t *p_info)=0;

	virtual _u16 get_page_size(void)=0;
	// consequently alloc
	virtual void *alloc(_u32 npages, _u8 type=RT_SYSTEM, _ulong limit=DEFAULT_PMA_LIMIT)=0;
	// consequently free
	virtual _u32 free(void *p, _u32 npages)=0;
	// inconsistent alloc
	virtual _u32 alloc(/* in */_u32 npages, // number of pages to alloc
			   /* out */_u64 *page_array, // array of page addresses in PM
			  /* in */_u8 type=RT_SYSTEM,
			  /* in */ _ulong limit=DEFAULT_PMA_LIMIT
			  )=0;
	// inconsistent free
	virtual _u32 free(/* in */_u32 npages, // number pages to release
			  /* in */_u64 *page_array // array of page addresses in PM
			 )=0;
	virtual _u32 alloc(void *p, _u32 npages, _u8 type=RT_SYSTEM)=0;
	virtual _u8 get_type(void *p)=0;

	virtual _ulong virtual_to_physical(_ulong vaddr)=0;
	virtual _ulong physical_to_virtual(_ulong paddr)=0;
	virtual void dump(void)=0;
};


#ifdef _64_
#define _vaddr_t _u64
#else
#define _vaddr_t _u32
#endif

// flags
#define VMMF_NON_CACHEABLE	1
#define VMMF_USER		(1<<1)
#define VMMF_READONLY		(1<<2)

class iVMM:public iBase {
public:
	DECLARE_INTERFACE(I_VMM, iVMM, iBase);

	// map 'npages' passed in 'page_array' at virtual address 'vaddr'
	//  and return the actual number of mapped pages
	virtual _u32 map(_vaddr_t *page_array, _u32 npages, _vaddr_t vaddr, _u8 flags, HMUTEX hlock=0)=0;
	// remove mapping at start address 'vaddr'
	//  and return the actual number of removed pages
	virtual _u32 unmap(_vaddr_t vaddr_start, _vaddr_t vaddr_end, HMUTEX hlock=0)=0;
	// trace mapping (if 'mapping' parameter is true) starts from 'vaddr_start' address and
	//  return the  last  available virtual address
	//  otherwise (if 'mapping' parameter is false) the function start trace unmapped memory region
	virtual _vaddr_t trace(_vaddr_t vaddr_start, bool mapping=true, HMUTEX hlock=0)=0;
	// convert virtual address to physical address
	virtual _vaddr_t physical(_vaddr_t va)=0;
	virtual void *alloc(_u32 mem_sz, _u8 flags, _vaddr_t vaddr_start, _vaddr_t vaddr_end, HMUTEX hlock=0)=0;
	// test mapping of single virtual address
	virtual bool test(_vaddr_t vaddr, HMUTEX hlock=0)=0;
	virtual void dump(_vaddr_t vaddr_start, _vaddr_t vaddr_end)=0;
	virtual iMutex *get_mutex(void)=0;
};

typedef struct {
	_u64 base; // base address
	_u32 size; // total heap size in bytes
	_u32 chunk_size; // size of metadata chunk
	_u32 data_load; // data load in bytes
	_u32 meta_load; // metadata load in bytes
	_u32 free; // free space in bytes
	_u32 unused; // unused space in bytes
	_u32 objects; 
} _heap_info_t;

#define DEFAULT_HEAP_LIMIT	DEFAULT_PMA_LIMIT

class iHeap:public iBase {
public:
  	//iHeap(void *heap_base, _u32 heap_sz);
	DECLARE_INTERFACE(I_HEAP, iHeap, iBase);

	virtual void init(void *p_base, _u32 heap_sz) = 0;	// init heap
	virtual bool init(_u32 heap_sz) = 0;	// init heap
	virtual	void *alloc(_u32 sz, _ulong limit=DEFAULT_HEAP_LIMIT) = 0;		// alloc memory
	virtual void free(void *p) = 0;			// free memory
	virtual void free(void *p, _u32 size) = 0;	// free memory
	virtual bool verify(void *p)=0;			// return true if the pointer belongs to Heap
	virtual bool verify(void *p, _u32 size)=0;	// return true if the pointer belongs to Heap
	virtual void info(_heap_info_t *p_info)=0;
	
	iBase *clone(iBase *p_object, _ulong limit=DEFAULT_HEAP_LIMIT) {
		iBase *r = 0;
		void *p=0;

		_u32 sz = p_object->object_size();
		if((p = alloc(sz, limit)))
			r = p_object->object_clone(p);
		
		return r;
	}
	
	void destroy(iBase *p_object) {
		if(verify(p_object, p_object->object_size())) {
			p_object->object_destroy();
			free(p_object, p_object->object_size());
		}
	}
};

class iDMAMemory:public iBase {
public:
	DECLARE_INTERFACE(I_DMA_MEMORY, iDMAMemory, iBase);
	virtual void init(_u8 *mem_base, _u32 mem_sz)=0;
	virtual _u8 *get_base(_u32 *size)=0;
	virtual _u8 *alloc_buffer(_u32 sz)=0;
	virtual void release_buffer(_u8 *p, _u32 sz)=0;
};

void* operator new(unsigned long s);
void operator delete(void *p);
void operator delete(void *p, unsigned long);

#endif

