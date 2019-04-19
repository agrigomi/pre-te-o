#include <stdio.h>
#include <str.h>
#include "clargs.h"

static _cl_arg_t *_g_p_args_=0;
_s8 _g_err_text_[256]="";
_str_t *_g_p_argv_=0;
_u32 _g_argc_=0;

static _s32 find_option(_str_t opt, bool *lopt=0, bool *sopt=0) {
	_s32 r = -1;
	_u32 n = 0;
	
	if(_g_p_args_) {
		n = 0;
		while(_g_p_args_[n].type != 0 || _g_p_args_[n].opt != 0 || _g_p_args_[n].sopt != 0) {
			bool blopt = (_g_p_args_[n].opt && 
					str_ncmp(opt, (_str_t)_g_p_args_[n].opt, 
						str_len((_str_t)_g_p_args_[n].opt)) == 0) 
				? true : false;
			bool bsopt = (_g_p_args_[n].sopt && 
					str_ncmp(opt, (_str_t)_g_p_args_[n].sopt, 
						str_len((_str_t)_g_p_args_[n].sopt)) == 0) 
				? true : false;
			
			if(lopt)
				*lopt = blopt;
			if(sopt)
				*sopt = bsopt;
			
			if(blopt || bsopt) {
				r = n;
				break;
			}
			
			n++;
		}
	}

	return r;
}

void clargs_reset(void) {
	_u32 n = 0;
	
	if(_g_p_args_) {
		n = 0;
		while(_g_p_args_[n].type) {
			_g_p_args_[n].arg = 0;
			n++;
		}
	}
}

bool clargs_parse(_s32 argc, _str_t argv[], _cl_arg_t opt[]) {
	bool r = true;
	_s32 n = 0;
	_u32 i = 0;
	
	_g_p_args_ = opt;
	_g_p_argv_ = argv;
	_g_argc_ = argc;

	clargs_reset();
	
	for(i = 1; i < (_u32)argc; i++) {
		if(argv[i][0] == '-') {
			bool blopt = false;
			bool bsopt = false;
			n = find_option(argv[i], &blopt, &bsopt);
			if( n != -1 ) {
				// option found
				if(opt[n].type & ARG_TFLAG) {
					opt[n].active = true;
					opt[n].idx = i;
				}
				
				if(opt[n].type & ARG_TINPUT) {
					if(blopt) {
						if(str_len(argv[i]) > str_len((_str_t)opt[n].opt))
							opt[n].arg = argv[i] + str_len((_str_t)opt[n].opt);
					}

					if(bsopt) {
						if(str_len(argv[i]) > str_len((_str_t)opt[n].sopt))
							opt[n].arg = argv[i] + str_len((_str_t)opt[n].sopt);
					}
					
					if(opt[n].arg) {
						if(*(opt[n].arg) == '=' || *(opt[n].arg) == ':')
							opt[n].arg += 1;
						else {
							_snprintf(_g_err_text_, sizeof(_g_err_text_), 
									(_str_t)"Invalid option '%s'\n", argv[i]);
							r = false;
							break;
						}
						
						opt[n].active = true;
						opt[n].idx = i;
						opt[n].sz_arg = str_len(opt[n].arg);
					} else {
						_snprintf(_g_err_text_, sizeof(_g_err_text_), 
							(_str_t)"Required argument for option '%s'\n", argv[i]);
						r = false;
						break;
					}
				}
			} else {
				_snprintf(_g_err_text_, sizeof(_g_err_text_), 
					(_str_t)"Unrecognized option '%s'\n", argv[i]);
				r = false;
				break;
			}
		}
	}
	
	return r;
}

bool clargs_option(_cstr_t opt, _str_t *p_arg, _u32 *sz) {
	bool r = false;
	_s32 n = find_option((_str_t)opt);
	
	if(n != -1) {
		if(_g_p_args_[n].active) {
			if(_g_p_args_[n].type & ARG_TINPUT) {
				if(p_arg)
					*p_arg = _g_p_args_[n].arg;
				
				if(sz)
					*sz = _g_p_args_[n].sz_arg;
			}
			
			r = true;
		}
	}
	
	return r;
}

void clargs_print(_cl_arg_t opt[]) {
	_s32 n = 0;
	
	while(opt[n].type != 0 || opt[n].opt != 0 || opt[n].sopt != 0) {	
		printf("%s (%s)\t\t\t: %s\n", (opt[n].opt)?opt[n].opt:"", 
						(opt[n].sopt)?opt[n].sopt:"-", 
						(opt[n].des)?opt[n].des:"");
		n++;
	}
}

_str_t clargs_err_text(void) {
	return _g_err_text_;
}

_str_t clargs_parameter(_u32 index) {
	_str_t r = 0;
	_u32 idx = 0;

	for(_u32 i = 0; i < _g_argc_; i++) {
		if(_g_p_argv_[i][0] != '-') {
			if(idx == index) {
				r = _g_p_argv_[i];
				break;
			}

			idx++;
		}
	}
	
	return r;
}


