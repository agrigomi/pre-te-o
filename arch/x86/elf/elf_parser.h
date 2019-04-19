#ifndef __ELF_PARSER__
#define __ELF_PARSER__

#include "iExecutable.h"
#include "elf.h"
#include "iSystemLog.h"
#include "iVFS.h"

typedef _u32 _elf_entry_t(void *);

typedef struct {
	_str_t		sname;
	Elf32_Shdr	shdr;
	bool		present;
} _section_info32_t;

class cELF32:public iExecutable {
public:
	cELF32() {
		m_present = false;
		m_base = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}

	//virtual ~cELF32() {}

	DECLARE_BASE(cELF32);
	DECLARE_INIT();
	DECLARE_DESTROY();

	bool init(FSHANDLE hfile // filesystem entry (source)
		 );

	// identify ELF 32
	bool identify(void);
	// returns the sum of the length of sections in the program header
	_u32 program_size(void);
	void *entry(void);
	// load at destination address
	bool load(_u8 *dst);
	// load at default virtual address
	bool load(void);
	// execute
	_u32 exec(void *p_args);

protected:
	bool		m_present;
	Elf32_Ehdr	m_hdr; // ELF 32 header
	FSHANDLE	m_hfile;
	iHeap		*m_p_heap;
	iRepositoryBase	*m_p_rb;
	_str_t		m_snames;
	Elf32_Addr	m_base;

	// read entry from section header by index
	bool sh_entry(Elf32_Half index, Elf32_Shdr *p_shdr);
	// read entry from program header by index
	bool ph_entry(Elf32_Half index, Elf32_Phdr *p_phdr);
	// allocate memory and read section names stringtable
	bool load_snames(void);
	// return pointer to section name
	_str_t sname(Elf32_Shdr *p_shdr);
	// load section content at destination address (dst)
	bool load_section(Elf32_Shdr *p_shdr, _u8 *dst);
        bool load_segment(Elf32_Phdr *p_phdr, _u8 *dst);
	bool relocate(void);
        bool section_by_name(_str_t sname, Elf32_Shdr *p_shdr);
};

///////////////////////////////////////////////////////////////////////////////////////////

class cELF64:public iExecutable {
public:
	cELF64() {
		m_present = false;
		m_base = 0;
		m_p_fshandle = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}

	//virtual ~cELF64() {}

	DECLARE_BASE(cELF64);
	DECLARE_INIT();
	DECLARE_DESTROY();

	bool init(FSHANDLE hfile // filesystem entry (source)
		 );

	// identify ELF 64
	bool identify(void);
	// returns the sum of the length of sections in the program header
	_u32 program_size(void);
	void *entry(void);
	// load at destination address
	bool load(_u8 *dst);
	// load at default virtual address
	bool load(void);
	// execute
	_u32 exec(void *p_args);

protected:
	bool		m_present;
	_u16		m_err;
	Elf64_Ehdr	m_hdr; // ELF 64 header
	FSHANDLE	m_hfile;
	iHeap		*m_p_heap;
	iRepositoryBase	*m_p_rb;
	_str_t		m_snames;
	_u32		m_sz_snames;
	Elf64_Addr	m_base;
	iSystemLog	*m_p_syslog;
	iFSHandle	*m_p_fshandle;

	// read entry from section header by index
	bool sh_entry(Elf64_Half index, Elf64_Shdr *p_shdr);
	// read entry from program header by index
	bool ph_entry(Elf64_Half index, Elf64_Phdr *p_phdr);
	// allocate memory and read section names stringtable
	bool load_snames(void);
	// return pointer to section name
	_str_t sname(Elf64_Shdr *p_shdr);
	// load section content at destination address (dst)
	bool load_section(Elf64_Shdr *p_shdr, _u8 *dst);
        bool load_segment(Elf64_Phdr *p_phdr, _u8 *dst);
	bool relocate(void);
        bool section_by_name(_str_t name, Elf64_Shdr *p_shdr);
	bool adjust_rela(void);
	bool adjust_dyn(void);
};

#endif

