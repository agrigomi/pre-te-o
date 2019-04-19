#ifndef __I_HIPERTEXT__
#define __I_HIPERTEXT__

#define I_HYPERTEXT	"iHypertextParser"
#define I_XML_PARSER	"iXMLParser"

#include "iDataStorage.h"

class iTag {
	public:
		iTag() {}
		~iTag() {}
		
		virtual _str_t name(void) = 0;
		virtual _u32 sz_name(void) = 0;
		virtual _str_t content(void) = 0;
		virtual _u32 sz_content(void) = 0;
		virtual _str_t parameter(void) = 0;
		virtual _u32 sz_parameter(void) = 0;
		virtual _str_t parameter(_cstr_t name, _u32 *sz) = 0;
		virtual void parameter_copy(_cstr_t name, _s8 *buffer, _u32 sz_buffer)=0;

};

class iHypertextParser: public iBase {
	public:
		DECLARE_INTERFACE(I_HYPERTEXT, iHypertextParser, iBase);
		
		virtual void init(_str_t p_hipertext, _u32 sz_hipertext) = 0;
		virtual _str_t parse(void) = 0;
		virtual void clear(void) = 0;
		virtual iTag *select(_cstr_t xpath = 0, iTag *p_start = 0, _u32 index = 0) = 0;
};

class iXMLParser: public iHypertextParser {
public:
	DECLARE_INTERFACE(I_XML_PARSER, iXMLParser, iHypertextParser);
};

#endif

