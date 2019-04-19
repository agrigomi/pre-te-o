#ifndef __ELF_32_64__
#define __ELF_32_64__

typedef _u32 	Elf32_Addr;
typedef _u16	Elf32_Half;
typedef _u32	Elf32_Off;
typedef _s32	Elf32_Sword;
typedef _u32	Elf32_Word;

typedef _u64	Elf64_Addr;
typedef _u64	Elf64_Off;
typedef _u16	Elf64_Half;
typedef _u32	Elf64_Word;
typedef _s32	Elf64_Sword;
typedef _u64	Elf64_Xword;
typedef _s64	Elf64_Sxword;

#define EI_NIDENT     16

typedef struct {
	_u8		e_ident[EI_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half	e_machine;
	Elf32_Word	e_version;
	Elf32_Addr	e_entry;
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	e_ehsize;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
	Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

typedef struct {
	_u8 		e_ident[EI_NIDENT]; /* ELF identification */
	Elf64_Half	e_type;      /* Object file type */
	Elf64_Half	e_machine;   /* Machine type */
	Elf64_Word	e_version;   /* Object file version */
	Elf64_Addr	e_entry;     /* Entry point address */
	Elf64_Off	e_phoff;     /* Program header offset */
	Elf64_Off	e_shoff;     /* Section header offset */
	Elf64_Word	e_flags;     /* Processor-specific flags */
	Elf64_Half	e_ehsize;    /* ELF header size */
	Elf64_Half	e_phentsize; /* Size of program header entry */
	Elf64_Half	e_phnum;     /* Number of program header entries */
	Elf64_Half	e_shentsize; /* Size of section header entry */
	Elf64_Half	e_shnum;     /* Number of section header entries */
	Elf64_Half	e_shstrndx;  /* Section name string table index */
} Elf64_Ehdr;

// e_ident
#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6

// ident mag
#define ELFMAG0		0x7f
#define ELFMAG1		'E'
#define ELFMAG2		'L'
#define ELFMAG3		'F'

// ident class
#define ELFCLASSNONE	0
#define ELFCLASS32	1	// ELF 32
#define ELFCLASS64	2	// ELF 64

// ident data
#define ELFDATANONE	0
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2

// e_type
#define	ET_NONE		0	// no file type
#define	ET_REL		1	// relocatable file
#define	ET_EXEC		2	// executable
#define	ET_DYN		3	// shared object
#define	ET_CORE		4	// core file
#define	ET_LOOS		0xfe00
#define	ET_HIOS		0xfeff
#define	ET_LOPROC	0xff00
#define	ET_HIPROC	0xffff

// e_machine
#define	EM_NONE		0
#define	EM_M32		1
#define	EM_SPARC	2
#define	EM_386		3
#define	EM_68K		4
#define	EM_88K		5
#define	EM_860		7
#define	EM_MIPS		8

// sections
typedef struct {
	Elf32_Word 	sh_name;
	Elf32_Word 	sh_type;
	Elf32_Word 	sh_flags;
	Elf32_Addr 	sh_addr;
	Elf32_Off  	sh_offset;
	Elf32_Word 	sh_size;
	Elf32_Word 	sh_link;
	Elf32_Word 	sh_info;
	Elf32_Word 	sh_addralign;
	Elf32_Word 	sh_entsize;
} Elf32_Shdr;

typedef struct {
	Elf64_Word	sh_name;      /* Section name */
	Elf64_Word	sh_type;      /* Section type */
	Elf64_Xword	sh_flags;     /* Section attributes */
	Elf64_Addr	sh_addr;      /* Virtual address in memory */
	Elf64_Off	sh_offset;    /* Offset in file */
	Elf64_Xword	sh_size;      /* Size of section */
	Elf64_Word	sh_link;      /* Link to other section */
	Elf64_Word	sh_info;      /* Miscellaneous information */
	Elf64_Xword	sh_addralign; /* Address alignment boundary */
	Elf64_Xword	sh_entsize;   /* Size of entries, if section has table */
} Elf64_Shdr;

#define SHN_UNDEF	0
#define SHN_LOPROC	0xff00
#define SHN_HIPROC	0xff1f
#define SHN_LOOS	0xff20
#define SHN_HIOS	0xff3f
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2
#define SHN_HIRESERVE	0xffff

// sh_type
#define	SHT_NULL	0	// Marks an unused section header
#define SHT_PROGBITS	1	// Contains information defined by the program
#define SHT_SYMTAB	2	// Contains a linker symbol table
#define SHT_STRTAB	3	// Contains a string table
#define SHT_RELA	4	// Contains “Rela” type relocation entries
#define SHT_HASH	5	// Contains a symbol hash table
#define SHT_DYNAMIC	6	// Contains dynamic linking tables
#define SHT_NOTE	7	// Contains note information
#define SHT_NOBITS	8	// Contains uninitialized space; does not occupy any space in the file
#define SHT_REL		9	// Contains “Rel” type relocation entries
#define SHT_SHLIB	10	// Reserved
#define SHT_DYNSYM	11	// Contains a dynamic loader symbol table
#define SHT_LOOS	0x60000000 // Environment-specific use
#define SHT_HIOS	0x6FFFFFFF
#define SHT_LOPROC	0x70000000 // Processor-specific use
#define SHT_HIPROC	0x7FFFFFFF
#define SHT_LOUSER	0x80000000
#define SHT_HIUSER	0xFFFFFFFF

// sh_flags
#define	SHF_WRITE	0x1		// Section contains writable data
#define SHF_ALLOC	0x2		// Section is allocated in memory image of program
#define	SHF_EXECINSTR	0x4		// Section contains executable instructions
#define	SHF_MASKOS	0x0F000000 	// Environment-specific use
#define	SHF_MASKPROC	0xF0000000	// Processor-specific use

// symbol table entry ELF32
typedef struct {
	Elf32_Word	st_name;
	Elf32_Addr	st_value;
	Elf32_Word	st_size;
	_u8		st_info;
	_u8		st_other;
	Elf32_Half	st_shndx;
} Elf32_Sym;

/* Macros for accessing the fields of st_info. */
#define ELF32_ST_BIND(info)		((info) >> 4)
#define ELF32_ST_TYPE(info)		((info) & 0xf)

/* Macro for constructing st_info from field values. */
#define ELF32_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))


// symbol table entry ELF64
typedef struct {
	Elf64_Word	st_name;  /* Symbol name */
	_u8		st_info;  /* Type and Binding attributes */
	_u8		st_other; /* Reserved */
	Elf64_Half	st_shndx; /* Section table index */
	Elf64_Addr	st_value; /* Symbol value */
	Elf64_Xword	st_size;  /* Size of object (e.g., common) */
} Elf64_Sym;

/* Macros for accessing the fields of st_info. */
#define ELF64_ST_BIND(info)		((info) >> 4)
#define ELF64_ST_TYPE(info)		((info) & 0xf)

/* Macro for constructing st_info from field values. */
#define ELF64_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

/* Macro for accessing the fields of st_other. */
#define ELF64_ST_VISIBILITY(oth)	((oth) & 0x3)

// Symbol Binding
#define	STB_LOCAL 	0
#define	STB_GLOBAL 	1
#define	STB_WEAK 	2
#define	STB_LOPROC 	13
#define	STB_HIPROC 	15


// symbol types
#define	STT_NOTYPE	0 	// No type specified (e.g., an absolute symbol)
#define	STT_OBJECT	1	// Data object
#define STT_FUNC	2	// Function entry point
#define	STT_SECTION	3	// Symbol is associated with a section
#define	STT_FILE	4	// Source file associated with the object file
#define	STT_LOOS	10	// Environment-specific use
#define	STT_HIOS	12
#define	STT_LOPROC	13	// Processor-specific use
#define	STT_HIPROC	15

// Relocations
typedef struct {
	Elf32_Addr	r_offset;
	Elf32_Word	r_info;
} Elf32_Rel;

typedef struct {
	Elf32_Addr	r_offset;
	Elf32_Word	r_info;
	Elf32_Sword	r_addend;
} Elf32_Rela;

/* Macros for accessing the fields of r_info. */
#define ELF32_R_SYM(info)	((info) >> 8)
#define ELF32_R_TYPE(info)	((unsigned char)(info))

/* Macro for constructing r_info from field values. */
#define ELF32_R_INFO(sym, type)	(((sym) << 8) + (unsigned char)(type))

// i386 relocations.
// TODO: this is just a subset
enum {
	R_386_NONE          = 0,
	R_386_32            = 1,
	R_386_PC32          = 2,
	R_386_GOT32         = 3,
	R_386_PLT32         = 4,
	R_386_COPY          = 5,
	R_386_GLOB_DAT      = 6,
	R_386_JUMP_SLOT     = 7,
	R_386_RELATIVE      = 8,
	R_386_GOTOFF        = 9,
	R_386_GOTPC         = 10,
	R_386_32PLT         = 11,
	R_386_TLS_TPOFF     = 14,
	R_386_TLS_IE        = 15,
	R_386_TLS_GOTIE     = 16,
	R_386_TLS_LE        = 17,
	R_386_TLS_GD        = 18,
	R_386_TLS_LDM       = 19,
	R_386_16            = 20,
	R_386_PC16          = 21,
	R_386_8             = 22,
	R_386_PC8           = 23,
	R_386_TLS_GD_32     = 24,
	R_386_TLS_GD_PUSH   = 25,
	R_386_TLS_GD_CALL   = 26,
	R_386_TLS_GD_POP    = 27,
	R_386_TLS_LDM_32    = 28,
	R_386_TLS_LDM_PUSH  = 29,
	R_386_TLS_LDM_CALL  = 30,
	R_386_TLS_LDM_POP   = 31,
	R_386_TLS_LDO_32    = 32,
	R_386_TLS_IE_32     = 33,
	R_386_TLS_LE_32     = 34,
	R_386_TLS_DTPMOD32  = 35,
	R_386_TLS_DTPOFF32  = 36,
	R_386_TLS_TPOFF32   = 37,
	R_386_TLS_GOTDESC   = 39,
	R_386_TLS_DESC_CALL = 40,
	R_386_TLS_DESC      = 41,
	R_386_IRELATIVE     = 42,
	R_386_NUM           = 43
};

typedef struct {
	Elf64_Addr	r_offset; /* Address of reference */
	Elf64_Xword	r_info;   /* Symbol index and type of relocation */
} Elf64_Rel;

typedef struct {
	Elf64_Addr	r_offset; /* Address of reference */
	Elf64_Xword	r_info;   /* Symbol index and type of relocation */
	Elf64_Sxword	r_addend; /* Constant part of expression */
} Elf64_Rela;


/* Macros for accessing the fields of r_info. */
#define ELF64_R_SYM(info)	((info) >> 32)
#define ELF64_R_TYPE(info)	((info) & 0xffffffffL)

/* Macro for constructing r_info from field values. */
#define ELF64_R_INFO(sym, type)	(((sym) << 32) + ((type) & 0xffffffffL))

// X86_64 relocations.
enum {
	R_X86_64_NONE       = 0,
	R_X86_64_64         = 1,
	R_X86_64_PC32       = 2,
	R_X86_64_GOT32      = 3,
	R_X86_64_PLT32      = 4,
	R_X86_64_COPY       = 5,
	R_X86_64_GLOB_DAT   = 6,
	R_X86_64_JUMP_SLOT  = 7,
	R_X86_64_RELATIVE   = 8,
	R_X86_64_GOTPCREL   = 9,
	R_X86_64_32         = 10,
	R_X86_64_32S        = 11,
	R_X86_64_16         = 12,
	R_X86_64_PC16       = 13,
	R_X86_64_8          = 14,
	R_X86_64_PC8        = 15,
	R_X86_64_DTPMOD64   = 16,
	R_X86_64_DTPOFF64   = 17,
	R_X86_64_TPOFF64    = 18,
	R_X86_64_TLSGD      = 19,
	R_X86_64_TLSLD      = 20,
	R_X86_64_DTPOFF32   = 21,
	R_X86_64_GOTTPOFF   = 22,
	R_X86_64_TPOFF32    = 23,
	R_X86_64_PC64       = 24,
	R_X86_64_GOTOFF64   = 25,
	R_X86_64_GOTPC32    = 26,
	R_X86_64_GOT64      = 27,
	R_X86_64_GOTPCREL64 = 28,
	R_X86_64_GOTPC64    = 29,
	R_X86_64_GOTPLT64   = 30,
	R_X86_64_PLTOFF64   = 31,
	R_X86_64_SIZE32     = 32,
	R_X86_64_SIZE64     = 33,
	R_X86_64_GOTPC32_TLSDESC = 34,
	R_X86_64_TLSDESC_CALL    = 35,
	R_X86_64_TLSDESC    = 36,
	R_X86_64_IRELATIVE  = 37
};

// Program header
typedef struct {
	Elf32_Word	p_type;
	Elf32_Off	p_offset;
	Elf32_Addr	p_vaddr;
	Elf32_Addr	p_paddr;
	Elf32_Word	p_filesz;
	Elf32_Word	p_memsz;
	Elf32_Word	p_flags;
	Elf32_Word	p_align;
} Elf32_Phdr;

typedef struct {
	Elf64_Word	p_type;          /* Type of segment */
	Elf64_Word	p_flags;         /* Segment attributes */
	Elf64_Off	p_offset;        /* Offset in file */
	Elf64_Addr	p_vaddr;         /* Virtual address in memory */
	Elf64_Addr	p_paddr;         /* Reserved */
	Elf64_Xword	p_filesz;        /* Size of segment in file */
	Elf64_Xword	p_memsz;         /* Size of segment in memory */
	Elf64_Xword	p_align;         /* Alignment of segment */
} Elf64_Phdr;

// p_type
#define	PT_NULL		0 		// Unused entry
#define	PT_LOAD		1 		// Loadable segment
#define	PT_DYNAMIC	2 		// Dynamic linking tables
#define	PT_INTERP	3 		// Program interpreter path name
#define	PT_NOTE		4 		// Note sections
#define	PT_SHLIB	5          	// Reserved
#define	PT_PHDR		6          	// Program header table
#define	PT_LOOS		0x60000000 	// Environment-specific use
#define	PT_HIOS		0x6FFFFFFF
#define	PT_LOPROC	0x70000000 	// Processor-specific use
#define	PT_HIPROC	0x7FFFFFFF

// p_flags
#define	PF_X		0x1		// Execute permission
#define	PF_W		0x2		// Write permission
#define	PF_R		0x4		// Read permission
#define	PF_MASKOS	0x00FF0000	// These flag bits are reserved for environment-specific use
#define	PF_MASKPROC	0xFF000000	// These flag bits are reserved for processor-specific use

// Dynamic table
typedef struct {
	Elf32_Sword	d_tag;
	union {
		Elf32_Word	d_val;
		Elf32_Addr	d_ptr;
	} d_un;
} Elf32_Dyn;
//extern Elf32_Dyn _DYNAMIC[];

typedef struct {
	Elf64_Sxword	d_tag;
	union {
		Elf64_Xword	d_val;
		Elf64_Addr	d_ptr;
	} d_un;
} Elf64_Dyn;
//extern Elf64_Dyn _DYNAMIC[];

// d_tag
#define	DT_NULL		0       // Marks the end of the dynamic array ignored
#define	DT_NEEDED 	1	// d_val   The string table offset of the name of a needed library.
#define	DT_PLTRELSZ	2 	// d_val   Total size, in bytes, of the relocation entries associated with the procedure linkage table.
#define	DT_PLTGOT	3	// d_ptr   Contains an address associated with the linkage table. The specific meaning of this field is processor-dependent.
#define	DT_HASH		4	// d_ptr   Address of the symbol hash table, described below.
#define	DT_STRTAB	5	// d_ptr   Address of the dynamic string table.
#define	DT_SYMTAB	6	// d_ptr   Address of the dynamic symbol table.
#define	DT_RELA		7	// d_ptr   Address of a relocation table with Elf64_Rela entries.
#define	DT_RELASZ	8	// d_val   Total size, in bytes, of the DT_RELA relocation table.
#define	DT_RELAENT	9	// d_val   Size, in bytes, of each DT_RELA relocation entry.
#define	DT_STRSZ	10	// d_val   Total size, in bytes, of the string table.
#define	DT_SYMENT	11	// d_val   Size, in bytes, of each symbol table entry.
#define	DT_INIT		12	// d_ptr   Address of the initialization function.
#define	DT_FINI		13	// d_ptr   Address of the termination function.
#define	DT_SONAME	14	// d_val   The string table offset of the name of this shared object.
#define	DT_RPATH	15	// d_val   The string table offset of a shared library search path string.
#define	DT_SYMBOLIC	16	// The presence of this dynamic table entry modifies the
                         	//	ignored
				//	symbol resolution algorithm for references within the
				//	library. Symbols defined within the library are used to
				//	resolve references before the dynamic linker searches the
				//	usual search path.
#define	DT_REL		17 	// d_ptr   Address of a relocation table with Elf64_Rel entries.
#define	DT_RELSZ	18	// d_val   Total size, in bytes, of the DT_REL relocation table.
#define	DT_RELENT	19	// d_val   Size, in bytes, of each DT_REL relocation entry.
#define	DT_PLTREL	20	// d_val   Type of relocation entry used for the procedure linkage
				//	table. The d_val member contains either DT_REL or DT_RELA.
#define	DT_DEBUG	21	// d_ptr   Reserved for debugger use.
#define	DT_TEXTREL	22	// The presence of this dynamic table entry signals that the
				//	ignored
				//	relocation table contains relocations for a non-writable
				//	segment.
#define	DT_JMPREL	23	// d_ptr   Address of the relocations associated with the procedure
				//	linkage table.
#define	DT_BIND_NOW	24	// The presence of this dynamic table entry signals that the
				//	ignored
				//	dynamic loader should process all relocations for this object
				//	before transferring control to the program.
#define	DT_INIT_ARRAY	25	// d_ptr   Pointer to an array of pointers to initialization functions.
#define	DT_FINI_ARRAY	26	// d_ptr   Pointer to an array of pointers to termination functions.
#define	DT_INIT_ARRAYSZ	27	// d_val   Size, in bytes, of the array of initialization functions.
#define	DT_FINI_ARRAYSZ	28	// d_val   Size, in bytes, of the array of termination functions.
#define	DT_LOOS		0x60000000 	// Defines a range of dynamic table tags that are reserved for
				   	//	environment-specific use.
#define	DT_HIOS		0x6FFFFFFF
#define	DT_LOPROC	0x70000000	// Defines a range of dynamic table tags that are reserved for
                      			// 	processor-specific use.
#define	DT_HIPROC	0x7FFFFFFF

extern Elf32_Addr _GLOBAL_OFFSET_TABLE_[];


#endif // __ELF_32_64__
