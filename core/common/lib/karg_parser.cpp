#include "str.h"
#include "karg_parser.h"

static _str_t parameter(_str_t args, _str_t name, _u32 *sz) {
	_u32 pvs = 0; // size of parameter value
	_str_t pv = 0; // pointer to parameter value
	_str_t p = args; // pointer to parameter content
	_u32 l = str_len(args); // size of parameters content
	_str_t pn = 0; // pointer to parameter name;
	_u32 pns = 0; // size of parameter name
	_u32 i = 0; // index
	_s8 div = ' ';
	bool bo = false;
	
	for(i = 0; i < l; i++) {
		if(*(p + i) < ' ' && *(p + i) != '\t') {
			pvs++;
			continue;
		}

		switch(*(p + i)) {
			case ' ':
			case '\t':
				if(div == '"') {
					if(pv != 0) {
						// space in parameter value
						pvs++;
						break;
					} else {
						pv = (p + i); 
						pvs = 1;
					}

				} else {
					pv = 0;
					pn = 0;
					div = ' ';
				}

				break;
				
			case '"':
				if(div == '"') {
					if(pn != 0) {
						// compare the parameter name
						if(mem_cmp((_u8 *)name,(_u8 *)pn,pns) == 0) {
							i = l; // end of 'for' cycle
							break;
						} else
							pv = 0;
					}

					div = ' ';
					bo = !bo;

				} else 
					div = *(p + i);					

				break;
				
			case '=':
				if(!bo) {
					pv = 0;
					div = *(p + i);
				} else
					pvs++;
				break;
				
			default:
				switch(div) {
					case '=':
						break;
						
					case '"':
						if(pv)
							pvs++;
						else {
							pv = (p + i);
							pvs = 1;
						}
						break;
						
					case ' ':
					case '\t':
						if(pn)
							pns++;
						else {
							pn = (p + i);
							pns = 1;
						}
						break;
				}
				break;
		}
	}
	
	*sz = pvs;
	return pv;
}

_u32 int_parameter(_str_t args, _str_t name) {
	_u32 r = 0;
	_u32 sz=0;
	_str_t p_arg = parameter(args, name, &sz);
	if(p_arg) {
		_s8 lb[32]="";
		_u32 lb_idx = 0;
		bool _break = false;
		
		for(_u32 i = 0; i < sz && _break == false; i++) {
			switch(p_arg[i]) {
				case 'm':
				case 'M':
					lb[lb_idx] = 0;
					r = str2i((_str_t)lb, (_s32)lb_idx);
					r *= 1024 * 1024;
					_break = true;
					break;
					
				case 'k':
				case 'K':
					lb[lb_idx] = 0;
					r = str2i((_str_t)lb, (_s32)lb_idx);
					r *= 1024;
					_break = true;
					break;

				default:
					if(p_arg[i] >= '0' && p_arg[i] <= '9') {
						lb[lb_idx] = p_arg[i];
						lb_idx++;
						lb[lb_idx] = 0;
					}
					break;
			}
		}

		if(lb_idx && !r)
			r = str2i((_str_t)lb, (_s32)lb_idx);
	}

	return r;
}

bool str_parameter(_str_t args, _str_t name, _str_t value, _u32 szv) {
	bool r = false;
	_u32 sz=0;
	_str_t p_arg = parameter(args, name, &sz);
	sz++;
	if(p_arg) {
		str_cpy(value, p_arg, (sz < szv)?sz:szv);
		r = true;
	}

	return r;
}


