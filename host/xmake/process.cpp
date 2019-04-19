#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "time.h"
#include "xmake.h"
#include "clargs.h"
#include "str.h"
#include "iHypertext.h"

#define MAX_TAG_LEN	64
#define MAX_FILE_NAME	256
#define MAX_FLAGS_LEN	2048
#define MAX_DEF_LEN	2048
#define MAX_INCL_LEN	2048

#define VTRACE(__fmt__, ...)  \
	(clargs_option("-V")) ? printf(__fmt__, __VA_ARGS__) : printf("")

FILE *_g_out_=0;

#define WRITE(__fmt__, ...) \
	fprintf(_g_out_, __fmt__, __VA_ARGS__)

_u8 *map_file(_str_t name,_u32 *sz) {
	_u8 *r = NULL;
	int fd = open(name,O_RDWR);
	unsigned long len;
	
	if(fd != -1) {
		len = lseek(fd,0,SEEK_END);
		lseek(fd,0,SEEK_SET);
		r = (_u8 *)mmap(NULL,len,PROT_WRITE,MAP_SHARED,fd,0);
		*sz = len;
	}
	
	return r;
}

bool create_output_file(void) {
	bool r = false;
	_s8 out_name[MAX_FILE_NAME];
	_str_t _out_opt;
	_u32 _out_opt_sz;

	if(clargs_option("-o", &_out_opt, &_out_opt_sz)) 
		str_cpy(out_name, _out_opt, sizeof(out_name)-1);
	else
		_snprintf(out_name, sizeof(out_name)-1, "%s.mk", clargs_parameter(1));

	_g_out_ = fopen(out_name, "w+");
	if(!_g_out_) {
		TRACE("Can't create output file '%s'\n", out_name);
	} else
		r = true;
		
	return r;
}

#define TAG_PROJECT		"project"
#define TAG_TARGET		"target"
#define TAG_TYPE		"type"
#define TAG_GROUP		"group"
#define TAG_BRANCH		"branch"
#define TAG_DEFINE		"define"
#define TAG_INCLUDE		"include"
#define TAG_TOOL		"tool"

#define PAR_TAG			"tag"
#define PAR_CMD			"cmd"
#define PAR_NAME		"name"
#define PAR_LOCATION		"location"
#define PAR_VALUE		"value"
#define PAR_EXT			"ext"
#define PAR_OUT			"out"
#define PAR_TOOL		"tool"
#define PAR_FLAGS		"flags"
#define PAR_FILE		"file"
#define PAR_DEPENDS		"depends"
#define PAR_BRANCH		"branch"
#define PAR_TYPE		"type"
#define PAR_GROUP		"group"


iXMLParser *_g_p_xml_=0;

void export_definitions(void) {
	iTag *p_project = _g_p_xml_->select(TAG_PROJECT);
	if(p_project) {
		_u32 idx=0;
		_s8 p1[MAX_DEF_LEN], p2[MAX_DEF_LEN], p3[MAX_DEF_LEN];

		// export project
		p_project->parameter_copy(PAR_NAME, p2, sizeof(p2));
		p_project->parameter_copy(PAR_LOCATION, p3, sizeof(p3));

		WRITE("%s.%s=%s\n", TAG_PROJECT, PAR_NAME, p2);
		WRITE("%s.%s=%s\n", TAG_PROJECT, PAR_LOCATION, p3);

		VTRACE("project name: '%s'\n", p2);

		// export tools
		VTRACE("tools:\n\t","");
		iTag *tag_tool = 0;
		while(tag_tool = _g_p_xml_->select(TAG_TOOL, p_project, idx)) {
			tag_tool->parameter_copy(PAR_TAG, p1, sizeof(p1));
			tag_tool->parameter_copy(PAR_CMD, p2, sizeof(p2));

			WRITE("%s=%s\n", p1,p2);
			VTRACE("%s, ",p2);

			idx++;
		}

		VTRACE("\n", "");

		// export global defines
		idx = 0;
		iTag *p_def = 0;
		while(p_def = _g_p_xml_->select(TAG_DEFINE, p_project, idx)) {
			p_def->parameter_copy(PAR_TAG, p1, sizeof(p1));
			p_def->parameter_copy(PAR_VALUE, p2, sizeof(p2));

			WRITE("%s=%s\n", p1,p2);

			idx++;
		}

		// export types
		VTRACE("file types:\n\t","");
		idx = 0;
		iTag *p_type = 0;
		while(p_type = _g_p_xml_->select(TAG_TYPE, p_project, idx)) {
			p_type->parameter_copy(PAR_TAG, p1, sizeof(p1));
			p_type->parameter_copy(PAR_EXT, p2, sizeof(p2));
			p_type->parameter_copy(PAR_OUT, p3, sizeof(p3));

			VTRACE("*%s,", p2);
			WRITE("%s.%s=%s\n", p1, PAR_EXT, p2);
			WRITE("%s.%s=%s\n", p1, PAR_OUT, p3);

			p_type->parameter_copy(PAR_TOOL, p2, sizeof(p2));
			p_type->parameter_copy(PAR_FLAGS, p3, sizeof(p3));

			WRITE("%s.%s=$(%s)\n", p1, PAR_TOOL, p2);
			WRITE("%s.%s=%s\n", p1, PAR_FLAGS, p3);

			idx++;
		}

		VTRACE("\n","");
	} else {
		TRACE("Can't select tag '%s'\n", TAG_PROJECT);
	}
}

void export_internal_defs(iTag *p_tag, _str_t tag_name) {
	_u32 idx=0;
	iTag *p_def=0;
	_s8 tag[MAX_TAG_LEN], value[MAX_DEF_LEN];

	while(p_def = _g_p_xml_->select(TAG_DEFINE, p_tag, idx)) {
		p_def->parameter_copy(PAR_TAG, tag, sizeof(tag));
		p_def->parameter_copy(PAR_VALUE, value, sizeof(value));

		WRITE("%s.%s=%s\n", tag_name, tag, value);

		idx++;
	}
}

void export_group_files(iTag *p_group, _str_t branch_name, _str_t group_name, _str_t branch_include_flags) {
	_u32 idx = 0;
	iTag *p_incl = 0;
	_s8 files[MAX_INCL_LEN], file[MAX_FILE_NAME], type[MAX_TAG_LEN], depends[MAX_DEF_LEN], flags[MAX_FLAGS_LEN];

	while(p_incl = _g_p_xml_->select(TAG_INCLUDE, p_group, idx)) {
		p_incl->parameter_copy(PAR_FILE, files, sizeof(files));
		p_incl->parameter_copy(PAR_TYPE, type, sizeof(type));
		p_incl->parameter_copy(PAR_DEPENDS, depends, sizeof(depends));
		p_incl->parameter_copy(PAR_FLAGS, flags, sizeof(flags));

		clrspc(files);
		
		while(div_str(files, file, sizeof(file), files, sizeof(files), ",")) {
			clrspc(files);
			clrspc(file);

			WRITE("\n$(%s.%s)/%s-%s$(%s.%s): $(%s.%s)/%s$(%s.%s) %s\n", 
						branch_name, PAR_OUT, group_name, file, type, PAR_OUT, 
						group_name, PAR_LOCATION, file, type, PAR_EXT, depends);
			
			WRITE("\t@echo [$(%s.%s)] $@\n", type, PAR_TOOL);
			WRITE("\t$(%s.%s) %s %s $(%s.%s) $(%s.%s) $(%s.%s) $(%s.%s)/%s$(%s.%s)\n\n", 
						type, PAR_TOOL, branch_include_flags, flags, type, PAR_FLAGS, 
						branch_name, PAR_FLAGS, group_name, PAR_FLAGS, group_name, PAR_LOCATION, 
						file, type, PAR_EXT);
		}

		idx++;
	}

}

void export_group_out_files(iTag *p_group, _str_t branch_name, _str_t group_name) {
	_u32 idx = 0;
	iTag *p_incl = 0;
	_s8 files[MAX_INCL_LEN], file[MAX_FILE_NAME], type[MAX_TAG_LEN];

	WRITE("%s.%s= ", group_name, PAR_OUT);
	while(p_incl = _g_p_xml_->select(TAG_INCLUDE, p_group, idx)) {
		p_incl->parameter_copy(PAR_FILE, files, sizeof(files));
		p_incl->parameter_copy(PAR_TYPE, type, sizeof(type));

		while(div_str(files, file, sizeof(file), files, sizeof(files), ",")) {
			clrspc(files);
			clrspc(file);
			if(str_len(file)) {
				WRITE("\\\n","");
				WRITE("\t$(%s.%s)/%s-%s$(%s.%s) ", branch_name, PAR_OUT, group_name, file, type, PAR_OUT);
			}
		}
		
		if(str_len(file)) {
			WRITE("\n","");
		}

		idx++;
	}
	
	WRITE("\n","");
}

void export_group_target(iTag *p_group, _str_t branch_name, _str_t group_name) {
	iTag *p_gtarget = _g_p_xml_->select(TAG_TARGET, p_group, 0);
	if(p_gtarget) {
		// this froup have a target
		_s8 name[MAX_TAG_LEN], type[MAX_TAG_LEN], flags[MAX_FLAGS_LEN];

		p_gtarget->parameter_copy(PAR_NAME, name, sizeof(name));
		p_gtarget->parameter_copy(PAR_TYPE, type, sizeof(type));
		p_gtarget->parameter_copy(PAR_FLAGS, flags, sizeof(flags));

		WRITE("\n%s.%s=$(%s.%s)/%s$(%s.%s)\n\n", group_name, TAG_TARGET, branch_name, PAR_OUT, name, type, PAR_OUT);
		WRITE("$(%s.%s)/%s$(%s.%s): $(%s.%s)\n", branch_name, PAR_OUT, name, type, PAR_OUT,
							group_name, PAR_OUT);
		WRITE("\t@echo [$(%s.%s)] $@\n", type, PAR_TOOL);
		WRITE("\t$(%s.%s) %s $(%s.%s) $(%s.%s)\n\n", type, PAR_TOOL, flags, type, PAR_FLAGS, group_name, PAR_OUT);
	} else {
		WRITE("\n%s.%s=$(%s.%s)\n", group_name, TAG_TARGET, group_name, PAR_OUT);
	}
}

void export_group(iTag *p_project, _str_t branch_name, _str_t group_name, _str_t branch_include_flags) {
	_u32 idx = 0;
	iTag *p_group = 0;
	_s8 tag[MAX_TAG_LEN]="";

	while(p_group = _g_p_xml_->select(TAG_GROUP, p_project, idx)) {
		p_group->parameter_copy(PAR_TAG, tag, sizeof(tag));
		if(str_cmp(tag, group_name) == 0) {
			_s8 location[MAX_FILE_NAME], flags[MAX_FLAGS_LEN];

			p_group->parameter_copy(PAR_LOCATION, location, sizeof(location));
			p_group->parameter_copy(PAR_FLAGS, flags, sizeof(flags));

			WRITE("\n### group: '%s' ###\n", group_name);
			WRITE("%s.%s=%s\n", tag, PAR_LOCATION, location);
			WRITE("%s.%s=%s\n", tag, PAR_FLAGS, flags);
			break;
		}

		idx++;
	}

	if(p_group) {
		export_internal_defs(p_group, tag);
		export_group_out_files(p_group, branch_name, group_name);
		export_group_target(p_group, branch_name, group_name);
		export_group_files(p_group, branch_name, group_name, branch_include_flags);
	} else {
		TRACE("ERROR: the group '%s' cannot be found\n", group_name);
	}
}

void export_branch_groups(iTag *p_project, iTag *p_branch, _str_t branch_name) {
	_u32 idx = 0;
	iTag *p_incl = 0;
	_s8 incl[MAX_INCL_LEN], group[MAX_TAG_LEN], flags[MAX_FLAGS_LEN];

	VTRACE("groups: \n","");
	while(p_incl = _g_p_xml_->select(TAG_INCLUDE, p_branch, idx)) {
		p_incl->parameter_copy(PAR_GROUP, incl, sizeof(incl));
		p_incl->parameter_copy(PAR_FLAGS, flags, sizeof(flags));
		VTRACE("\t%s\n", incl);
		clrspc(incl);
		while(div_str(incl, group, sizeof(group), incl, sizeof(incl), ",")) {
			clrspc(incl);
			clrspc(group);

			export_group(p_project, branch_name, group, flags);
		}

		idx++;
	}
}

void export_branch_all(iTag *p_branch, _str_t branch_name) {
	_u32 idx = 0;
	iTag *p_incl = 0;
	_s8 incl[MAX_INCL_LEN], group[MAX_TAG_LEN];

	WRITE("%s.all= ", branch_name);
	while(p_incl = _g_p_xml_->select(TAG_INCLUDE, p_branch, idx)) {
		p_incl->parameter_copy(PAR_GROUP, incl, sizeof(incl));

		while(div_str(incl, group, sizeof(group), incl, sizeof(incl), ",")) {
			clrspc(incl);
			clrspc(group);
			if(str_len(group)) {
				WRITE("\\\n","");
				WRITE("\t$(%s.%s) ", group, TAG_TARGET);
			}
		}

		if(str_len(group)) {
			WRITE("\n","");
		}

		idx++;
	}

	WRITE("\n\n","");
}

bool export_branch(iTag *p_project, _str_t branch_name) {
	bool r = false;
	_u32 idx=0;
	iTag *p_branch=0;
	_s8 tag[MAX_TAG_LEN]="", out[MAX_FILE_NAME]="", flags[MAX_FLAGS_LEN]="";
	
	while(p_branch = _g_p_xml_->select(TAG_BRANCH, p_project, idx)) {
		p_branch->parameter_copy(PAR_TAG, tag, sizeof(tag));
		if(str_cmp(branch_name, tag) == 0)
			break;

		idx++;
	}

	if(p_branch) {
		VTRACE("branch: '%s'\n", tag);

		p_branch->parameter_copy(PAR_OUT, out, sizeof(out));
		p_branch->parameter_copy(PAR_FLAGS, flags, sizeof(flags));
		
		WRITE("\n### branch '%s' ###\n\n", branch_name);
		WRITE("branch=%s\n", branch_name);
		WRITE("branch.%s=%s\n", PAR_OUT, out);
		WRITE("branch.%s=%s\n", PAR_FLAGS, flags);
		WRITE("%s.%s=%s\n", tag, PAR_OUT, out);
		WRITE("%s.%s=%s\n", tag, PAR_FLAGS, flags);

		export_internal_defs(p_branch, tag);
		export_branch_groups(p_project, p_branch, tag);
		export_branch_all(p_branch, tag);

		// export branch target
		_s8 tname[MAX_TAG_LEN], ttype[MAX_TAG_LEN];
		iTag *p_btarget = _g_p_xml_->select(TAG_TARGET, p_branch, 0);
		
		if(p_btarget) {
			p_btarget->parameter_copy(PAR_NAME, tname, sizeof(tname));
			p_btarget->parameter_copy(PAR_TYPE, ttype, sizeof(ttype));
			p_btarget->parameter_copy(PAR_FLAGS, flags, sizeof(flags));

			WRITE("\n%s.%s=$(%s.%s)/%s$(%s.%s)\n\n", tag, TAG_TARGET, branch_name, PAR_OUT, tname, ttype, PAR_OUT);
			WRITE("$(%s.%s)/%s$(%s.%s): $(%s.all)\n", tag, PAR_OUT, tname, ttype, PAR_OUT, tag);
			WRITE("\t@echo [$(%s.%s)] $@\n", ttype, PAR_TOOL);
			WRITE("\t$(%s.%s) $(%s.%s) %s $(%s.all)\n\n", ttype, PAR_TOOL, ttype, PAR_FLAGS, flags, tag);
			r = true;
		} else {
			WRITE("%s.%s=$(%s.all)\n", tag, TAG_TARGET, tag);
			r = true;
		}
	} else {
		TRACE("ERROR: the branch '%s' cannot be found\n", branch_name);
	}

	return r;
}

bool export_target(iTag *p_project, iTag *p_target) {
	bool r = false;
	_u32 idx = 0;
	iTag *p_branch = 0;
	_s8 name[MAX_TAG_LEN], type[MAX_TAG_LEN], branch[MAX_TAG_LEN], out[MAX_FILE_NAME], flags[MAX_FLAGS_LEN];
	_str_t branch_opt=0;
	_u32 sz_branch_opt=0;

	p_target->parameter_copy(PAR_NAME, name, sizeof(name));
	p_target->parameter_copy(PAR_TYPE, type, sizeof(type));
	if(clargs_option("-b", &branch_opt, &sz_branch_opt)) {
		if(!strlen(branch_opt))
			goto _use_target_branch_;

		str_cpy(branch, branch_opt, sizeof(branch));
	} else {
_use_target_branch_:
		p_target->parameter_copy(PAR_BRANCH, branch, sizeof(branch));
	}
	p_target->parameter_copy(PAR_OUT, out, sizeof(out));
	p_target->parameter_copy(PAR_FLAGS, flags, sizeof(flags));

	WRITE("%s=%s\n", TAG_TARGET, name);
	WRITE("%s.%s=%s\n", TAG_TARGET, PAR_FLAGS, flags);
	VTRACE("target: '%s'\n", name);
	if(export_branch(p_project, branch)) {
		WRITE("%s/%s$(%s.%s): $(%s.%s)\n", out, name, type, PAR_OUT, branch, TAG_TARGET);
		WRITE("\t@echo [$(%s.%s)] $@\n", type, PAR_TOOL);
		WRITE("\t$(%s.%s) $(%s.%s) %s $(%s.%s)\n\n", type, PAR_TOOL, branch, TAG_TARGET, flags, type, PAR_FLAGS);
		WRITE("$(%s): %s/%s$(%s.%s)\n", TAG_TARGET, out, name, type, PAR_OUT);
		r = true;
	}

	return r;
}

void export_rules(void) {
	iTag *p_project = _g_p_xml_->select(TAG_PROJECT);
	if(p_project) {
		_u32 idx=0;
		iTag *p_target=0;

		_str_t str_target=0;
		_u32 sz_str_target=0;

		if(clargs_option("-t", &str_target, &sz_str_target)) {
			while(p_target = _g_p_xml_->select(TAG_TARGET, p_project, idx)) {
				_u32 psz=0;
				_str_t pname = p_target->parameter(PAR_NAME, &psz);
				if(pname) {
					if(psz == sz_str_target && str_ncmp(str_target, pname, sz_str_target) == 0)
						// found the requested target
						break;
				}

				idx++;
			}

			if(p_target) 
				export_target(p_project, p_target);
			else {
				TRACE("ERROR: target '%s' cannot be found\n", str_target);
			}
		} else {
			TRACE("missing target parameter\n","");
		}
	}
}

void xmake_process(void) {
	_u32 sz_input = 0;

	_u8 *p_input = map_file(clargs_parameter(1), &sz_input);
	if(p_input) {
		VTRACE("processing: %s\n", clargs_parameter(1));

		if(create_output_file()) {
			WRITE("# Generated by xmake tool\n\n", "");

			_g_p_xml_ = (iXMLParser *)__g_p_repository__->get_object_by_interface(I_XML_PARSER, RY_CLONE);
			if(_g_p_xml_) {
				_g_p_xml_->init((_str_t)p_input, sz_input);
				_g_p_xml_->parse();
				export_definitions();
				export_rules();
				_g_p_xml_->clear();
				__g_p_repository__->release_object(_g_p_xml_);
			} else {
				TRACE("ERROR: can't find XML parser\n", "");
			}

			fclose(_g_out_);
		}

		munmap(p_input, sz_input);
	} else {
		TRACE("ERROR: can't open input file '%s'\n", clargs_parameter(1));
	}
}

