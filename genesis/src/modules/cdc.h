#ifndef _cdc_h_
#define _cdc_h_

#include "defs.h"
#include "cdc_pcode.h"

void init_cdc(int argc, char ** argv);
void uninit_cdc(void);

#ifndef _cdc_
extern module_t cdc_module;
#endif

NATIVE_METHOD(buflen);
NATIVE_METHOD(buf_replace);
NATIVE_METHOD(subbuf);
NATIVE_METHOD(buf_to_str);
NATIVE_METHOD(buf_to_strings);
NATIVE_METHOD(str_to_buf);
NATIVE_METHOD(strings_to_buf);
NATIVE_METHOD(dict_keys);
NATIVE_METHOD(dict_values);
NATIVE_METHOD(dict_add);
NATIVE_METHOD(dict_del);
NATIVE_METHOD(dict_add_elem);
NATIVE_METHOD(dict_del_elem);
NATIVE_METHOD(dict_contains);
NATIVE_METHOD(dict_union);
NATIVE_METHOD(listlen);
NATIVE_METHOD(sublist);
NATIVE_METHOD(insert);
NATIVE_METHOD(replace);
NATIVE_METHOD(delete);
NATIVE_METHOD(setadd);
NATIVE_METHOD(setremove);
NATIVE_METHOD(union);
NATIVE_METHOD(join);
NATIVE_METHOD(sort);
NATIVE_METHOD(strftime);
NATIVE_METHOD(next_objnum);
NATIVE_METHOD(status);
NATIVE_METHOD(version);
NATIVE_METHOD(hostname);
NATIVE_METHOD(ip);
NATIVE_METHOD(strlen);
NATIVE_METHOD(substr);
NATIVE_METHOD(explode);
NATIVE_METHOD(strsub);
NATIVE_METHOD(pad);
NATIVE_METHOD(match_begin);
NATIVE_METHOD(match_template);
NATIVE_METHOD(match_pattern);
NATIVE_METHOD(match_regexp);
NATIVE_METHOD(crypt);
NATIVE_METHOD(uppercase);
NATIVE_METHOD(lowercase);
NATIVE_METHOD(strcmp);
NATIVE_METHOD(strfmt);
NATIVE_METHOD(trim);
NATIVE_METHOD(split);
NATIVE_METHOD(regexp);
NATIVE_METHOD(strsed);
NATIVE_METHOD(capitalize);
NATIVE_METHOD(to_veil_pkts);
NATIVE_METHOD(from_veil_pkts);
NATIVE_METHOD(and);
NATIVE_METHOD(or);
NATIVE_METHOD(xor);
NATIVE_METHOD(shleft);
NATIVE_METHOD(shright);
NATIVE_METHOD(not);
NATIVE_METHOD(word);
NATIVE_METHOD(dbquote_explode);

#endif
