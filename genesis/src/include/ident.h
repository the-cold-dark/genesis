/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_ident_h
#define cdc_ident_h

#include "string_tab.h"

#define NOT_AN_IDENT -1

/* error id's */
extern Ident perm_id, type_id, div_id, integer_id, float_id, string_id, objnum_id;
extern Ident list_id, symbol_id, error_id, frob_id, unrecognized_id;
extern Ident methodnf_id, methoderr_id, parent_id, maxdepth_id, objnf_id;
extern Ident numargs_id, range_id, varnf_id, file_id, ticks_id, connect_id;
extern Ident disconnect_id, parse_id, startup_id, socket_id, bind_id;
extern Ident servnf_id, varexists_id, dictionary_id, keynf_id, address_id;
extern Ident refused_id, net_id, timeout_id, other_id, failed_id;
extern Ident heartbeat_id, regexp_id, buffer_id, object_id, namenf_id, salt_id;
extern Ident function_id, opcode_id, method_id, interpreter_id;
extern Ident directory_id, eof_id, backup_done_id;

extern Ident public_id, protected_id, private_id, root_id, driver_id;
extern Ident noover_id, sync_id, locked_id, native_id, forked_id, atomic_id;
extern Ident fpe_id, inf_id, preaddr_id, pretype_id;

extern Ident SEEK_SET_id, SEEK_CUR_id, SEEK_END_id;
extern Ident breadth_id, depth_id, full_id, partial_id;

/* limit idents */
extern Ident datasize_id, forkdepth_id, calldepth_id, recursion_id, objswap_id;

/* driver config idents */
extern Ident cachelog_id, cachewatch_id, cachewatchcount_id, cleanerwait_id, cleanerignore_id;
extern Ident log_malloc_size_id, log_method_cache_id, cache_history_size_id;

/* cache stats options */
extern Ident ancestor_cache_id, method_cache_id, name_cache_id, object_cache_id;

/* method id's */
extern Ident signal_id;

/* used by cdc_string, set here incase they are needed elsewhere */
extern Ident left_id, right_id, both_id;

extern StringTab *idents;

void   init_ident(void);

#define ident_get(s)		string_tab_get(idents, s)
#define ident_get_length(s,len)	string_tab_get_length(idents, s, len)
#define ident_get_string(str)	string_tab_get_string(idents, str)
#define ident_discard(id)	string_tab_discard(idents, id)
#define ident_dup(id)		string_tab_dup(idents, id)
#define ident_name(id)		string_tab_name(idents, id)
#define ident_name_size(id, sz) string_tab_name_size(idents, id, sz)
#define ident_name_str(id)      string_tab_name_str(idents, id)
#define ident_hash(id)		string_tab_hash(idents, id)

#endif

