/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: include/old_ops.h
// ---
//
*/

#ifndef _oldops_
#define _oldops_

void op_buffer_len(void);
void op_buffer_retrieve(void);
void op_buffer_append(void);
void op_buffer_replace(void);
void op_buffer_add(void);
void op_buffer_truncate(void);
void op_buffer_tail(void);
void op_buffer_to_string(void);
void op_buffer_from_string(void);
void op_buffer_to_strings(void);
void op_buffer_from_strings(void);
void op_dict_keys(void);
void op_dict_add(void);
void op_dict_del(void);
void op_dict_contains(void);
void op_random(void);
void op_max(void);
void op_min(void);
void op_abs(void);
void op_listlen(void);
void op_sublist(void);
void op_insert(void);
void op_replace(void);
void op_delete(void);
void op_setadd(void);
void op_setremove(void);
void op_union(void);
void op_hostname(void);
void op_ip(void);
void op_strfmt(void);
void op_strlen(void);
void op_substr(void);
void op_explode(void);
void op_strsub(void);
void op_pad(void);
void op_match_begin(void);
void op_match_template(void);
void op_match_pattern(void);
void op_match_regexp(void);
void op_crypt(void);
void op_uppercase(void);
void op_lowercase(void);
void op_strcmp(void);
void op_strfmt(void);
void op_next_objnum(void);
void op_status(void);
void op_version(void);
void op_time(void);
void op_localtime(void);
void op_strftime(void);
void op_mtime(void);
void op_ctime(void);
void op_tokenize_cml(void);
void op_buf_to_veil_packets(void);
void op_buf_from_veil_packets(void);
void native_decode(void);
void native_encode(void);

#endif
