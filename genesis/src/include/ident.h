/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_ident_h
#define cdc_ident_h

#define NOT_AN_IDENT -1

/* error id's */
extern Ident perm_id, type_id, div_id, integer_id, float_id, string_id, objnum_id;
extern Ident list_id, symbol_id, error_id, frob_id, unrecognized_id;
extern Ident methodnf_id, methoderr_id, parent_id, maxdepth_id, objnf_id;
extern Ident numargs_id, range_id, varnf_id, file_id, ticks_id, connect_id;
extern Ident disconnect_id, parse_id, startup_id, socket_id, bind_id;
extern Ident servnf_id, varexists_id, dictionary_id, keynf_id, address_id;
extern Ident refused_id, net_id, timeout_id, other_id, failed_id;
extern Ident heartbeat_id, regexp_id, buffer_id, namenf_id, salt_id;
extern Ident function_id, opcode_id, method_id, interpreter_id;
extern Ident directory_id, eof_id, backup_done_id;

extern Ident public_id, protected_id, private_id, root_id, driver_id;
extern Ident noover_id, sync_id, locked_id, native_id, fork_id, atomic_id;
extern Ident fpe_id;

/* method id's */
extern Ident signal_id;

void   init_ident(void);
Ident  ident_get(char *s);
void   ident_discard(Ident id);
Ident  ident_dup(Ident id);
char * ident_name(Ident id);

#endif

