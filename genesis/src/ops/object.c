/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: functions.c
// ---
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

#include "config.h"
#include "defs.h"

#include "lookup.h"
#include "execute.h"
#include "object.h"
#include "grammar.h"
#include "cache.h"

void func_add_var(void) {
    data_t * args;
    long     result;

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

void func_del_var(void) {
    data_t * args;
    long     result;

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

void func_variables(void) {
    list_t   * vars;
    object_t * obj;
    int        i;
    Var      * var;
    data_t     d;

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

void func_set_var(void) {
    data_t * args,
             d;
    long     result;

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

void func_get_var(void) {
    data_t * args,
             d;
    long     result;

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

void func_clear_var(void) {
    data_t * args;
    long     result = 0;

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

void func_add_method(void) {
    data_t   * args,
             * d;
    method_t * method;
    list_t   * code,
             * errors;
    int        flags=-1, access=-1;

    /* Accept a list of lines of code and a symbol for the name. */
    if (!func_init_2(&args, LIST, SYMBOL))
	return;

    method = object_find_method(cur_frame->object->objnum, args[1].u.symbol);

    if (method && (method->m_flags & MF_LOCK))
        THROW((perm_id, "Method is locked, and cannot be changed."))
    if (method && (method->m_flags & MF_NATIVE))
        THROW((perm_id, "Method is native, and cannot be recompiled."))


    /* keep these for later reference, if its already around */
    if (method) {
        flags = method->m_flags;
        access = method->m_access;
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
	object_add_method(cur_frame->object, args[1].u.symbol, method);
	method_discard(method);
    }

    pop(2);
    push_list(errors);
    list_discard(errors);
}

void func_rename_method(void) {
    data_t   * args;
    method_t * method;

    if (!func_init_2(&args, SYMBOL, SYMBOL))
        return;

    method = object_find_method(cur_frame->object->objnum, args[1].u.symbol);
    if (method != NULL) {
        cthrow(method_id, "Method %I already exists!", args[1].u.symbol);
        return;
    }

    if (!object_rename_method(cur_frame->object,
                                  args[0].u.symbol,
                                  args[1].u.symbol)) {
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

INTERNAL list_t * list_method_flags(int flags) {
    list_t * list;
    data_t d;

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
    if (flags & MF_NATIVE)
        LADD(native_id);

    return list;
}

#undef LADD

void func_method_flags(void) {
    data_t  * args;
    list_t  * list;

    if (!func_init_1(&args, SYMBOL))
        return;

    list = list_method_flags(object_get_method_flags(cur_frame->object, args[0].u.symbol));

    pop(1);
    push_list(list);
    list_discard(list);
}

void func_set_method_flags(void) {
    data_t  * args,
            * d;
    list_t  * list;
    int       flags,
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
        else if (d->u.symbol == native_id)
            THROW((perm_id, "Native flag can only be set by the driver."))
        else
            THROW((perm_id, "Unknown method flag (%D).", d))
    }

    object_set_method_flags(cur_frame->object, args[0].u.symbol, new_flags);

    pop(2);
    push_int(new_flags);
}

void func_method_access(void) {
    int       access;
    data_t  * args;

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
        default:           push_int(0);               break;
    }
}

void func_set_method_access(void) {
    int       access = 0;
    data_t  * args;
    Ident     sym;

    if (!func_init_2(&args, SYMBOL, SYMBOL))
        return;

    sym = args[1].u.symbol;
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
    else
        cthrow(type_id, "Invalid method access flag.");

    object_set_method_access(cur_frame->object, args[0].u.symbol, access);

    if (access == -1)
        cthrow(type_id, "Method %D not found.", args[0]);

    pop(2);
    push_int(access);
}

void func_method_info(void) {
    data_t   * args,
             * list;
    list_t   * output;
    method_t * method;
    string_t * str;
    char     * s;
    int        i;

    /* A symbol for the Method name. */
    if (!func_init_1(&args, SYMBOL))
	return;

    method = object_find_method(cur_frame->object->objnum, args[0].u.symbol);
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
            str = string_addc(str, '[');
            s = ident_name(object_get_ident(method->object, method->rest));
            str = string_add_chars(str, s, strlen(s));
            str = string_addc(str, ']');
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
        default:           list[4].type = INTEGER; list[4].u.val = 0;  break;
    }

    list[5].type = LIST;
    list[5].u.list = list_method_flags(method->m_flags);

    pop(1);
    push_list(output);
    list_discard(output);
}

void func_methods(void) {
    list_t   * methods;
    data_t     d;
    object_t * obj;
    int        i;

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

void func_find_method(void) {
    data_t   * args;
    method_t * method;

    /* Accept a symbol argument giving the method name. */
    if (!func_init_1(&args, SYMBOL))
	return;

    /* Look for the method on the current object. */
    method = object_find_method(cur_frame->object->objnum, args[0].u.symbol);
    pop(1);
    if (method) {
	push_objnum(method->object->objnum);
	cache_discard(method->object);
    } else {
	cthrow(methodnf_id, "Method %I not found.", args[0].u.symbol);
    }
}

void func_find_next_method(void) {
    data_t   * args;
    method_t * method;

    /* Accept a symbol argument giving the method name, and a objnum giving the
     * object to search past. */
    if (!func_init_2(&args, SYMBOL, OBJNUM))
	return;

    /* Look for the method on the current object. */
    method = object_find_next_method(cur_frame->object->objnum,
				     args[0].u.symbol, args[1].u.objnum);
    if (method) {
	push_objnum(method->object->objnum);
	cache_discard(method->object);
    } else {
	cthrow(methodnf_id, "Method %I not found.", args[0].u.symbol);
    }
}

void func_decompile(void) {
    int      num_args,
             indent,
             parens;
    data_t * args;
    list_t * code;

    /* Accept a symbol for the method name, an optional integer for the
     * indentation, and an optional integer to specify full
     * parenthesization. */
    if (!func_init_1_to_3(&args, &num_args, SYMBOL, INTEGER, INTEGER))
	return;

    indent = (num_args >= 2) ? args[1].u.val : DEFAULT_INDENT;
    indent = (indent < 0) ? 0 : indent;
    parens = (num_args == 3) ? (args[2].u.val != 0) : 0;
    code = object_list_method(cur_frame->object, args[0].u.symbol, indent,
			      parens);

    if (code) {
	pop(num_args);
	push_list(code);
	list_discard(code);
    } else {
	cthrow(methodnf_id, "Method %I not found.", args[0].u.symbol);
    }
}

void func_del_method(void) {
    data_t * args;
    int      status;

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

void func_parents(void) {
    /* Accept no arguments. */
    if (!func_init_0())
	return;

    /* Push the parents list onto the stack. */
    push_list(cur_frame->object->parents);
}

void func_children(void) {
    /* Accept no arguments. */
    if (!func_init_0())
	return;

    /* Push the children list onto the stack. */
    push_list(cur_frame->object->children);
}

void func_descendants(void) {
    list_t * desc;

    if (!func_init_0())
        return;

    desc = object_descendants(cur_frame->object->objnum);

    push_list(desc);
    list_discard(desc);
}

void func_ancestors(void) {
    list_t * ancestors;

    /* Accept no arguments. */
    if (!func_init_0())
	return;

    /* Get an ancestors list from the object. */
    ancestors = object_ancestors(cur_frame->object->objnum);
    push_list(ancestors);
    list_discard(ancestors);
}

void func_has_ancestor(void) {
    data_t * args;
    int result;

    /* Accept a objnum to check as an ancestor. */
    if (!func_init_1(&args, OBJNUM))
	return;

    result = object_has_ancestor(cur_frame->object->objnum, args[0].u.objnum);
    pop(1);
    push_int(result);
}

void func_create(void) {
    data_t *args, *d;
    list_t *parents;
    object_t *obj;

    /* Accept a list of parents. */
    if (!func_init_1(&args, LIST))
        return;

    /* Get parents list from second argument. */
    parents = args[0].u.list;

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

void func_chparents(void) {
    data_t   * args,
             * d,
               d2;
    int        wrong;

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

void func_destroy(void) {
    data_t   * args;
    object_t * obj;

    /* Accept a objnum to destroy. */
    if (!func_init_1(&args, OBJNUM))
        return;

    if (args[0].u.objnum == ROOT_OBJNUM) {
        cthrow(perm_id, "You can't destroy the root object.");
    } else if (args[0].u.objnum == SYSTEM_OBJNUM) {
        cthrow(perm_id, "You can't destroy the system object.");
    } else {
        obj = cache_retrieve(args[0].u.objnum);
        if (!obj) {
            cthrow(objnf_id, "Object #%l not found.", args[0].u.objnum);
            return;
        }
        /* Set the object dead, so it will go away when nothing is holding onto
         * it.  cache_discard() will notice the dead flag, and call
         * object_destroy(). */
        obj->dead = 1;
        cache_discard(obj);
        pop(1);
        push_int(1);
    }
}

void func_data(void) {
    data_t   * args,
               key,
               value;
    Dict     * dict;
    object_t * obj = cur_frame->object;
    int        i,
               nargs;
    objnum_t   objnum;

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

void func_set_objname(void) {
    data_t *args;

    if (!func_init_1(&args, SYMBOL))
        return;

    if (!object_set_objname(cur_frame->object, args[0].u.symbol)) {
        cthrow(error_id, "The name '%I is already taken.", args[0].u.symbol);
        return;
    }

    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/

void func_del_objname(void) {
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

void func_objname(void) {
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

void func_lookup(void) {
    data_t *args;
    long objnum;

    if (!func_init_1(&args, SYMBOL))
        return;

    if (!lookup_retrieve_name(args[0].u.symbol, &objnum)) {
        cthrow(namenf_id, "Cannot find object %I.", args[0].u.symbol);
        return;
    }

    pop(1);
    push_objnum(objnum);
}

/*
// -----------------------------------------------------------------
*/

void func_objnum(void) {
    if (!func_init_0())
        return;
  
    push_int(cur_frame->object->objnum);
}

void func_compile(void) {
    if (!func_init_0())
        return;
    push_int(1);
}

void func_get_method(void) {
    data_t       * args, d;
    method_t     * method;
    list_t       * list;
    register int   x;
    long         * ops;

    /* Accept a list of lines of code and a symbol for the name. */
    if (!func_init_1(&args, SYMBOL))
        return;

    method = object_find_method(cur_frame->object->objnum, args[0].u.symbol);

    /* keep these for later reference, if its already around */
    if (!method)
        THROW((methodnf_id, "Method %D not found.", &args[0]))

    list = list_new(method->num_opcodes);
    d.type = SYMBOL;
    ops = method->opcodes;
    for (x=0; x < method->num_opcodes; x++) {
        switch (ops[x]) {
            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '!':
            case '>':
            case '<':
                d.type = SYMBOL;
                d.u.symbol = ident_dup(op_table[ops[x]].symbol);
                break;
            default:
                if (ops[x] > FIRST_TOKEN) {
                    d.type = SYMBOL;
                    d.u.symbol = ident_dup(op_table[ops[x]].symbol);
                } else {
                    d.type = INTEGER;
                    d.u.val = ops[x];
                }
        }
        list = list_add(list, &d);
    }

    pop(1);
    push_list(list);
    list_discard(list);
}

