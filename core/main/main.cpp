#include "repository.h"
#include "iCPUHost.h"
#include "iSyncObject.h"
#include "iSystemLog.h"
#include "iExecutable.h"
#include "iMemory.h"
#include "str.h"
#include "startup_context.h"
#include "karg_parser.h"
#include "iSerialDevice.h"

#define INITIAL_BOOT_DIR		(_str_t)"init"
#define INITIAL_BOOT_MOUNT_POINT	(_str_t)"/init"
#define INIT_EXTENSION			(_str_t)"/init/boot/init.elf"


extern _startup_context_t *__g_p_startup_context__;

void mount_initial_boot(void) {
	iFSHost *p_fs_host = (iFSHost *)__g_p_repository__->get_object_by_interface(I_FS_HOST, RY_STANDALONE);
	iFSRoot *p_fs_root = (iFSRoot *)__g_p_repository__->get_object_by_interface(I_FS_ROOT, RY_STANDALONE);
	if(p_fs_host && p_fs_root) {
		// create directory for initial boot drive
		FSHANDLE hfile;
		if(p_fs_root->fopen(0, (_str_t)"/", INITIAL_BOOT_DIR, 
				FSP_OWNER_READ|FSP_OWNER_WRITE|FSP_OWNER_EXEC|FSP_DIR|FSP_CREATE_NEW, 
				&hfile) == FS_RESULT_OK) {

			p_fs_root->fclose(&hfile);

			// mount initial boot volume
			iVolume *p_vboot = p_fs_host->volume_by_serial(
					(_str_t)__g_p_startup_context__->volume_serial);
			if(p_vboot)
				p_fs_root->mount(p_vboot, INITIAL_BOOT_MOUNT_POINT);
		}
		__g_p_repository__->release_object(p_fs_root);
		__g_p_repository__->release_object(p_fs_host);
	}
}

void setup_root_fs(void) {
	FSHANDLE hdev = 0;

	iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, 
											RY_STANDALONE);
	iFSHandle *p_fsh = (iFSHandle *)__g_p_repository__->get_object_by_interface(I_FS_HANDLE, RY_STANDALONE);
	
	if(p_dr && p_fsh) {
		_driver_request_t dr;
		dr.flags = DRF_IDENT;
		dr.DRIVER_IDENT = "vrdd0";
		iVolume *p_vol = (iVolume *)p_dr->driver_request(&dr);
		if(p_vol) {
			dr.DRIVER_IDENT = "MGFS3-VFS";
			iFS *p_fs = (iFS *)p_dr->driver_request(&dr);
			if(p_fs) {
				if((hdev = p_fsh->create(p_vol, 0, HTYPE_VOLUME))) {
					p_fsh->update_state(hdev, HSTATE_OPEN);
					_u64 dev_size = p_vol->_vol_block_size_ * p_vol->_vol_size_;
					if(p_fs->format(hdev, dev_size, 1, 0, (_str_t)"ROOT")) {
						// mount root
						iFSRoot *p_fs_root = 
							(iFSRoot *)__g_p_repository__->get_object_by_interface(
									I_FS_ROOT, RY_STANDALONE);
						if(p_fs_root) {
							p_fs_root->mount(p_vol, (_str_t)"/");
							__g_p_repository__->release_object(p_fs_root);
						}
					}
				}
			}
		}

		__g_p_repository__->release_object(p_dr);
	}
}

_s32 main(_str_t args) {
	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);

	setup_root_fs();
	mount_initial_boot();

	iExtLoader *p_eld = (iExtLoader *)__g_p_repository__->get_object_by_interface(I_EXT_LOADER, RY_STANDALONE);
	if(p_eld) {
		_s8 par[128]="";
		if(str_parameter(args,(_str_t)"init", par, sizeof(par))) {
			HEXTENSION hinit = p_eld->load(INIT_EXTENSION, (_str_t)"init", 4);
			if(hinit) 
				p_eld->exec(hinit, par);
			else
				syslog->write("missing 'init' executable\r\n");
				
		} else
			syslog->fwrite("missing 'init' parameter in '%s'\r\n", args);

		__g_p_repository__->release_object(p_eld);
	}
	
	return 0;
}

