#ifndef _cdc_h_
#define _cdc_h_

#include "defs.h"
#include "modules.h"

void init_cdc(int argc, char ** argv);
void uninit_cdc(void);

#ifndef _modules_
module_t cdc_module = {init_cdc, uninit_cdc};
#else
extern module_t cdc_module;
#endif

void native_buffer_len(void);
void native_buffer_retrieve(void);
void native_buffer_append(void);
void native_buffer_replace(void);
void native_buffer_add(void);
void native_buffer_truncate(void);
void native_buffer_subrange(void);
void native_buffer_tail(void);
void native_buffer_to_string(void);
void native_buffer_to_strings(void);
void native_buffer_from_string(void);
void native_buffer_from_strings(void);
void native_dict_keys(void);
void native_dict_add(void);
void native_dict_del(void);
void native_dict_contains(void);
void native_listlen(void);
void native_sublist(void);
void native_insert(void);
void native_replace(void);
void native_delete(void);
void native_setadd(void);
void native_setremove(void);
void native_union(void);
void native_strftime(void);
void native_next_objnum(void);
void native_status(void);
void native_version(void);
void native_hostname(void);
void native_ip(void);
void native_strlen(void);
void native_substr(void);
void native_explode(void);
void native_strsub(void);
void native_pad(void);
void native_match_begin(void);
void native_match_template(void);
void native_match_pattern(void);
void native_match_regexp(void);
void native_crypt(void);
void native_uppercase(void);
void native_lowercase(void);
void native_strcmp(void);
void native_strfmt(void);
void native_strfmt(void);

#endif
