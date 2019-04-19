#include "iDisplay.h"
#include "elf_parser.h"
#include "str.h"

#define GOT	(_str_t)".got"
#define SYMTAB	(_str_t)".symtab"
#define DATA	(_str_t)".data"
#define RODATA	(_str_t)".rodata"
#define BSS	(_str_t)".bss"
#define CTORS	(_str_t)".ctors"
#define DTORS	(_str_t)".dtors"
#define GOT_PLT	(_str_t)".got.plt"
#define INIT	(_str_t)".init"
#define FINI	(_str_t)".fini"
#define JCR	(_str_t)".jcr"
#define TEXT	(_str_t)".text"
#define REL_DYN	(_str_t)".rel.dyn"

IMPLEMENT_BASE(cELF32, "cELF32", 0x0001);

IMPLEMENT_DESTROY(cELF32) {
	if(m_snames) {
		m_p_heap->free(m_snames);
		m_snames = 0;
	}
}

IMPLEMENT_INIT(cELF32, p_rb) {	
	bool r = false;
	
	REPOSITORY = p_rb; // keep repository
	m_snames = 0;
	if((m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE)))
		r = true;
	
	return r;
}

bool cELF32::init(_fs_handle_ hfile) {
	bool r = false;
	
	if(hfile->htype == HTYPE_FILE) {
		m_hfile = hfile; // keep file handle
		
		mem_set((_u8 *)&m_hdr,0,sizeof(m_hdr));
		
		if(m_snames) { // release allocation of prev. section names
			m_p_heap->free(m_snames);
			m_snames = 0;
		}
		
		// read ELF header
		if(m_hfile->p_fscxt->read(0, (_u8 *)&m_hdr, sizeof(m_hdr), m_hfile) == sizeof(m_hdr)) {
			if(identify())
				// load section names
				r = load_snames();
		}
	}
	
	return r;
}

bool cELF32::identify(void) {
	bool r = false;
	
	if(m_hdr.e_ident[EI_MAG0] == ELFMAG0 && 
	   m_hdr.e_ident[EI_MAG1] == ELFMAG1 &&
	   m_hdr.e_ident[EI_MAG2] == ELFMAG2 &&
	   m_hdr.e_ident[EI_MAG3] == ELFMAG3 &&
	   m_hdr.e_ident[EI_CLASS] == ELFCLASS32)
		r = true;

	return r;
}

// read entry from section header by index
bool cELF32::sh_entry(Elf32_Half index, Elf32_Shdr *p_shdr) {
	bool r = false;
	_umax sh_offset = 0;
	
	if(index < m_hdr.e_shentsize) {
		// calculate the entry offset
		sh_offset = m_hdr.e_shoff + (index * sizeof(Elf32_Shdr));
		// read entry
		if(m_hfile->p_fscxt->fread(sh_offset, (_u8 *)p_shdr, sizeof(Elf32_Shdr), m_hfile) == sizeof(Elf32_Shdr))
			r = true;
	}
	
	return r;
}

// read entry from program header by index
bool cELF32::ph_entry(Elf32_Half index, Elf32_Phdr *p_phdr) {
	bool r = false;
	_umax ph_offset = 0;

        if(index < m_hdr.e_phentsize) {
		// calculate the entry offset
		ph_offset = m_hdr.e_phoff + (index * sizeof(Elf32_Phdr));
		// read entry
		if(m_hfile->p_fscxt->fread(ph_offset, (_u8 *)p_phdr, sizeof(Elf32_Phdr), m_hfile) == sizeof(Elf32_Phdr))
			r = true;
	}
	
	return r;
}

// returns the sum of the length of sections in the program header
_u32 cELF32::program_size(void) {
	_u32 r = 0;
	Elf32_Phdr phdr;
	Elf32_Half pidx = 0;

	while(pidx < m_hdr.e_phnum) {
		if(ph_entry(pidx, &phdr)) {
			if(phdr.p_type == PT_LOAD)
				r += phdr.p_filesz;
		}
		
		pidx++;
	}	
	
	return r;
}

// load section names stringtable
bool cELF32::load_snames(void) {
	bool r = false;
	Elf32_Shdr se_names; // section names entry
	
	if(m_hdr.e_shstrndx != SHN_UNDEF) {
		// read section entry for section names
		if(sh_entry(m_hdr.e_shstrndx, &se_names)) {
			// alloc memory for section names stringtable
			if((m_snames = (_str_t)m_p_heap->alloc((_u32)(se_names.sh_size+1)))) {
				// read section names from file
				if(m_hfile->p_fscxt->read(se_names.sh_offset, (_u8 *)m_snames, se_names.sh_size, m_hfile) == 
						se_names.sh_size)
					r = true;
			}
		}
	}
	
	return r;
}

// return pointer to section name
_str_t cELF32::sname(Elf32_Shdr *p_shdr) {
	_str_t r = 0;
	
	if(m_snames)
		r = m_snames + p_shdr->sh_name;
	
	return r;
}

bool cELF32::load_section(Elf32_Shdr *p_shdr, _u8 *dst) {
	bool r = false;
	
	if(m_hfile) {
		if(m_hfile->p_fscxt->read(p_shdr->sh_offset, dst, p_shdr->sh_size, m_hfile) == p_shdr->sh_size)
			r = true;
	}
	
	return r;
}

bool cELF32::load_segment(Elf32_Phdr *p_phdr, _u8 *dst) {
	bool r = false;

	if(m_hfile) {
		if(m_hfile->p_fscxt->read(p_phdr->p_offset, dst, p_phdr->p_filesz, m_hfile) == p_phdr->p_filesz)
			r = true;
	}

	return r;
}

bool cELF32::relocate(void) {
	bool r = false;
	Elf32_Shdr shdr;
	
	if(section_by_name(REL_DYN, &shdr)) {
		Elf32_Rel *p_rel = (Elf32_Rel *)(m_base + shdr.sh_addr);
		_u32 count = shdr.sh_size / sizeof(Elf32_Rel);
		_u32 i = 0;
		
		while(i < count) {
			Elf32_Addr *p_addr = (Elf32_Addr *)(m_base + (p_rel + i)->r_offset);
			if(*p_addr)
				*p_addr = *p_addr + m_base;

			i++;
		}
		
		r = true;
	}
	
	return r;
}

bool cELF32::load(_u8 *dst) {
	bool r = false;
	Elf32_Phdr phdr;
	Elf32_Half pidx = 0;

	while(pidx < m_hdr.e_phnum) {
		if(ph_entry(pidx, &phdr)) {
			if(phdr.p_type == PT_LOAD) {
				if(!load_segment(&phdr, dst + phdr.p_vaddr))
					break;
			}
		}

		pidx++;
	}

	if(pidx == m_hdr.e_phnum) {
		m_base = (_u32)(_u64)dst;
		if(relocate())
			r = m_present = true;
	}
	
	return r;
}

bool cELF32::load(void) {
	bool r = false;

	//???
	
	return r;
}

bool cELF32::section_by_name(_str_t name, Elf32_Shdr *p_shdr) {
	bool r = false;

	Elf32_Half sidx = 1;
	Elf32_Shdr shdr;
	_str_t _sn=0;

	while(sidx < m_hdr.e_shnum) {
		if(sh_entry(sidx, &shdr)) {
			_sn = sname(&shdr);

			if(str_cmp(name, _sn) == 0) {
				mem_cpy((_u8 *)p_shdr, (_u8 *)&shdr, sizeof(Elf32_Shdr));
				r = true;
				break;
			}
		}
		
		sidx++;
	}

	return r;
}

_u32 cELF32::exec(void *p_args) {
	_u32 r = 0;
	
	if(m_present) {
		// recalculate entry address
		_elf_entry_t *p_entry = (_elf_entry_t *)(m_base + m_hdr.e_entry);
		// call entry point
		p_entry(p_args);
	} else
		r = 0xffffffff;
	
	return r;
}
