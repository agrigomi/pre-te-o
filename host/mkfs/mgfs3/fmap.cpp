#include <stdio.h>
#include <sys/mman.h>
#include "mkfs.h"
#include "file.h"
#include "str.h"

char *fmap_line(char *file_ptr, _u32 *line_size, _u64 file_sz) {
	char *r = file_ptr;
	_u32 sz_line=0;
		
	while(file_ptr[sz_line] >= 0x20 && file_ptr[sz_line] < 0x7f && sz_line < file_sz)
		sz_line++;

	*line_size = sz_line;
	return r;
}

void parse_file_map(void) {
	char *argv[256];
	int argc;
	_u32 tmp_sz=0;
	char cmd_line[256];
	_u64 map_sz;
	_str_t fmap_name;

	if(clargs_option("-f", &fmap_name, &tmp_sz)) {
		char *fmap_ptr = (char *)map_file(fmap_name, &map_sz);
		if(fmap_ptr) {
			_u32 line_sz = 0;
			_u64 file_size = map_sz;
			char *fptr = fmap_ptr;
			while(file_size) {
				mem_set((_u8 *)cmd_line, 0, sizeof(cmd_line));

				char *ptr_line = fmap_line(fptr, &line_sz, file_size);
				if(!line_sz) 
					line_sz = 1;
				else {
					_str_t dev_name=0;
					if(clargs_option("-d", &dev_name, &tmp_sz)) {
						sprintf(cmd_line, "%s -d:%s ", (char *)clargs_parameter(0), dev_name);
						mem_cpy((_u8 *)cmd_line + str_len(cmd_line), (_u8 *)ptr_line, line_sz);
						_u32 cmd_sz = str_len(cmd_line);
						argc = 0;
						argv[argc]  = cmd_line;
						argc++;
						for(_u32 i = 0; i < cmd_sz; i++) {
							if(cmd_line[i] == ' ' && cmd_line[i+1] > ' ' && cmd_line[i+1] < 0x7f) {
								cmd_line[i] = 0;
								argv[argc] = &cmd_line[i+1];
								argc++;
							}							
						}

						if(clargs_parse(argc, argv, _g_options_)) {
							_str_t str_action = clargs_parameter(1);
							clrspc(str_action);
							if(str_action) {
								_u32 n = 0;
								while(_g_action_[n].action) {
									if(str_cmp((_str_t)_g_action_[n].action, str_action) == 0) {
										_g_action_[n].p_action();
										break;
									}

									n++;
								}

								if(!_g_action_[n].action) {
									TRACE("ERROR: Unknown command '%s'\n", str_action);
								}
							}
						}
					}
				}

				file_size -= line_sz;
				fptr += line_sz;
			}

			munmap(fmap_ptr, map_sz);
		}
	}
}

