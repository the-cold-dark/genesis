/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_list_h
#define cdc_list_h

cList * list_new(Int len);
cList * list_dup(cList * list);
cData * list_empty_spaces(cList * list, Int spaces);
Int     list_search(cList * list, cData * data);
Int     list_binary_search(cList * list, cData * data, cData * key);
Int     list_cmp(cList * l1, cList * l2);
cList * list_insert(cList * list, Int pos, const cData * elem);
cList * list_add(cList * list, cData * elem);
cList * list_add_sorted(cList * list, cData * elem, cData * key);
cList * list_replace(cList * list, Int pos, const cData * elem);
cList * list_delete(cList * list, Int pos);
cList * list_delete_element(cList * list, cData * elem);
cList * list_delete_sorted_element(cList * list, cData * elem, cData * key);
cList * list_append(cList * list1, const cList * list2);
cList * list_reverse(cList * list);
cList * list_setadd(cList * list, cData * elem);
cList * list_setremove(cList * list, cData * elem);
cList * list_union(cList * list1, cList * list2);
cList * list_sublist(cList * list, Int start, Int len);
void    list_discard(cList * list);
cList * list_prep(cList * list, Int start, Int len);
cStr  * list_join(cList * list, const cStr * sep);
int     list_index(cList * list, cData * search, int origin);

inline Int list_length(const cList *list) {
    return list->len;
}

inline cData *list_first(cList *list) {
    return (list->len) ? list->el + list->start : NULL;
}

inline cData *list_next(cList *list, cData *d) {
    return (d < list->el + list->start + list->len - 1) ? d + 1 : NULL;
}

inline cData *list_last(cList *list) {
    return (list->len) ? list->el + list->start + list->len - 1 : NULL;
}

inline cData *list_prev(cList *list, cData *d) {
    return (d > list->el + list->start) ? d - 1 : NULL;
}

inline cData *list_elem(cList *list, Int i) {
    return list->el + list->start + i;
}

#endif

