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
Int         string_tab_get(StringTab *tab, char *s);
Int         string_tab_get_string(StringTab *tab, cStr * str);
void        string_tab_discard(StringTab *tab, Int id);
Int         string_tab_dup(StringTab *tab, Int id);
char      * string_tab_name(StringTab *tab, Int id);
cStr      * string_tab_name_str(StringTab *tab, Int id);
uLong       string_tab_hash(StringTab *tab, Int id);

#endif

