/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include <string.h>
#include "util.h"

/* We use MALLOC_DELTA to keep the table sizes at least 32 bytes below a power
 * of two, assuming an Int is four bytes. */
/* HACKNOTE: BAD */
#define MALLOC_DELTA 8
#define INIT_TAB_SIZE (512 - MALLOC_DELTA)

typedef struct xident_entry {
    char *s;
    Int refs;
    uLong hash;
    Long next;
} xIdent_entry;

static xIdent_entry *tab;
static Long *hashtab;
static Long tab_size, blanks;

Ident perm_id, type_id, div_id, integer_id, float_id, string_id, objnum_id,
      list_id, symbol_id, error_id, frob_id, methodnf_id, methoderr_id,
      parent_id, maxdepth_id, objnf_id, numargs_id, range_id, varnf_id,
      file_id, ticks_id, connect_id, disconnect_id, startup_id, parse_id,
      socket_id, bind_id, servnf_id, varexists_id, dictionary_id, keynf_id,
      address_id, refused_id, net_id, timeout_id, other_id, failed_id,
      heartbeat_id, regexp_id, buffer_id, namenf_id, salt_id, function_id,
      opcode_id, method_id, interpreter_id, signal_id, directory_id, eof_id,
      backup_done_id;

Ident public_id, protected_id, private_id, root_id, driver_id, fpe_id, inf_id,
      noover_id, sync_id, locked_id, native_id, forked_id, atomic_id;

Ident SEEK_SET_id, SEEK_CUR_id, SEEK_END_id, preaddr_id, pretype_id;
Ident breadth_id, depth_id, full_id, partial_id;

/* limits */
Ident datasize_id, forkdepth_id, calldepth_id, recursion_id, objswap_id;     
Ident left_id, right_id, both_id;

void init_ident(void)
{
    Long i;

    tab_size = INIT_TAB_SIZE;

    tab = EMALLOC(xIdent_entry, tab_size);
    hashtab = EMALLOC(Long, tab_size);

    for (i = 0; i < tab_size; i++) {
	tab[i].s = NULL;
	tab[i].next = i + 1;
	hashtab[i] = -1;
    }
    tab[tab_size - 1].next = -1;

    blanks = 0;

    perm_id = ident_get("perm");
    type_id = ident_get("type");
    div_id = ident_get("div");
    integer_id = ident_get("integer");
    float_id = ident_get("float");
    fpe_id = ident_get("fpe");
    inf_id = ident_get("inf");
    string_id = ident_get("string");
    objnum_id = ident_get("objnum");
    list_id = ident_get("list");
    symbol_id = ident_get("symbol");
    error_id = ident_get("error");
    frob_id = ident_get("frob");
    methodnf_id = ident_get("methodnf");
    methoderr_id = ident_get("methoderr");
    parent_id = ident_get("parent");
    maxdepth_id = ident_get("maxdepth");
    objnf_id = ident_get("objnf");
    numargs_id = ident_get("numargs");
    range_id = ident_get("range");
    varnf_id = ident_get("varnf");
    file_id = ident_get("file");
    ticks_id = ident_get("ticks");
    connect_id = ident_get("connect");
    disconnect_id = ident_get("disconnect");
    parse_id = ident_get("parse");
    startup_id = ident_get("startup");
    socket_id = ident_get("socket");
    bind_id = ident_get("bind");
    servnf_id = ident_get("servnf");
    varexists_id = ident_get("varexists");
    dictionary_id = ident_get("dictionary");
    keynf_id = ident_get("keynf");
    address_id = ident_get("address");
    refused_id = ident_get("refused");
    net_id = ident_get("net");
    timeout_id = ident_get("timeout");
    other_id = ident_get("other");
    failed_id = ident_get("failed");
    heartbeat_id = ident_get("heartbeat");
    regexp_id = ident_get("regexp");
    buffer_id = ident_get("buffer");
    namenf_id = ident_get("namenf");
    salt_id = ident_get("salt");
    function_id = ident_get("function");
    opcode_id = ident_get("opcode");
    method_id = ident_get("method");
    interpreter_id = ident_get("interpreter");
    signal_id = ident_get("signal");
    directory_id = ident_get("directory");
    eof_id = ident_get("eof");
    public_id = ident_get("public");
    protected_id = ident_get("protected");
    private_id = ident_get("private");
    root_id = ident_get("root");
    driver_id = ident_get("driver");
    noover_id = ident_get("nooverride");
    forked_id = ident_get("forked");
    sync_id = ident_get("synchronized");
    locked_id = ident_get("locked");
    native_id = ident_get("native");
    atomic_id = ident_get("atomic");
    backup_done_id = ident_get("backup_done");
    SEEK_SET_id = ident_get("SEEK_SET");
    SEEK_CUR_id = ident_get("SEEK_CUR");
    SEEK_END_id = ident_get("SEEK_END");
    preaddr_id = ident_get("preaddr");
    pretype_id = ident_get("pretype");

    datasize_id = ident_get("datasize");
    forkdepth_id = ident_get("forkdepth");
    recursion_id = ident_get("recursion");
    objswap_id = ident_get("objswap");
    calldepth_id = ident_get("calldepth");

    left_id = ident_get("left");
    right_id = ident_get("right");
    both_id = ident_get("both");
    breadth_id = ident_get("breadth");
    depth_id = ident_get("depth");
    full_id = ident_get("full");
    partial_id = ident_get("partial");
}

INTERNAL Ident ident_from_hash(uLong hval, char * s, int len) {
    Long ind, new_size, i;

    /* Look for an existing identifier. */
    ind = hashtab[hval % tab_size];
    while (ind != -1) {
	if (tab[ind].hash == hval && strncmp(tab[ind].s, s, len) == 0) {
	    tab[ind].refs++;
	    return ind;
	}
	ind = tab[ind].next;
    }

    /* Check if we have to resize the table. */
    if (blanks == -1) {

	/* Allocate new space for table. */
	new_size = tab_size * 2 + MALLOC_DELTA;
	tab = EREALLOC(tab, xIdent_entry, new_size);
	hashtab = EREALLOC(hashtab, Long, new_size);

	/* Make new string of blanks. */
	for (i = tab_size; i < new_size - 1; i++)
	    tab[i].next = i + 1;
	tab[i].next = -1;
	blanks = tab_size;

	/* Reset hash table. */
	for (i = 0; i < new_size; i++)
	    hashtab[i] = -1;

	/* Install old symbols in hash table. */
	for (i = 0; i < tab_size; i++) {
	    ind = tab[i].hash % new_size;
	    tab[i].next = hashtab[ind];
	    hashtab[ind] = i;
	}

	tab_size = new_size;
    }

    /* Install symbol at first blank. */
    ind = blanks;
    blanks = tab[ind].next;
    tab[ind].s = tstrndup(s, len);
    tab[ind].hash = hval;
    tab[ind].refs = 1;
    tab[ind].next = hashtab[hval % tab_size];
    hashtab[hval % tab_size] = ind;

    return ind;
}

Ident ident_get(char *s) {
    uLong hval = hash_nullchar(s);

    return ident_from_hash(hval, s, strlen(s));
}

Ident ident_get_string(cStr * str) {
    uLong hval;

    hval = hash_string(str);

    return ident_from_hash(hval, string_chars(str), string_length(str));
}

void ident_discard(Ident id) {
    Long ind, *p;

    tab[id].refs--;

    if (!tab[id].refs) {
	/* Get the hash table thread for this entry. */
	ind = hash_nullchar(tab[id].s) % tab_size;

	/* Free the string. */
	tfree_chars(tab[id].s);
	tab[id].s = NULL;

	/* Find the pointer to this entry. */
	for (p = &hashtab[ind]; p && *p != id; p = &tab[*p].next);

	/* Remove this entry and add it to blanks. */
	*p = tab[id].next;
	tab[id].next = blanks;
	blanks = id;
    }
}

Ident ident_dup(Ident id) {
    tab[id].refs++;

    if (!tab[id].s)
      panic("ident_dup tried to duplicate freed name.");

    return id;
}

char *ident_name(Ident id) {
    return tab[id].s;
}

uLong ident_hash(Ident id) {
    return tab[id].hash;
}
