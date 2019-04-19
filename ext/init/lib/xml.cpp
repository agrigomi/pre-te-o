#include "str.h"
#include "iHypertext.h"

#define XML_ROOT	0

union _parser_status {
	_u16	 	sw;
	struct {
		bool	_bo:1; // <
		bool	_to:1; // <xxx>
		bool	_tc:1; // /> or </
		bool	_tp:1; // <nnn xxx=xxx
		bool	_tn:1; // <nnn
	} f;
};

class cXMLTag:public iTag {
	public:
		cXMLTag() {}
		~cXMLTag() {}
		
		friend class cXMLParser;
		
		_str_t name(void);
		_u32 sz_name(void);
		_str_t content(void);
		_u32 sz_content(void);
		_str_t parameter(void);
		_u32 sz_parameter(void);
		_str_t parameter(_cstr_t name, _u32 *sz);
		void parameter_copy(_cstr_t name, _s8 *buffer, _u32 sz_buffer);

	private:
		_str_t		m_p_name;	// full tag name
		_u32		m_sz_name;
		_str_t		m_p_content;	// tag content
		_u32		m_sz_content;	// content size
		_str_t		m_p_par;
		_u32		m_sz_par;
		iDataStorage	*m_p_child;
};

class cXMLParser:public iXMLParser {
	public:
		cXMLParser() { 
			m_p_root = 0;
			m_p_content = 0;
			m_size = 0;
			REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
		}
		
		virtual ~cXMLParser();
		
		// required completely initialized objects of iHeap only
		void init(_str_t p_xml, // pointer to XML content
			  _u32 sz_xml);
		_str_t parse(void);
		void clear(void);
		iTag *select(_cstr_t xpath, iTag *p_start, _u32 index);


		DECLARE_BASE(cXMLParser);
		DECLARE_INIT();
		DECLARE_DESTROY();
				
	protected:
		iDataStorage	*m_p_root;
		_str_t		m_p_content;
		_u32		m_size; // content size in bytes
		_u16		m_err;		// error code
		
		
		_str_t parse(_str_t p,_parser_status &_s,iDataStorage *_v);
		_str_t _parse(_str_t p, iDataStorage *_v);
		cXMLTag *_select(_cstr_t xpath, iDataStorage *_v, _u32 index = 0);
		void clear(iDataStorage *p_storage);
		void init_tag(cXMLTag *t);
		void destroy_tag(cXMLTag *t);
};

cXMLParser::~cXMLParser() {
	object_destroy();
}


IMPLEMENT_BASE(cXMLParser, "cXMLParser", 0x0001);

IMPLEMENT_DESTROY(cXMLParser) {	
	clear();
}

IMPLEMENT_INIT(cXMLParser, p_rb) {	
	bool r = false;
	
	REPOSITORY = p_rb;
	
	if((m_p_root = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE))) {
		// init the child storage
		m_p_root->init(sizeof(cXMLTag));
		r = true;
	}
	
	return r;
}

void cXMLParser::init(_str_t p_xml, _u32 sz_xml) {
	m_p_content = p_xml;
	m_size = sz_xml;
}

void cXMLParser::destroy_tag(cXMLTag *t) {
	if(t->m_p_child) {
		RELEASE_OBJECT(t->m_p_child);
		t->m_p_child = 0;
	}
}

void cXMLParser::init_tag(cXMLTag *t) {
	t->m_p_name = 0;
	t->m_sz_name = 0;
	t->m_p_content = 0;
	t->m_sz_content = 0;
	t->m_p_par = 0;
	t->m_sz_par = 0;
	t->m_p_child = 0;
	
	// clone the 'iDataStorage' object
	if(REPOSITORY) {
		if((t->m_p_child = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE)))
			t->m_p_child->init(sizeof(cXMLTag));
	}
	////////////////////////////
}

_str_t cXMLParser::parse(void) {
	_str_t r = 0;
	
	_parser_status status;

	if(m_p_content) {
		// reset parser status
		status.sw = 0;
		// parse content
		r = parse(m_p_content, status, m_p_root);
	}
	
	return r;
}

void cXMLParser::clear(iDataStorage *p_storage) {
	cXMLTag *p_tag;
	_u32 i = 0;
	_u32 c = p_storage->cnt();
	
	for(i = 0; i < c; i++) {
		if((p_tag = (cXMLTag *)p_storage->get((_umax)i))) {
			if(p_tag->m_p_child) {
				clear(p_tag->m_p_child);
				p_tag->m_p_child = 0;
			}
		}
	}
	
	if(REPOSITORY)
		RELEASE_OBJECT(p_storage);
}

void cXMLParser::clear(void) {
	if(m_p_root) {
		clear(m_p_root);
		m_p_root = 0;
	}
}

_str_t cXMLParser::_parse(_str_t p, iDataStorage *_v) {
	_str_t _p = p;
	cXMLTag _t;
	bool used_tag = false;

	init_tag(&_t);
	

	/*
		Parse the packages fenced with "<! ... !> or <? ... ?>"
	*/
	switch(*_p) {
		case '?': {
			if(*(_p - 1) == '<') {
				/**** Get the package name  "<?name ... ?>" ****/
				_t.m_p_name = _p + 1;
				do {
					_p++;
				} while(*_p != ' ');
				
				_t.m_sz_name = (_u32)(_p - _t.m_p_name) ;
				/***********************************************/
				
				/* Get the package parameters  */
				_t.m_p_par =  _t.m_p_content = _p;
				do {
					_p++;
				}while(mem_cmp((_u8 *)_p,(_u8 *)"?>",2) != 0);
				
				_t.m_sz_par = _t.m_sz_content = (_u32)(_p - _t.m_p_par) ;
				/********************
				Attention:
					In this case the package parameters and tag content are the same
				*/
					
				_p += 1;
				_v->add(&_t);
				used_tag = true;
			}
		} break;
		
		case '!': {
			if(*(_p - 1) == '<') {
				if(mem_cmp((_u8 *)_p,(_u8 *)"!--",3) == 0) {
					_t.m_p_content = _p + 1;
					do {
						_p++;
					} while(mem_cmp((_u8 *)_p,(_u8 *)"-->",3) != 0);
						
					_p += 2;
						
					_t.m_sz_content = (_u32)(_p - p) - 1;
					_v->add(&_t);
					used_tag = true;
				} else {
					while(*_p != '>')
						_p++;
						
					_t.m_sz_par = (_u32)(_p - p) - 1;
					_v->add(&_t);
					used_tag = true;
				}
			}
		}break;
	}
	
	if(!used_tag)
		destroy_tag(&_t);
	
	return _p;
}

_str_t cXMLParser::parse(_str_t p,_parser_status &__s,iDataStorage *_v) {
	_str_t res = 0;
	_str_t _p = p;
	cXMLTag _t;
	_parser_status _s;
	
	_s.sw = 0; // clear parser status
	init_tag(&_t);
	
	while((_u32)(_p - m_p_content) < m_size) {
		switch(*_p) {
			case '<':
				_s.f._bo = true;
				_s.f._tp = false;
				
				if(*(_p+1) != '/') {
					if(_s.f._to) {
						_str_t __p = parse(_p,_s,_t.m_p_child);
						_t.m_sz_content += (_u32)(__p - _p);
						_p = __p;
					} else {
						_t.m_p_name = _p + 1;
						_t.m_sz_name = 0;
						_s.f._tn = true;
					}
				}
				
				break;
				
			case '>':
				_s.f._bo = false;
				_s.f._tn = false;
				_s.f._tp = false;
					
				if(_s.f._tc) {
					_s.f._tc = false;
					_s.f._to = false;
					
					_v->add(&_t);
					return _p;
				} else {
					_s.f._to = true;
					_s.f._tc = false;
					_t.m_sz_content = 0;
					_t.m_p_content = _p+1;
				}
				
				break;
				
			case '/':
				if(_s.f._bo) {
					if(*(_p+1) == '>' || *(_p-1) == '<') {
						_s.f._tn = false;
						_s.f._tp = false;
						_s.f._tc = true;
						
						_s.f._to = false;
					}
					
					if(_s.f._tp)
						_t.m_sz_par++;
				}
				
				if(_s.f._to)
					_t.m_sz_content++;
				
				break;
				
			case ' ':
				if(_s.f._bo && !_s.f._to) {
					if(_s.f._tp)
						_t.m_sz_par++;

					if(_s.f._tn) {
						_s.f._tn = false;
						_s.f._tp = true;
						_t.m_p_par = _p + 1;
						_t.m_sz_par = 0;
					} 
				} else {
					if(_s.f._to)
						_t.m_sz_content++;
				}
				
				break;

			case '?':
			case '!': 
				if(_s.f._bo && *(_p - 1) == '<') {
					_str_t __p = _parse(_p,_v);
					_t.m_sz_content += (_u32)(__p - _p);
					_p = __p;
					if(__s.f._to) {
						destroy_tag(&_t);
						return _p;
					}
				} 
				
				break;

			default:
				if(_s.f._to)
					_t.m_sz_content++;
					
				if(_s.f._tn)
					_t.m_sz_name++;
					
				if(_s.f._tp)
					_t.m_sz_par++;
					
				break;
		}
		
		_p++;
	}
	
	destroy_tag(&_t);
	return res;
}

cXMLTag *cXMLParser::_select(_cstr_t xpath, iDataStorage *_v, _u32 index) {
	cXMLTag *r = 0;
	cXMLTag *_t = 0;

	_u32 i = 0,j = 0;
	_u32 _sz = 0;
	_char_t _lb[512];
	
	if(xpath) {
		_sz = (_u32)str_len((_str_t)xpath);
		mem_set((_u8 *)_lb,0,sizeof(_lb));
		for(i = 0; i < _sz; i++) {
			if(xpath[i] != '/') {
				_lb[j] = xpath[i];
				j++;
			} else
				break;
		}
		
		_u32 _vsz = (_u32)_v->cnt();
		_u32 ix = 0;
		for(_u32 _i = 0; _i < _vsz; _i++) {
			if((_t = (cXMLTag *)_v->get((_umax)_i))) {
				if(_t->m_p_name) {
					if(mem_cmp((_u8 *)(_t->m_p_name),(_u8 *)_lb,_t->m_sz_name) == 0 && _t->m_sz_name == str_len(_lb)) {
						if(i < _sz) {
							return _select(xpath+i+1,_t->m_p_child,index);
						} else {
							if(ix == index) {
								r = _t;
								break;
							} 
							
							ix++;
						}
					}
				}
			}
		}
	} else
		r = (cXMLTag *)_v->get((_umax)index);

	return r;
}

iTag *cXMLParser::select(_cstr_t xpath = XML_ROOT, iTag *p_start = XML_ROOT, _u32 index = 0) {
	iTag *r = 0;
	
	if(p_start)
		r = _select(xpath, ((cXMLTag *)p_start)->m_p_child, index);
	else
		r = _select(xpath, m_p_root, index);
	
	return r;
}


////////////////////////////////////////////////////////////////////////////////////////

_str_t cXMLTag::name(void) {
	return m_p_name;
}

_u32 cXMLTag::sz_name(void) {
	return m_sz_name;
}

_str_t cXMLTag::content(void) {
	return m_p_content;
}

_u32 cXMLTag::sz_content(void) {
	return m_sz_content;
}

_str_t cXMLTag::parameter(void) {
	return m_p_par;
}

_u32 cXMLTag::sz_parameter(void) {
	return m_sz_par;
}

_str_t cXMLTag::parameter(_cstr_t name, _u32 *sz) {
	_u32 pvs = 0; // size of parameter value
	_str_t pv = 0; // pointer to parameter value
	_str_t p = parameter(); // pointer to parameter content
	_u32 l = sz_parameter(); // size of parameters content
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

void cXMLTag::parameter_copy(_cstr_t name, _s8 *buffer, _u32 sz_buffer) {
	_u32 psz = 0;
	_str_t par = parameter(name, &psz);

	mem_set((_u8 *)buffer, 0, sz_buffer);
	if(par) 
		str_cpy(buffer, par, (psz < sz_buffer) ? psz + 1 : sz_buffer - 1);
}




