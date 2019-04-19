#include "init.h"

//#define _DEBUG_
#include "debug.h"

#define MAX_ENV_VNAME	128
#define ENV_PATH	(_str_t)"PATH"

static iSystemLog *_g_p_syslog_ = 0;
static iXMLParser *_g_p_xml_ = 0;
static iEnv	  *_g_p_env_ = 0;
static iFSRoot	  *_g_p_fs_root_ = 0;
static iHeap	  *_g_p_heap_ = 0;
static _str_t	  _g_init_content_=0;
static _u64	  _g_sz_init_=0;
static iVector	  *_g_p_v_ext_=0;
static iExtLoader *_g_p_ext_loader_=0;

iSystemLog *get_syslog(void) {
	if(!_g_p_syslog_)
		_g_p_syslog_ = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);
	return _g_p_syslog_;
}

iXMLParser *get_xml_parser(void) {
	if(!_g_p_xml_)
		_g_p_xml_ = (iXMLParser *)__g_p_repository__->get_object_by_interface(I_XML_PARSER, RY_CLONE);
	return _g_p_xml_;
}

iEnv *get_env(void) {
	if(!_g_p_env_)
		_g_p_env_ = (iEnv *)__g_p_repository__->get_object_by_interface(I_ENV, RY_ONCE);

	return _g_p_env_;
}

iFSRoot *get_fs_root(void) {
	if(!_g_p_fs_root_)
		_g_p_fs_root_ = (iFSRoot *)__g_p_repository__->get_object_by_interface(I_FS_ROOT, RY_STANDALONE);
	return _g_p_fs_root_;
}

iHeap *get_heap(void) {
	if(!_g_p_heap_)
		_g_p_heap_ = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	return _g_p_heap_;
}

iVector *get_ext_vector(void) {
	if(!_g_p_v_ext_) {
		if((_g_p_v_ext_ = (iVector *)__g_p_repository__->get_object_by_interface(I_VECTOR, RY_CLONE)))
			_g_p_v_ext_->init(sizeof(_mod_info_t));
	}
	return _g_p_v_ext_;
}

iExtLoader *get_ext_loader(void) {
	if(!_g_p_ext_loader_)
		_g_p_ext_loader_ = (iExtLoader *)__g_p_repository__->get_object_by_interface(I_EXT_LOADER, RY_STANDALONE);
	return _g_p_ext_loader_;
}

bool load_init_config(_str_t args) {
	bool r = false;
	iFSRoot *p_fs_root = get_fs_root();
	if(p_fs_root) {
		FSHANDLE hf_init;
		if(p_fs_root->fopen(0, args, (_str_t)"", 0, &hf_init) == FS_RESULT_OK) {
			p_fs_root->fsize(hf_init, &_g_sz_init_);
			iHeap *p_heap = get_heap();
			if(p_heap) {
				_g_init_content_ = (_str_t)p_heap->alloc(_g_sz_init_);
				if(_g_init_content_) {
					_u32 nb = 0;
					if(p_fs_root->fread(hf_init, _g_init_content_, _g_sz_init_, &nb) == FS_RESULT_OK)
						r = true;
					else
						p_heap->free(_g_init_content_, _g_sz_init_);
				}
			}
			p_fs_root->fclose(&hf_init);
		} else
			get_syslog()->fwrite("INIT: can't open file '%s'\r\n", args);
	}
	return r;
}

_mod_info_t *load_module(_str_t mod_name, _str_t mod_fname, _u32 sz_fname, _str_t params, _u32 sz_params) {
	_mod_info_t *r = 0;
	_mod_info_t mod;

	mem_set((_u8 *)&mod, 0, sizeof(_mod_info_t));
	str_cpy(mod.module_name, mod_name, sizeof(mod.module_name));
	// set module file name
	str_cpy(mod.module_file, mod_fname, (sz_fname+1 < sizeof(mod.module_file))?sz_fname+1:sizeof(mod.module_file));
	// set module parameters
	str_cpy(mod.par_cmdline, params, (sz_params < sizeof(mod.par_cmdline))?sz_params:sizeof(mod.par_cmdline));

	iExtLoader *p_xld = get_ext_loader();
	if(p_xld) {
		iVector *p_vext = get_ext_vector();
		if((mod.h_extension = p_xld->load(mod.module_file, mod.module_name)))
			r = (_mod_info_t *)p_vext->add(&mod);
		else
			get_syslog()->fwrite("INIT: can't open file '%s'\r\n", mod.module_file);
	}

	return r;
}

void parse_init_config(void) {
	iXMLParser *p_xml = get_xml_parser();
	if(p_xml) {
		p_xml->init(_g_init_content_, _g_sz_init_);
		if(p_xml->parse()) {
			DBG("INIT: successful parse xml content\r\n","");
			_u32 idx = 0;
			iEnv *p_env = get_env();
			DBG("INIT: select tag 'init/env'\r\n","");
			iTag *p_tag_env = p_xml->select("init/env", 0, idx);

			if(p_tag_env && p_env) { 
				iTag *p_tag_var = 0;
				idx=0;

				DBG("INIT: loading environment ...\r\n","");
				while((p_tag_var = p_xml->select("var", p_tag_env, idx))) {
					_s8 vname[MAX_ENV_VNAME]="";
					p_tag_var->parameter_copy("name", vname, sizeof(vname));
					DBG("INIT: set_env(%s)\r\n", vname);
					if(str_len(vname)) {
						_u32 sz_val = 0;
						_str_t val = p_tag_var->parameter("val", &sz_val);
						if(val)
							p_env->set(vname, val, sz_val);
					}
					idx++;
				}
			} else {
				DBG("INIT: ERROR p_tag_env=0x%h\r\n", p_tag_env);
				DBG("INIT: ERROR p_env=0x%h\r\n", p_env);
			}

			iVector *p_v_ext = get_ext_vector();
			if(p_v_ext) {
				idx = 0;
				iTag *p_tag_module = 0;

				while((p_tag_module = p_xml->select("init/module", 0, idx))) {
					_str_t params  = p_tag_module->content();
					_u32 sz_params = p_tag_module->sz_content();
					_s8 mod_name[MAX_EXT_NAME]="";
					_u32  sz_mod_file=0;
					_str_t mod_file = p_tag_module->parameter("file", &sz_mod_file);

					p_tag_module->parameter_copy("name", mod_name, sizeof(mod_name));
					load_module(mod_name, mod_file, sz_mod_file, params, sz_params);

					idx++;
				}
			}
		}
		p_xml->clear();
	}
}

void _main(_str_t args) {
	get_syslog()->write("--INIT--\r\n");
	
	if(load_init_config(args)) {
		parse_init_config();
		get_heap()->free(_g_init_content_, _g_sz_init_);
		iVector *pv = get_ext_vector();
		iExtLoader *p_xld = get_ext_loader();
		if(pv && p_xld) {
			_mod_info_t *pm = (_mod_info_t *)pv->first();
			if(pm) {
				do {
					p_xld->exec(pm->h_extension, pm->par_cmdline);
				}while((pm = (_mod_info_t *)pv->next()));
			}
		}
	}
}

