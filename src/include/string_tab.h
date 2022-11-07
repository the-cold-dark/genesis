/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef string_tab_h
#define string_tab_h

typedef struct _string_tab_entry
{
    cStr *str;
    Int refs;
    uLong hash;
    Long next;
} StringTabEntry;

typedef struct _string_tab
{
    StringTabEntry *tab;
    Long *hashtab;
    Long tab_size;
    Long tab_num;
    Long blanks;
} StringTab;

StringTab * string_tab_new(void);
StringTab * string_tab_new_with_size(Long size);
void        string_tab_fixup_hashtab(StringTab *tab, Long num);
void        string_tab_free(StringTab *tab);
Ident       string_tab_get(StringTab *tab, const char *s);
Ident       string_tab_get_length(StringTab *tab, const char *s, Int len);
Ident       string_tab_get_string(StringTab *tab, cStr * str);
void        string_tab_discard(StringTab *tab, Ident id);
Ident       string_tab_dup(StringTab *tab, Ident id);
char      * string_tab_name(const StringTab *tab, Ident id);
cStr      * string_tab_name_str(const StringTab *tab, Ident id);
char      * string_tab_name_size(const StringTab *tab, Ident id, Int *sz);
uLong       string_tab_hash(const StringTab *tab, Ident id);

#endif

