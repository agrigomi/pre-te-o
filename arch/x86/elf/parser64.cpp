#include "elf_parser.h"
#include "str.h"

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

#define GOT		(_str_t)".got"
#define SYMTAB		(_str_t)".symtab"
#define DATA		(_str_t)".data"
#define RODATA		(_str_t)".rodata"
#define BSS		(_str_t)".bss"
#define CTORS		(_str_t)".ctors"
#define DTORS		(_str_t)".dtors"
#define GOT_PLT		(_str_t)".got.plt"
#define INIT		(_str_t)".init"
#define FINI		(_str_t)".fini"
#define JCR		(_str_t)".jcr"
#define TEXT		(_str_t)".text"
#define RELA_DYN	(_str_t)".rela.dyn"


IMPLEMENT_BASE(cELF64, ELF64_CLASS_NAME, 0x0001);

IMPLEMENT_DESTROY(cELF64) {	
	if(m_snames) {
		m_p_heap->free(m_snames, m_sz_snames);
		m_snames = 0;
	}
}

IMPLEMENT_INIT(cELF64, prb) {	
	bool r = false;
	
	REPOSITORY = prb; // keep repository
	m_snames = 0;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG,RY_STANDALONE);
	m_p_fshandle = (iFSHandle *)OBJECT_BY_INTERFACE(I_FS_HANDLE, RY_STANDALONE);
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP,RY_STANDALONE);
	if(m_p_fshandle && m_p_heap)
		r = true;
	
	return r;
}

bool cELF64::init(FSHANDLE hfile) {
	bool r = false;
	
	if(m_p_fshandle->type(hfile) == HTYPE_FILE) {
		m_hfile = hfile; // keep file handle
		
		mem_set((_u8 *)&m_hdr,0,sizeof(m_hdr));
		
		if(m_snames) { // release allocation of prev. section names
			m_p_heap->free(m_snames, m_sz_snames);
			m_snames = 0;
		}
		
		// read ELF header
		iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfile);
		if(p_fscxt) {
			_u32 hdr_sz = 0;
			p_fscxt->fread(0, (_u8 *)&m_hdr, sizeof(m_hdr), &hdr_sz, m_hfile);
			if(hdr_sz == sizeof(m_hdr)) {
				if(identify())
					// load section names
					r = load_snames();
			} else {
				LOG("ELF64: failed to load ELF header\r\n", "");
			}
		}
	}
	
	return r;
}

bool cELF64::identify(void) {
	bool r = false;
	
	if(m_hdr.e_ident[EI_MAG0] == ELFMAG0 && 
	   m_hdr.e_ident[EI_MAG1] == ELFMAG1 &&
	   m_hdr.e_ident[EI_MAG2] == ELFMAG2 &&
	   m_hdr.e_ident[EI_MAG3] == ELFMAG3 &&
	   m_hdr.e_ident[EI_CLASS] == ELFCLASS64)
		r = true;

	return r;
}

// read entry from section header by index
bool cELF64::sh_entry(Elf64_Half index, Elf64_Shdr *p_shdr) {
	bool r = false;
	_umax sh_offset = 0;
	
	if(index < m_hdr.e_shnum) {
		// calculate the entry offset
		sh_offset = m_hdr.e_shoff + (index * sizeof(Elf64_Shdr));
		// read entry
		iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfile);
		if(p_fscxt) {
			_u32 nb=0;
			p_fscxt->fread(sh_offset, (_u8 *)p_shdr, sizeof(Elf64_Shdr), &nb, m_hfile);
			if(nb == sizeof(Elf64_Shdr))
				r = true;
			else
				LOG("ELF64: failed to read section %d\r\n", index);
		}
	}
	
	return r;
}

// read entry from program header by index
bool cELF64::ph_entry(Elf64_Half index, Elf64_Phdr *p_phdr) {
	bool r = false;
	_umax ph_offset = 0;

        if(index < m_hdr.e_phentsize) {
		// calculate the entry offset
		ph_offset = m_hdr.e_phoff + (index * sizeof(Elf64_Phdr));
		// read entry
		iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfile);
		if(p_fscxt) {
			_u32 nb=0;
			p_fscxt->fread(ph_offset, (_u8 *)p_phdr, sizeof(Elf64_Phdr), &nb, m_hfile);
			if(nb == sizeof(Elf64_Phdr))
				r = true;
			else
				LOG("ELF64: failed to read segment %d\r\n", index);
		}
	}

	return r;
}

// returns the sum of the length of sections in the program header
_u32 cELF64::program_size(void) {
	_u32 r = 0;
	Elf64_Phdr phdr;
	Elf64_Half pidx = 0;

	while(pidx < m_hdr.e_phnum) {
		if(ph_entry(pidx, &phdr)) {
			if(phdr.p_type == PT_LOAD)
				r += phdr.p_memsz;
		}
		
		pidx++;
	}	
	
	return r;
}

// load section names stringtable
bool cELF64::load_snames(void) {
	bool r = false;
	Elf64_Shdr se_names; // section names entry
	
	if(m_hdr.e_shstrndx != SHN_UNDEF) {
		// read section entry for section names
		if(sh_entry(m_hdr.e_shstrndx, &se_names)) {
			// alloc memory for section names stringtable
			m_sz_snames = (_u32)(se_names.sh_size+1);
			if((m_snames = (_str_t)m_p_heap->alloc(m_sz_snames))) {
				// read section names from file
				iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfile);
				if(p_fscxt) {
					_u32 nb = 0;
					p_fscxt->fread(se_names.sh_offset, (_u8 *)m_snames, se_names.sh_size, &nb, m_hfile);
					if(nb == se_names.sh_size)
						r = true;
					else
						LOG("ELF64: failed to load section names\r\n","");
				}
			}
		} else
			LOG("ELF64: failed to section name %d\r\n", m_hdr.e_shstrndx);
	}
	
	return r;
}

// return pointer to section name
_str_t cELF64::sname(Elf64_Shdr *p_shdr) {
	_str_t r = 0;
	
	if(m_snames)
		r = m_snames + p_shdr->sh_name;

	return r;
}

bool cELF64::load_section(Elf64_Shdr *p_shdr, _u8 *dst) {
	bool r = false;
	
	if(m_hfile) {
		iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfile);
		if(p_fscxt) {
			_u32 nb = 0;
			p_fscxt->fread(p_shdr->sh_offset, dst, p_shdr->sh_size, &nb, m_hfile);
			if(nb == p_shdr->sh_size)
				r = true;
		}
	}
	
	return r;
}

bool cELF64::load_segment(Elf64_Phdr *p_phdr, _u8 *dst) {
	bool r = false;

	if(m_hfile) {
		DBG("load segment offset=%u, size=%u at 0x%h\r\n", p_phdr->p_offset, p_phdr->p_filesz, dst);
		iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfile);
		if(p_fscxt) {
			_u32 nb = 0;
			p_fscxt->read(p_phdr->p_offset, dst, p_phdr->p_filesz, &nb, m_hfile);
			if(nb == p_phdr->p_filesz)
				r = true;
		}
	}

	return r;
}

bool cELF64::adjust_rela(void) {
	bool r = false;
	Elf64_Shdr shdr;
	
	if(section_by_name(RELA_DYN, &shdr)) {
		DBG("relocate 'rela.dyn'\r\n","");
		Elf64_Rela *p_rel = (Elf64_Rela *)(m_base + shdr.sh_addr);
		_u32 count = shdr.sh_size / shdr.sh_entsize;
		_u32 i = 0;
		while(i < count) {
			Elf64_Addr *p_addr = (Elf64_Addr *)(m_base + p_rel->r_offset);
			DBG("0x%h --> ", *p_addr);
			if(*p_addr) {
				Elf64_Xword info = p_rel->r_info;
				switch(ELF64_R_TYPE(info)) {
					case R_X86_64_RELATIVE:
						if(*p_addr == (Elf64_Xword)p_rel->r_addend)
							*p_addr = p_rel->r_addend + m_base;
						p_rel->r_addend += m_base;
						break;
					case R_X86_64_IRELATIVE:
						p_rel->r_addend += m_base;
						break;
					case R_X86_64_JUMP_SLOT:
						*p_addr = *p_addr + m_base;
						break;
				}
			}
			DBG("0x%h\r\n", *p_addr);
			p_rel++;
			i++;
		}
		
		r = true;
	}
	
	return r;
}

bool cELF64::adjust_dyn(void) {
	bool r = false;
	//???
	return r;
}

bool cELF64::relocate(void) {
	bool r = adjust_rela();
	/*if(r)
		r = adjust_dyn();*/
	
	return r;
}

void *cELF64::entry(void) {
	return (void *)(m_base + m_hdr.e_entry);
}

bool cELF64::load(_u8 *dst) {
	bool r = false;
	Elf64_Phdr phdr;
	Elf64_Half pidx = 0;

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
		m_base = (_u64)dst;
		if(relocate())
			r = m_present = true;
	}

	DBG("loading ELF done !\r\n", "");
	
	return r;
}

bool cELF64::load(void) {
	bool r = false;
	
	//???
	
	return r;
}

bool cELF64::section_by_name(_str_t name, Elf64_Shdr *p_shdr) {
	bool r = false;

	Elf64_Half sidx = 1;
	Elf64_Shdr shdr;
	_str_t _sn=0;

	while(sidx < m_hdr.e_shnum) {
		if(sh_entry(sidx, &shdr)) {
			_sn = sname(&shdr);

			if(str_cmp(name, _sn) == 0) {
				mem_cpy((_u8 *)p_shdr, (_u8 *)&shdr, sizeof(Elf64_Shdr));
				r = true;
				break;
			}
		}

		sidx++;
	}

	return r;
}

_u32 cELF64::exec(void *p_args) {
	_u32 r = 0;
	
	if(m_present) {
		// recalculate entry address
		_elf_entry_t *p_entry = (_elf_entry_t *)entry();
		// call entry point
		p_entry(p_args);
	} else
		r = 0xffffffff;
	
	return r;
}

