#ifndef __I_REPOSITORY__
#define __I_REPOSITORY__

#include "iBase.h"

#define I_REPOSITORY	"iRepository"

#define INVALID_SO_STORAGE_INDEX	0


typedef struct {
	_ry_flags_t 	flags;
	_ry_id_t	id;
	iBase 		*p_obj;
	_u16		ref_cnt;
} __attribute__((packed)) _object_t;


class iRepository:public iRepositoryBase {
public:
	DECLARE_INTERFACE(I_REPOSITORY, iRepository, iRepositoryBase);
	virtual _ulong add_so_storage(_object_t so_storage[], _u16 count, _u16 limit)=0;
	virtual bool remove_so_storage(_ulong so_idx)=0;
	virtual bool init_so_storage(_ulong so_idx)=0; // require initialized heap & mutex
};

#endif

