#include "iExecutable.h"
#include "iDataStorage.h"
#include "iScheduler.h"
#include "ext.h"
#include "repository.h"
#include "iSystemLog.h"
#include "iVFS.h"

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

class cExtLdr:public iExtLoader {
public:
	cExtLdr() {
		m_p_vector = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cExtLdr() {}

	DECLARE_BASE(cExtLdr);
	DECLARE_INIT();

	HEXTENSION load(_str_t ext_path, _str_t name, _u32 sz_name=0);
	void exec(HEXTENSION hext, _str_t args);
	bool remove(HEXTENSION hext);
	void list(_cb_ext_info_t *p_cb, void *p_udata);

protected:
	iSystemLog 	*m_p_syslog;
	iVector		*m_p_vector;

	HEXTENSION load(FSHANDLE hfile, _str_t name, _u32 sz_name=0);
	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
};

typedef _u32 _ext_entry_t(_ext_context_t *);

typedef struct {
	_u8	*addr;
	_u32	size;
	_ext_entry_t *entry;
	_ext_context_t context;
	_s8	name[MAX_EXT_NAME];
}_ext_info_t;

IMPLEMENT_BASE(cExtLdr, "cExtLoader", 0x0001);
IMPLEMENT_INIT(cExtLdr, rbase) {
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	if((m_p_vector = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE)))
		m_p_vector->init(sizeof(_ext_info_t));
	return true;
}

HMUTEX cExtLdr::lock(HMUTEX hlock) {
	HMUTEX r = 0;
	if(m_p_vector)
		r = acquire_mutex(m_p_vector->get_mutex(), hlock);
	return r;
}

void cExtLdr::unlock(HMUTEX hlock) {
	if(m_p_vector)
		m_p_vector->get_mutex()->unlock(hlock);
}

HEXTENSION cExtLdr::load(FSHANDLE hfile, _str_t name, _u32 _UNUSED_ sz_name) {
	HEXTENSION r = 0;
	iExecutable *p_elf = (iExecutable *)OBJECT_BY_INTERFACE(I_EXECUTABLE, RY_CLONE);
	if(p_elf) {
		if(p_elf->init(hfile)) {
			iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
			if(p_pma) {
				_u32 prog_sz = p_elf->program_size();
				_u32 pages = prog_sz / p_pma->get_page_size();
				pages += (prog_sz % p_pma->get_page_size())?1:0;
				LOG("\t\t\tprogram size: %u.%u Kb (%d pages)\r\n", 
					prog_sz/1024, prog_sz%1024, pages);
				_u8 *p_elf_base = (_u8 *)p_pma->alloc(pages);
				if(p_elf_base) {
					DBG("loading ELF at 0x%h\r\n", p_elf_base);
					if(p_elf->load(p_elf_base)) {
						if(m_p_vector) {
							_ext_info_t ei;

							mem_set((_u8 *)&ei, 0, sizeof(_ext_info_t));
							LOG("\t\t\tstart address: 0x%h\r\n", p_elf_base);
							ei.addr = p_elf_base;
							str_cpy(ei.name, name, sizeof(ei.name));
							ei.size = prog_sz;
							ei.entry = (_ext_entry_t *)p_elf->entry();
							r = m_p_vector->add(&ei);
						}
					} else
						LOG("ERROR: in loading the program\r\n","");
				} else
					LOG("ERROR: in allocation of %d pages by PMA\r\n", pages);
				RELEASE_OBJECT(p_pma);
			}
		}
		RELEASE_OBJECT(p_elf);
	}

	return r;
}

HEXTENSION cExtLdr::load(_str_t ext_path, _str_t name, _u32 sz_name) {
	HEXTENSION r = 0;
	iFSRoot *p_fs_root = (iFSRoot *)OBJECT_BY_INTERFACE(I_FS_ROOT, RY_STANDALONE);
	if(p_fs_root) {
		FSHANDLE hfext=0;
		LOG("loading extension '%s'\r\n", ext_path);
		if(p_fs_root->fopen(0, ext_path, 0, 0, &hfext) == FS_RESULT_OK) {
			r = load(hfext, name, sz_name);
			p_fs_root->fclose(hfext);
		} else
			LOG("ERROR: failed to load extension '%s' (%s)\r\n", name, ext_path);
	}
	return r;
}

void cExtLdr::exec(HEXTENSION hext, _str_t args) {
	if(hext) {
		_ext_info_t *p_ei = (_ext_info_t *)hext;
		p_ei->context.p_repository = __g_p_repository__;
		p_ei->context.args = args;
		DBG("EXT: name='%s', addr=0x%h, size=%u\r\n", p_ei->name, p_ei->addr, p_ei->size);
		p_ei->entry(&p_ei->context);
	}
}

bool cExtLdr::remove(HEXTENSION hext) {
	bool r = false;
	HMUTEX hlock = lock();
	_ext_info_t *p_ei = (_ext_info_t *)hext;
	// TODO: ???
	unlock(hlock);
	return r;
}

void cExtLdr::list(_cb_ext_info_t *p_cb, void *p_udata) {
	HMUTEX hlock = lock();
	_ext_info_t *ei = (_ext_info_t *)m_p_vector->first(hlock);
	if(ei) {
		do {
			p_cb(ei->name, ei->size, (_ulong)ei->addr, p_udata);
		}while((ei = (_ext_info_t *)m_p_vector->next(hlock)));
	}
	unlock(hlock);
}

