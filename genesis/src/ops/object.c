/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Function operators
//
// This file contains functions inherent to the system, which are actually
// operators, but nobody needs to know.
//
// Many of these functions require information from the current frame,
// which is why they are not modularized (such as object functions) or
// they are inherent to the functionality of ColdC
//
// The need to split these into seperate files is not too great, as they
// will not be changing often.
*/

#include "defs.h"

#include "cdc_pcode.h"
#include "cdc_db.h"

COLDC_FUNC(add_var) {
    cData * args;
    Long     result;

    /* Accept a symbol argument a data value to assign to the variable. */
    if (!func_init_1(&args, SYMBOL))
        return;

    result = object_add_var(cur_frame->object, args[0].u.symbol);
    if (result == varexists_id)
        THROW((varexists_id,
              "Object variable %I already exists.", args[0].u.symbol))

    pop(1);
    push_int(1);
}

COLDC_FUNC(del_var) {
    cData * args;
    Long     result;

    /* Accept one symbol argument. */
    if (!func_init_1(&args, SYMBOL))
        return;

    result = object_del_var(cur_frame->object, args[0].u.symbol);
    if (result == varnf_id) {
        cthrow(varnf_id, "Object variable %I does not exist.", args[0].u.symbol);
    } else {
        pop(1);
        push_int(1);
    }
}

COLDC_FUNC(variables) {
    cList   * vars;
    Obj * obj;
    Int        i;
    Var      * var;
    cData     d;

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    /* Construct the list of variable names. */
    obj = cur_frame->object;
    vars = list_new(0);
    d.type = SYMBOL;
    for (i = 0; i < obj->vars.size; i++) {
        var = &obj->vars.tab[i];
        if (var->name != -1 && var->cclass == obj->objnum) {
            d.u.symbol = var->name;
            vars = list_add(vars, &d);
        }
    }

    /* Push the list onto the stack. */
    push_list(vars);
    list_discard(vars);
}

COLDC_FUNC(set_var) {
    cData * args,
             d;
    Long     result;

    /* Accept a symbol for the variable name and a data value of any type. */
    if (!func_init_2(&args, SYMBOL, 0))
        return;

    result = object_assign_var(cur_frame->object, cur_frame->method->object,
        		       args[0].u.symbol, &args[1]);
    if (result == varnf_id) {
        cthrow(varnf_id, "Object variable %I does not exist.", args[0].u.symbol);
    } else {
        /* This is just a stupid way of returning args[1] */
        data_dup(&d, &args[1]);
        pop(2);
        data_dup(&stack[stack_pos++], &d);
        data_discard(&d);
    }
}

COLDC_FUNC(get_var) {
    cData * args,
             d;
    Long     result;

    /* Accept a symbol argument. */
    if (!func_init_1(&args, SYMBOL))
        return;

    result = object_retrieve_var(cur_frame->object, cur_frame->method->object,
        			 args[0].u.symbol, &d);
    if (result == varnf_id) {
        cthrow(varnf_id, "Object variable %I does not exist.", args[0].u.symbol);
    } else {
        pop(1);
        data_dup(&stack[stack_pos++], &d);
        data_discard(&d);
    }
}

COLDC_FUNC(default_var) {
    cData * args,
            d;
    Long    result;

    /* Accept a symbol argument. */
    if (!func_init_1(&args, SYMBOL))
        return;

    result = object_default_var(cur_frame->object, cur_frame->method->object,
                                SYM1, &d);
    if (result == varnf_id) {
        cthrow(varnf_id, "Object variable %I does not exist.", SYM1);
    } else {
        pop(1);
        data_dup(&stack[stack_pos++], &d);
        data_discard(&d);
    }
}

COLDC_FUNC(inherited_var) {
    cData * args,
            d;
    Long    result;

    /* Accept a symbol argument. */
    if (!func_init_1(&args, SYMBOL))
        return;

    result = object_inherited_var(cur_frame->object, 
                                cur_frame->method->object, SYM1, &d);
    if (result == varnf_id) {
        cthrow(varnf_id, "Object variable %I does not exist.", SYM1);
    } else {
        pop(1);
        data_dup(&stack[stack_pos++], &d);
        data_discard(&d);
    }
}

COLDC_FUNC(clear_var) {
    cData * args;
    Long     result = 0;

    /* Accept a symbol argument. */
    if (!func_init_1(&args, SYMBOL))
        return;

    /* if this is the definer, ignore clear, will be wrong if the method
       doesn't exist, as it doesn't do lookup, but *shrug* */
    if (cur_frame->object != cur_frame->method->object) {
        result = object_delete_var(cur_frame->object,
                                   cur_frame->method->object,
                                   args[0].u.symbol);
    }

    if (result == varnf_id) {
        cthrow(varnf_id, "Object variable %I does not exist.", args[0].u.symbol);
    } else {
        pop(1);
        push_int(1);
    }
}

COLDC_FUNC(add_method) {
    cData   * args,
            * d;
    Method  * method;
    cList   * code,
            * errors;
    Int       flags=-1, access=-1, native=-1;
    char    * name;

    /* Accept a list of lines of code and a symbol for the name. */
    if (!func_init_2(&args, LIST, SYMBOL))
        return;

    name = ident_name(SYM2);
    if (is_reserved_word(name))
        THROW((parse_id,
              "%I is a reserved word, and cannot be used as a method name",
              SYM2))

    method = object_find_method_local(cur_frame->object, args[1].u.symbol, FROB_ANY);

    if (method && (method->m_flags & MF_LOCK))
        THROW((perm_id, "Method is locked, and cannot be changed."))

    /* keep these for later reference, if its already around */
    if (method) {
        flags = method->m_flags;
        access = method->m_access;
        native = method->native;
    /*    cache_discard(method->object); */
    }

    code = args[0].u.list;

    /* Make sure that every element in the code list is a string. */
    for (d = list_first(code); d; d = list_next(code, d)) {
        if (d->type != STRING) {
            cthrow(type_id, "Line %d (%D) is not a string.",
        	  d - list_first(code), d);
            return;
        }
    }

    method = compile(cur_frame->object, code, &errors);
    if (method) {
        if (flags != -1)
            method->m_flags = flags;
        if (access != -1)
            method->m_access = access;
        method->native = native;
        object_add_method(cur_frame->object, args[1].u.symbol, method);
        method_discard(method);
    }

    pop(2);
    push_list(errors);
    list_discard(errors);
}

COLDC_FUNC(rename_method) {
    cData   * args;
    Method * method;

    if (!func_init_2(&args, SYMBOL, SYMBOL))
        return;

    method = object_find_method_local(cur_frame->object, SYM2, FROB_ANY);

    if (method != NULL) {
        cthrow(method_id, "Method %I already exists!", SYM2);
        return;
    }

    if (!object_rename_method(cur_frame->object, SYM1, SYM2)) {
        cthrow(methodnf_id, "Method not found.");
        return;
    }

    pop(2);
    push_int(1);
}

#define LADD(__s) { \
        d.u.symbol = __s; \
        list = list_add(list, &d); \
    }

INTERNAL cList * list_method_flags(Int flags) {
    cList * list;
    cData d;

    if (flags == F_FAILURE)
        flags = MF_NONE;

    list = list_new(0);
    d.type = SYMBOL;
    if (flags & MF_NOOVER)
        LADD(noover_id);
    if (flags & MF_SYNC)
        LADD(sync_id);
    if (flags & MF_LOCK)
        LADD(locked_id);
    if (flags & MF_FORK)
        LADD(forked_id);
    if (flags & MF_NATIVE)
        LADD(native_id);

    return list;
}

#undef LADD

COLDC_FUNC(method_flags) {
    cData  * args;
    cList  * list;

    if (!func_init_1(&args, SYMBOL))
        return;

    list = list_method_flags(object_get_method_flags(cur_frame->object, args[0].u.symbol));

    pop(1);
    push_list(list);
    list_discard(list);
}

COLDC_FUNC(set_method_flags) {
    cData  * args,
            * d;
    cList  * list;
    Int       flags,
              new_flags = MF_NONE;

    if (!func_init_2(&args, SYMBOL, LIST))
        return;

    flags = object_get_method_flags(cur_frame->object, args[0].u.symbol);
    if (flags == -1)
        THROW((methodnf_id, "Method not found."))
    if (flags & MF_LOCK)
        THROW((perm_id, "Method is locked and cannot be changed."))
    if (flags & MF_NATIVE)
        THROW((perm_id,"Method is native and cannot be changed."))

    list = args[1].u.list;
    for (d = list_first(list); d; d = list_next(list, d)) {
        if (d->type != SYMBOL)
            THROW((type_id, "Invalid method flag (%D).", d))
        if (d->u.symbol == noover_id)
            new_flags |= MF_NOOVER;
        else if (d->u.symbol == sync_id)
            new_flags |= MF_SYNC;
        else if (d->u.symbol == locked_id)
            new_flags |= MF_LOCK;
        else if (d->u.symbol == forked_id)
            new_flags |= MF_FORK;
        else if (d->u.symbol == native_id)
            THROW((perm_id, "Native flag can only be set by the driver."))
        else
            THROW((perm_id, "Unknown method flag (%D).", d))
    }

    object_set_method_flags(cur_frame->object, args[0].u.symbol, new_flags);

    pop(2);
    push_int((cNum) new_flags);
}

COLDC_FUNC(method_access) {
    Int       access;
    cData  * args;

    if (!func_init_1(&args, SYMBOL))
        return;

    access = object_get_method_access(cur_frame->object, args[0].u.symbol);

    pop(1);

    switch(access) {
        case MS_PUBLIC:    push_symbol(public_id);    break;
        case MS_PROTECTED: push_symbol(protected_id); break;
        case MS_PRIVATE:   push_symbol(private_id);   break;
        case MS_ROOT:      push_symbol(root_id);      break;
        case MS_DRIVER:    push_symbol(driver_id);    break;
        case MS_FROB:      push_symbol(frob_id);      break;
        default:           push_int(0);               break;
    }
}

COLDC_FUNC(set_method_access) {
    Int       access = 0;
    cData   * args;
    Ident     sym;

    if (!func_init_2(&args, SYMBOL, SYMBOL))
        return;

    sym = SYM2;
    if (sym == public_id)
        access = MS_PUBLIC;
    else if (sym == protected_id)
        access = MS_PROTECTED;
    else if (sym == private_id)
        access = MS_PRIVATE;
    else if (sym == root_id)
        access = MS_ROOT;
    else if (sym == driver_id)
        access = MS_DRIVER;
    else if (sym == frob_id)
        access = MS_FROB;
    else
        THROW((type_id, "Invalid method access flag."));

    access = object_set_method_access(cur_frame->object, SYM1, access);

    if (access == -1)
        THROW((type_id, "Method %I not found.", SYM1));

    pop(2);
    push_int(access);
}

COLDC_FUNC(method_info) {
    cData   * args,
             * list;
    cList   * output;
    Method * method;
    cStr * str;
    char     * s;
    Int        i;

    /* A symbol for the Method name. */
    if (!func_init_1(&args, SYMBOL))
        return;

    method = object_find_method_local(cur_frame->object, args[0].u.symbol, FROB_ANY);
    if (!method) {
        cthrow(methodnf_id, "Method not found.");
        return;
    }

    /* initialize the list */
    output = list_new(6);
    list = list_empty_spaces(output, 6);

    /* build up the args list (string) */
    str = string_new(0);
    if (method->num_args || method->rest != -1) {
        for (i = method->num_args - 1; i >= 0; i--) {
            s = ident_name(object_get_ident(method->object, method->argnames[i]));
            str = string_add_chars(str, s, strlen(s));
            if (i > 0 || method->rest != -1)
                str = string_add_chars(str, ", ", 2);
        }
        if (method->rest != -1) {
            str = string_addc(str, '@');
            s = ident_name(object_get_ident(method->object, method->rest));
            str = string_add_chars(str, s, strlen(s));
        }
    }

    list[0].type = STRING;
    list[0].u.str = str;
    list[1].type = INTEGER;
    list[1].u.val = method->num_args;
    list[2].type = INTEGER;
    list[2].u.val = method->num_vars;
    list[3].type = INTEGER;
    list[3].u.val = method->num_opcodes;

    list[4].type = SYMBOL;
    switch(method->m_access) {
        case MS_PUBLIC:    list[4].u.symbol = ident_dup(public_id);    break;
        case MS_PROTECTED: list[4].u.symbol = ident_dup(protected_id); break;
        case MS_PRIVATE:   list[4].u.symbol = ident_dup(private_id);   break;
        case MS_ROOT:      list[4].u.symbol = ident_dup(root_id);      break;
        case MS_DRIVER:    list[4].u.symbol = ident_dup(driver_id);    break;
        case MS_FROB:      list[4].u.symbol = ident_dup(frob_id);      break;
        default:           list[4].type = INTEGER; list[4].u.val = 0;  break;
    }

    list[5].type = LIST;
    list[5].u.list = list_method_flags(method->m_flags);

    pop(1);
    cache_discard(method->object);
    push_list(output);
    list_discard(output);
}

COLDC_FUNC(methods) {
    cList   * methods;
    cData     d;
    Obj * obj;
    Int        i;

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    /* Construct the list of method names. */
    obj = cur_frame->object;
    methods = list_new(obj->methods.size);
    for (i = 0; i < obj->methods.size; i++) {
        if (obj->methods.tab[i].m) {
            d.type = SYMBOL;
            d.u.symbol = obj->methods.tab[i].m->name;
            methods = list_add(methods, &d);
        }
    }

    /* Push the list onto the stack. */
    check_stack(1);
    push_list(methods);
    list_discard(methods);
}

COLDC_FUNC(find_method) {
    cData   * args;
    Method * m, * m2;

    /* Accept a symbol argument giving the method name. */
    if (!func_init_1(&args, SYMBOL))
        return;

    /* Look for the method on the current object. */
    m = object_find_method(cur_frame->object->objnum, SYM1, FROB_YES);
    m2 = object_find_method(cur_frame->object->objnum, SYM1, FROB_NO);
    if (!m) {
        m = m2;
    } else if (m2) {
        if (object_has_ancestor(m2->object->objnum, m->object->objnum)) {
            cache_discard(m->object);
            m = m2;
        } else {
            cache_discard(m2->object);
        }
    }

    pop(1);
    if (m) {
        push_objnum(m->object->objnum);
        cache_discard(m->object);
    } else {
        cthrow(methodnf_id, "Method %I not found.", args[0].u.symbol);
    }
}

COLDC_FUNC(find_next_method) {
    cData   * args;
    Method * m, * m2;

    /* Accept a symbol argument giving the method name, and a objnum giving the
     * object to search past. */
    if (!func_init_2(&args, SYMBOL, OBJNUM))
        return;

    /* Look for the method on the current object. */
    m = object_find_next_method(cur_frame->object->objnum,
                                SYM1, OBJNUM2, FROB_YES);
    m2 = object_find_next_method(cur_frame->object->objnum,
                                 SYM1, OBJNUM2, FROB_NO);
    if (!m) {
        m = m2;
    } else if (m2) {
        if (object_has_ancestor(m2->object->objnum, m->object->objnum)) {
            cache_discard(m->object);
            m = m2;
        } else {
            cache_discard(m2->object);
        }
    }

    if (m) {
        push_objnum(m->object->objnum);
        cache_discard(m->object);
    } else {
        cthrow(methodnf_id, "Method %I not found.", args[0].u.symbol);
    }
}

COLDC_FUNC(list_method) {
    Int      argc,
             indent;
    int      format_flags = FMT_DEFAULT;
    cData * args;
    cList * code;

    /* Accept a symbol for the method name, an optional integer for the
     * indentation, and an optional integer to specify full
     * parenthesization. */
    if (!func_init_1_to_3(&args, &argc, SYMBOL, INTEGER, INTEGER))
        return;

    indent = (argc >= 2) ? INT2 : DEFAULT_INDENT;
    if (indent < 1)
        THROW((type_id, "Invalid indentation %d, must be one or more.", INT2))
    if (argc == 3) {
        if (INT3 & FMT_FULL_PARENS)
            format_flags |= FMT_FULL_PARENS;
        if (INT3 & FMT_FULL_BRACES)
            format_flags |= FMT_FULL_BRACES;
    }

    code = object_list_method(cur_frame->object, SYM1, indent, format_flags);

    if (code) {
        pop(argc);
        push_list(code);
        list_discard(code);
    } else {
        cthrow(methodnf_id, "Method %I not found.", SYM1);
    }
}

COLDC_FUNC(del_method) {
    cData * args;
    Int      status;

    /* Accept a symbol for the method name. */
    if (!func_init_1(&args, SYMBOL))
        return;

    status = object_del_method(cur_frame->object, args[0].u.symbol);
    if (status == 0) {
        cthrow(methodnf_id, "No method named %I was found.", args[0].u.symbol);
    } else if (status == -1) {
        cthrow(perm_id, "Method is locked, and cannot be removed.");
    } else {
        pop(1);
        push_int(1);
    }
}

COLDC_FUNC(parents) {
    /* Accept no arguments. */
    if (!func_init_0())
        return;

    /* Push the parents list onto the stack. */
    push_list(cur_frame->object->parents);
}

COLDC_FUNC(children) {
    /* Accept no arguments. */
    if (!func_init_0())
        return;

    /* Push the children list onto the stack. */
    push_list(cur_frame->object->children);
}

COLDC_FUNC(ancestors) {
    cList * ancestors;
    cData * args;
    Int     argc;

    /* Accept no arguments. */
    if (!func_init_0_or_1(&args, &argc, SYMBOL))
        return;

    /* breadth order? */
    if (argc == 1 && SYM1 == breadth_id)
        ancestors = object_ancestors_breadth(cur_frame->object->objnum);
    else
        ancestors = object_ancestors_depth(cur_frame->object->objnum);
    if (argc)
        pop(1);
    push_list(ancestors);
    list_discard(ancestors);
}

COLDC_FUNC(has_ancestor) {
    cData * args;
    Int result;

    /* Accept a objnum to check as an ancestor. */
    if (!func_init_1(&args, OBJNUM))
        return;

    result = object_has_ancestor(cur_frame->object->objnum, args[0].u.objnum);
    pop(1);
    push_int((cNum) result);
}

COLDC_FUNC(create) {
    cData *args, *d;
    cList *parents;
    Obj *obj;

    /* Accept a list of parents. */
    if (!func_init_1(&args, LIST))
        return;

    /* we need some parents */
    parents = args[0].u.list;
    if (list_length(parents) <= 0) {
        cthrow(type_id, "No parents specified.");
        return;
    }

    /* Verify that all parents are objnums. */
    for (d = list_first(parents); d; d = list_next(parents, d)) {
        if (d->type != OBJNUM) {
            cthrow(type_id, "Parent %D is not a objnum.", d);
            return;
        } else if (!cache_check(d->u.objnum)) {
            cthrow(objnf_id, "Parent %D does not refer to an object.", d);
            return;
        }
    }

    /* Create the new object. */
    obj = object_new(-1, parents);

    pop(1);
    push_objnum(obj->objnum);
    cache_discard(obj);
}

COLDC_FUNC(chparents) {
    cData   * args,
             * d,
               d2;
    Int        wrong;

    /* Accept a list of parents to change to. */
    if (!func_init_1(&args, LIST))
        return;

    if (cur_frame->object->objnum == ROOT_OBJNUM) {
        cthrow(perm_id, "You cannot change the root object's parents.");
        return;
    }

    if (!list_length(args[0].u.list)) {
        cthrow(perm_id, "You must specify at least one parent.");
        return;
    }

    /* Call object_change_parents().  This will return the number of a
     * parent which was invalid, or -1 if they were all okay. */
    wrong = object_change_parents(cur_frame->object, args[0].u.list);
    if (wrong >= 0) {
        d = list_elem(args[0].u.list, wrong);
        if (d->type != OBJNUM) {
            cthrow(type_id,   "New parent %D is not a objnum.", d);
        } else if (d->u.objnum == cur_frame->object->objnum) {
            cthrow(parent_id, "New parent %D is already a parent.", d);
        } else if (!cache_check(d->u.objnum)) {
            cthrow(objnf_id,  "New parent %D does not exist.", d);
        } else {
            d2.type = OBJNUM;
            d2.u.objnum = cur_frame->object->objnum;
            cthrow(parent_id, "New parent %D is a descendent of %D.", d, &d2);
        }
    } else {
        pop(1);
        push_int(1);
    }
}

COLDC_FUNC(destroy) {
    Obj * obj;

    if (!func_init_0())
        return;

    obj = cur_frame->object;
    if (obj->objnum == ROOT_OBJNUM)
        THROW((perm_id, "You can't destroy the root object."))
    else if (obj->objnum == SYSTEM_OBJNUM)
        THROW((perm_id, "You can't destroy the system object."))
     
    /*
    // Set the object dead, so it will go away when nothing is
    // holding onto it.  cache_discard() will notice the dead
    // flag, and call object_destroy().
    */
    obj->dead = 1;
    push_int(1);
}

COLDC_FUNC(data) {
    cData   * args,
               key,
               value;
    cDict   * dict;
    Obj * obj = cur_frame->object;
    Int        i,
               nargs;
    cObjnum   objnum;

    if (!func_init_0_or_1(&args, &nargs, OBJNUM))
        return;

    dict = dict_new_empty();
    if (nargs) {
        objnum = args[0].u.objnum;

        for (i = 0; i < obj->vars.size; i++) {
            if (obj->vars.tab[i].name == INV_OBJNUM ||
                obj->vars.tab[i].cclass != objnum)
                continue;
            key.type = SYMBOL;
            key.u.symbol = obj->vars.tab[i].name;
            dict = dict_add(dict, &key, &obj->vars.tab[i].val);
        }

        pop(1);
    } else {
        for (i = 0; i < obj->vars.size; i++) {
            if (obj->vars.tab[i].name == INV_OBJNUM)
                continue;
            key.type = OBJNUM;
            key.u.objnum = obj->vars.tab[i].cclass;
            if (dict_find(dict, &key, &value) == keynf_id) {
                value.type = DICT;
                value.u.dict = dict_new_empty();
                dict = dict_add(dict, &key, &value);
            }
            key.type = SYMBOL;
            key.u.symbol = obj->vars.tab[i].name;
            value.u.dict = dict_add(value.u.dict, &key, &obj->vars.tab[i].val);
            key.type = OBJNUM;
            key.u.objnum = obj->vars.tab[i].cclass;
            dict = dict_add(dict, &key, &value);
            dict_discard(value.u.dict);
        }
    }

    push_dict(dict);
    dict_discard(dict);
}

/*
// -----------------------------------------------------------------
*/

COLDC_FUNC(set_objname) {
    cData *args;

    if (!func_init_1(&args, SYMBOL))
        return;

    if (!object_set_objname(cur_frame->object, args[0].u.symbol)) {
        cthrow(error_id, "The name $%I is already taken.", args[0].u.symbol);
        return;
    }

    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/

COLDC_FUNC(del_objname) {
    if (!func_init_0())
        return;

    if (!object_del_objname(cur_frame->object)) {
        cthrow(namenf_id, "Object #%l does not have a name.",
               cur_frame->object->objnum);
        return;
    }

    push_int(1);
}

/*
// -----------------------------------------------------------------
*/

COLDC_FUNC(objname) {
    if (!func_init_0())
        return;
  
    if (cur_frame->object->objname == -1)
        cthrow(namenf_id,
               "No name is assigned to #%l.",
               cur_frame->object->objnum);
    else
        push_symbol(cur_frame->object->objname);
}

/*
// -----------------------------------------------------------------
*/

COLDC_FUNC(lookup) {
    cData *args;
    Long objnum;

    if (!func_init_1(&args, SYMBOL))
        return;

    if (!lookup_retrieve_name(args[0].u.symbol, &objnum)) {
        cthrow(namenf_id, "Cannot find object $%I.", args[0].u.symbol);
        return;
    }

    pop(1);
    push_objnum(objnum);
}

/*
// -----------------------------------------------------------------
*/

COLDC_FUNC(objnum) {
    if (!func_init_0())
        return;
  
    push_int(cur_frame->object->objnum);
}

INTERNAL cList * add_op_arg(cList * out, Int type, Long op, Method * method) {
    Obj * obj = method->object;
    cData d;

    switch (type) {
        case INTEGER:
            d.type = INTEGER;
            d.u.val = op;
            break;
        case FLOAT:
            d.type = FLOAT;
            d.u.fval = *((Float*)(&op));
            break;
        case T_ERROR:
            if (op == -1) {
                d.type = INTEGER;
                d.u.val = -1;
            } else {
                d.type = T_ERROR;
                d.u.error = object_get_ident(obj, op);
            }
            break;
        case IDENT:
            d.type = SYMBOL;
            d.u.symbol = object_get_ident(obj, op);
            break;
        case VAR: {
            Long id;
    
            d.type = SYMBOL;
    
            if (op < method->num_args) {
                op = method->num_args - op - 1;
                id = object_get_ident(obj, method->argnames[op]);
                d.u.symbol = id;
                break;
            }
            op -= method->num_args;
    
            if (method->rest != -1) {
                if (op == 0) {
                   id = object_get_ident(obj, method->rest);
                   d.u.symbol = id;
                   break;
                }
                op--;
            }

            id = object_get_ident(obj, method->varnames[op]);
            d.u.symbol = id;
            break;
            }
        case STRING:
            d.type = STRING;
            d.u.str = object_get_string(obj, op);
            break;
        /* case JUMP: */ /* ignore JUMP */
        default:
            return out;
#if DISABLED   /* none of these are used as args in op_table */
        case LIST:
        case FROB:
        case DICT:
        case BUFFER:
#endif
    }

    out = list_add(out, &d);
    /* do not discard, we were not using duped data */

    return out;
}

COLDC_FUNC(method_bytecode) {
    cData       * args, d;
    Method     * method;
    cList       * list;
    register Int   x;
    Long         * ops;
    Op_info      * info;
    Long opcode;

    /* Accept a list of lines of code and a symbol for the name. */
    if (!func_init_1(&args, SYMBOL))
        return;

    method = object_find_method(cur_frame->object->objnum, args[0].u.symbol, FROB_ANY);

    /* keep these for later reference, if its already around */
    if (!method)
        THROW((methodnf_id, "Method %D not found.", &args[0]))

    list = list_new(method->num_opcodes);
    d.type = SYMBOL;
    ops = method->opcodes;
    x=0;
    while (x < method->num_opcodes) {
        opcode = ops[x];
        info = &op_table[opcode];
        d.type = SYMBOL;
        d.u.symbol = info->symbol;
        list = list_add(list, &d);
        /* dont bother discarding, we didnt dup twice */
        x++;

        if (info->arg1) {
            list = add_op_arg(list, info->arg1, ops[x], method);
            x++;
        }

        if (info->arg2) {
            list = add_op_arg(list, info->arg1, ops[x], method);
            x++;
        }
    }

    pop(1);
    push_list(list);
    list_discard(list);
}
