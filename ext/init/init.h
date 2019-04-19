#ifndef __EXT_INIT_H__
#define __EXT_INIT_H__

#include "ext.h"
#include "repository.h"
#include "iSystemLog.h"
#include "iExecutable.h"
#include "iSerialDevice.h"
#include "iHypertext.h"
#include "iDataStorage.h"

#define MAX_EXT_CMD_LINE	256

typedef struct {
	HEXTENSION	h_extension;
	_s8		module_name[MAX_EXT_NAME];
	_s8		module_file[FS_MAX_PATH];
	_s8		par_cmdline[MAX_EXT_CMD_LINE];
}_mod_info_t;

iSystemLog *get_syslog(void);
iXMLParser *get_xml_parser(void);
iEnv *get_env(void);
iFSRoot *get_fs_root(void);
iHeap *get_heap(void);
iVector *get_ext_vector(void);

#endif

