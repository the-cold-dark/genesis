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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>  /* func_files() */
#include <fcntl.h>
#include "y.tab.h"
#include "lookup.h"
#include "functions.h"
#include "execute.h"
#include "data.h"
#include "ident.h"
#include "object.h"
#include "grammar.h"
#include "cache.h"
#include "dbpack.h"
#include "memory.h"
#include "opcodes.h"
#include "log.h"       /* op_log() */
#include "net.h"       /* network functions */
#include "util.h"      /* some file functions */
#include "file.h"

void func_null_function(void) {
    if (!func_init_0())
        return;
    push_int(1);
}

void func_add_var(void) {
    data_t * args;
    long     result;

    /* Accept a symbol argument a data value to assign to the variable. */
    if (!func_init_1(&args, SYMBOL))
	return;

    result = object_add_var(cur_frame->object, args[0].u.symbol);
    if (result == varexists_id) {
	cthrow(varexists_id,
	      "Object variable %I already exists.", args[0].u.symbol);
    } else {
	pop(1);
	push_int(1);
    }
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
    if (method && (method->m_flags & MF_LOCK)) {
        cthrow(perm_id, "Method is locked, and cannot be changed.");
        return;
    }

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
    int        result;

    if (!func_init_2(&args, SYMBOL, SYMBOL))
        return;

    result = object_rename_method(cur_frame->object,
                                  args[0].u.symbol,
                                  args[1].u.symbol);

    pop(2);
    push_int(result);
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

#define _THROW_(__e, __m) { cthrow(__e, __m); return; }

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
        _THROW_(methodnf_id, "Method not found.");
    if (flags & MF_LOCK)
        _THROW_(perm_id, "Method is locked and cannot be changed.");
    if (flags & MF_NATIVE)
        _THROW_(perm_id,"Method is native and cannot be changed.");

    list = args[1].u.list;
    for (d = list_first(list); d; d = list_next(list, d)) {
	if (d->type != SYMBOL) {
	    cthrow(type_id, "Invalid method flag (%D).", d);
	    return;
	}
        if (d->u.symbol == noover_id) {
            new_flags |= MF_NOOVER;
        } else if (d->u.symbol == sync_id) {
            new_flags |= MF_SYNC;
        } else if (d->u.symbol == locked_id) {
            new_flags |= MF_LOCK;
        } else if (d->u.symbol == native_id) {
            _THROW_(perm_id, "Native flag can only be set by the driver.");
        } else {
            cthrow(perm_id, "Unknown method flag (%D).", d);
            return;
        }
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
        case MS_PUBLIC:    list[4].u.symbol = public_id;    break;
        case MS_PROTECTED: list[4].u.symbol = protected_id; break;
        case MS_PRIVATE:   list[4].u.symbol = private_id;   break;
        case MS_ROOT:      list[4].u.symbol = root_id;      break;
        case MS_DRIVER:    list[4].u.symbol = driver_id;    break;
        default:           list[4].type = INTEGER; list[4].u.val = 0; break;
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

void func_size(void) {
    data_t * args;
    int      nargs,
             size;

    if (!func_init_0_or_1(&args, &nargs, 0))
	return;

    if (nargs) {
        if (args[0].type == OBJNUM) {
            cthrow(perm_id, "Attempt to size object which is not this().");
            return;
        }
        size = size_data(&args[0]);
        pop(1);
    } else {
        size = size_object(cur_frame->object);
    }

    /* Push size of current object. */
    push_int(size);
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

/* MODULE_NOTE: remove this once the file module is fully functional */

void func_log(void) {
    data_t * args;

    /* Accept a string. */
    if (!func_init_1(&args, STRING))
        return;

    write_log("%S", args[0].u.str);
    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
//
// Modifies: The object cache, identifier table, and binary database
//           files via cache_sync() and ident_dump().
// Effects: If called by the sytem object with no arguments,
//          performs a binary dump, ensuring that the files db and
//          db are consistent.  Returns 1 if the binary dump
//          succeeds, or 0 if it fails.
//
*/

#define SHELL_FAILURE 127

void func_backup(void) {
    char          buf1[255];
    char          buf2[255];
    struct stat   statbuf;

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    strcpy(buf1, c_dir_binary);
    strcat(buf1, ".bak");

    /* blast old backups */
    if (stat(buf1, &statbuf) != F_FAILURE) {
        sprintf(buf2, "rm -rf %s", buf1);
        if (system(buf2) == SHELL_FAILURE)
            push_int(0);
    }
    cache_sync();
    sprintf(buf2, "cp -r %s %s", c_dir_binary, buf1);
    if (system(buf2) != SHELL_FAILURE) {
        push_int(1);
    } else {
        push_int(-1);
    }
}

#undef SHELL_FAILURE

/*
// -----------------------------------------------------------------
//
// Modifies: The 'running' global (defs.h) may be set to 0.
//
// Effects: If called by the system object with no arguments, sets
//          'running' to 0, causing the program to exit after this
//          iteration of the main loop finishes.  Returns 1.
//
*/

void func_shutdown(void) {

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    running = 0;
    push_int(1);
}

/*
// -----------------------------------------------------------------
//
// run an executable from the filesystem
//
// MODULE_NOTE: Move this to the file module
*/

void func_execute(void) {
    data_t *args, *d;
    list_t *script_args;
    int num_args, argc, len, i, fd, status;
    pid_t pid;
    char *fname, **argv;

    /* Accept a name of a script to run, a list of arguments to give it, and
     * an optional flag signifying that we should not wait for completion. */
    if (!func_init_2_or_3(&args, &num_args, STRING, LIST, INTEGER))
        return;

    script_args = args[1].u.list;

    /* Verify that all items in argument list are strings. */
    for (d = list_first(script_args), i=0;
         d;
         d = list_next(script_args, d), i++) {
        if (d->type != STRING) {
            cthrow(type_id,
                   "Execute argument %d (%D) is not a string.",
                   i+1, d);
            return;
        }
    }

    /* Don't allow walking back up the directory tree. */
    if (strstr(string_chars(args[0].u.str), "../")) {
        cthrow(perm_id, "Filename %D is not legal.", &args[0]);
        return;
    }

    /* Construct the name of the script. */
    len = string_length(args[0].u.str);
    fname = TMALLOC(char, len + 9);
    memcpy(fname, "scripts/", 8);
    memcpy(fname + 8, string_chars(args[0].u.str), len);
    fname[len + 8] = 0;

    /* Build an argument list. */
    argc = list_length(script_args) + 1;
    argv = TMALLOC(char *, argc + 1);
    argv[0] = tstrdup(fname);
    for (d = list_first(script_args), i = 0;
         d;
         d = list_next(script_args, d), i++)
        argv[i + 1] = tstrdup(string_chars(d->u.str));
    argv[argc] = NULL;

    pop(num_args);

    /* Fork off a process. */
#ifdef USE_VFORK
    pid = vfork();
#else
    pid = fork();
#endif
    if (pid == 0) {
        /* Pipe stdin and stdout to /dev/null, keep stderr. */
        fd = open("/dev/null", O_RDWR);
        if (fd == -1) {
            write_err("EXEC: Failed to open /dev/null: %s.", strerror(errno));
            _exit(-1);
        }
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        execv(fname, argv);
        write_err("EXEC: Failed to exec \"%s\": %s.", fname, strerror(errno));
        _exit(-1);
    } else if (pid > 0) {
        if (num_args == 3 && args[2].u.val) {
            if (waitpid(pid, &status, WNOHANG) == 0)
                status = 0;
        } else {
            waitpid(pid, &status, 0);
        }
    } else {
        write_err("EXEC: Failed to fork: %s.", strerror(errno));
        status = -1;
    }

    /* Free the argument list. */
    for (i = 0; i < argc; i++)
        tfree_chars(argv[i]);
    TFREE(argv, argc + 1);

    push_int(status);
}

/*
// -----------------------------------------------------------------
//
// Modifies: heartbeat_freq (defs.h)
//
*/

void func_set_heartbeat(void) {
    data_t *args;

    if (!func_init_1(&args, INTEGER))
        return;

    if (args[0].u.val <= 0)
        args[0].u.val = -1;
    heartbeat_freq = args[0].u.val;
    pop(1);
}

/*
// -----------------------------------------------------------------
//
// Return all vars on an object.
//
// Completely destroys encapsulation, but it is needed for a run-time
// environment.
//
// Modify later to specify which defining ancestor
*/

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
        cthrow(error_id, "The name $%I is already taken.", args[0].u.symbol);
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
        push_int(0);
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

/* ----------------------------------------------------------------- */
/* cancel a suspended task                                           */
void func_cancel(void) {
    data_t *args;
 
    if (!func_init_1(&args, INTEGER))
        return;


    if (!task_lookup(args[0].u.val)) {
        cthrow(type_id, "No task %d.", args[0].u.val);
    } else {
        task_cancel(args[0].u.val);
        pop(1);
        push_int(1);
    }
}

/* ----------------------------------------------------------------- */
/* suspend a task                                                    */
void func_suspend(void) {

    if (!func_init_0())
        return;

    if (atomic) {
        cthrow(atomic_id, "Attempt to suspend while executing atomically.");
        return;
    }

    task_suspend();

    /* we'll let task_resume push something onto the stack for us */
}

/* ----------------------------------------------------------------- */
void func_resume(void) {
    data_t *args;
    int nargs;
    long tid;

    if (!func_init_1_or_2(&args, &nargs, INTEGER, 0))
        return;

    tid = args[0].u.val;

    if (!task_lookup(tid)) {
        cthrow(type_id, "No task %d.", args[0].u.val);
    } else {
        if (nargs == 1)
            task_resume(tid, NULL);
        else
            task_resume(tid, &args[1]);
        pop(nargs);
        push_int(0);
    }
}

/* ----------------------------------------------------------------- */
void func_pause(void) {
    if (!func_init_0())
        return;

    if (atomic) {
        cthrow(atomic_id, "Attempt to pause while executing atomically.");
        return;
    }

    push_int(1);

    task_pause();
}

/* ----------------------------------------------------------------- */
void func_atomic(void) {
    data_t * args;

    if (!func_init_1(&args, INTEGER))
        return;

    atomic = (int) (args[0].u.val ? 1 : 0);

    pop(1);
    push_int(1);
}

/* ----------------------------------------------------------------- */
void func_refresh(void) {

    if (!func_init_0())
        return;

    push_int(1);

    if (cur_frame->ticks <= REFRESH_METHOD_THRESHOLD) {
        if (atomic) {
            cur_frame->ticks = PAUSED_METHOD_TICKS;
        } else {
            task_pause();
        }
    }
}

/* ----------------------------------------------------------------- */
void func_tasks(void) {
    list_t * list;

    if (!func_init_0())
        return;

    list = task_list();

    push_list(list);
    list_discard(list);
}

/* ----------------------------------------------------------------- */
void func_tick(void) {
    if (!func_init_0())
        return;
    push_int(tick);
}

/* ----------------------------------------------------------------- */
void func_stack(void) {
    list_t * list;

    if (!func_init_0())
        return;

    list = task_stack();

    push_list(list);
    list_discard(list);
}

void func_bind_function(void) {
    data_t * args;
    int      opcode;

    /* accept a symbol and objnum */
    if (!func_init_2(&args, SYMBOL, OBJNUM))
        return;

    opcode = find_function(ident_name(args[0].u.symbol));

    if (opcode == -1) {
        cthrow(perm_id, "Attempt to bind function which does not exist.");
        return;
    }

    op_table[opcode].binding = args[1].u.objnum;

    pop(2);
    push_int(1);
}

void func_unbind_function(void) {
    data_t *args;
    int   opcode;

    /* accept a symbol */
    if (!func_init_1(&args, SYMBOL))
        return;

    opcode = find_function(ident_name(args[0].u.symbol));

    if (opcode == -1) {
        cthrow(perm_id, "Attempt to unbind function which does not exist.");
        return;
    }

    op_table[opcode].binding = INV_OBJNUM;

    pop(1);
    push_int(1);
}

void func_method(void) {
    if (!func_init_0())
        return;

    push_symbol(cur_frame->method->name);
}

void func_this(void) {
    /* Accept no arguments, and push the objnum of the current object. */
    if (!func_init_0())
        return;
    push_objnum(cur_frame->object->objnum);
}

void func_definer(void) {
    /* Accept no arguments, and push the objnum of the method definer. */
    if (!func_init_0())
        return;
    push_objnum(cur_frame->method->object->objnum);
}

void func_sender(void) {
    /* Accept no arguments, and push the objnum of the sending object. */
    if (!func_init_0())
        return;
    if (cur_frame->sender == NOT_AN_IDENT)
        push_int(0);
    else
        push_objnum(cur_frame->sender);
}

void func_caller(void) {
    /* Accept no arguments, and push the objnum of the calling method's
     * definer. */
    if (!func_init_0())
        return;
    if (cur_frame->caller == NOT_AN_IDENT)
        push_int(0);
    else
        push_objnum(cur_frame->caller);
}

void func_task_id(void) {
    /* Accept no arguments, and push the task ID. */
    if (!func_init_0())
        return;
    push_int(task_id);
}

void func_ticks_left(void) {
    if (!func_init_0())
      return;

    push_int(cur_frame->ticks);
}

void func_type(void) {
    data_t *args;
    int type;

    /* Accept one argument of any type. */
    if (!func_init_1(&args, 0))
	return;

    /* Replace argument with symbol for type name. */
    type = args[0].type;
    pop(1);
    push_symbol(data_type_id(type));
}

void func_class(void) {
    data_t *args;
    long cclass;

    /* Accept one argument of frob type. */
    if (!func_init_1(&args, FROB))
	return;

    /* Replace argument with class. */
    cclass = args[0].u.frob->cclass;
    pop(1);
    push_objnum(cclass);
}

void func_toint(void) {
    data_t *args;
    long val = 0;

    /* Accept a string or integer to convert into an integer. */
    if (!func_init_1(&args, 0))
	return;

    if (args[0].type == INTEGER) {
  	return;
    } else if (args[0].type == FLOAT) {
        val = (long) args[0].u.fval;
    } else if (args[0].type == STRING) {
	val = atol(string_chars(args[0].u.str));
    } else if (args[0].type == OBJNUM) {
	val = args[0].u.objnum;
    } else {
	cthrow(type_id, "The first argument (%D) is not an integer or string.",
	      &args[0]);
    }
    pop(1);
    push_int(val);
}

void func_tofloat(void) {
      data_t * args;
      float val = 0;
  
      /* Accept a string, integer or integer to convert into a float. */
      if (!func_init_1(&args, 0))
  	return;
  
      if (args[0].type == STRING) {
  	val = atof(string_chars(args[0].u.str));
      } else if (args[0].type == INTEGER) {
  	val = args[0].u.val;
      } else if (args[0].type == FLOAT) {
  	return;
      } else if (args[0].type == OBJNUM) {
  	val = args[0].u.objnum;
      } else {
  	cthrow(type_id, "The first argument (%D) is not an integer or string.",
  	      &args[0]);
      }
      pop(1);
      push_float(val);
}

void func_tostr(void) {
    data_t *args;
    string_t *str;

    /* Accept one argument of any type. */
    if (!func_init_1(&args, 0))
	return;

    /* Replace the argument with its text version. */
    str = data_tostr(&args[0]);
    pop(1);
    push_string(str);
    string_discard(str);
}

void func_toliteral(void) {
    data_t *args;
    string_t *str;

    /* Accept one argument of any type. */
    if (!func_init_1(&args, 0))
	return;

    /* Replace the argument with its unparsed version. */
    str = data_to_literal(&args[0]);
    pop(1);
    push_string(str);
    string_discard(str);
}

void func_toobjnum(void) {
    data_t *args;

    /* Accept an integer to convert into a objnum. */
    if (!func_init_1(&args, INTEGER))
	return;

    if (args[0].u.val < 0)
        cthrow(type_id, "Objnums must be 0 or greater");

    args[0].u.objnum = args[0].u.val;
    args[0].type = OBJNUM;
}

void func_tosym(void) {
    data_t *args;
    long sym;

    /* Accept one string argument. */
    if (!func_init_1(&args, STRING))
	return;

    sym = ident_get(string_chars(args[0].u.str));
    pop(1);
    push_symbol(sym);
}

void func_toerr(void) {
    data_t *args;
    long error;

    /* Accept one string argument. */
    if (!func_init_1(&args, STRING))
	return;

    error = ident_get(string_chars(args[0].u.str));
    pop(1);
    push_error(error);
}

void func_valid(void) {
    data_t *args;
    int is_valid;

    /* Accept one argument of any type (only objnums can be valid, though). */
    if (!func_init_1(&args, 0))
	return;

    is_valid = (args[0].type == OBJNUM && cache_check(args[0].u.objnum));
    pop(1);
    push_int(is_valid);
}

void func_error(void) {
    if (!func_init_0())
	return;

    if (!cur_frame->handler_info) {
	cthrow(error_id, "Request for handler info outside handler.");
	return;
    }

    push_error(cur_frame->handler_info->error);
}

void func_traceback(void) {
    if (!func_init_0())
	return;

    if (!cur_frame->handler_info) {
	cthrow(error_id, "Request for handler info outside handler.");
	return;
    }

    push_list(cur_frame->handler_info->traceback);
}

void func_throw(void) {
    data_t *args, error_arg;
    int num_args;
    string_t *str;

    if (!func_init_2_or_3(&args, &num_args, ERROR, STRING, 0))
	return;

    /* Throw the error. */
    str = string_dup(args[1].u.str);
    if (num_args == 3) {
	data_dup(&error_arg, &args[2]);
	user_error(args[0].u.error, str, &error_arg);
	data_discard(&error_arg);
    } else {
	user_error(args[0].u.error, str, NULL);
    }
    string_discard(str);
}

void func_rethrow(void) {
    data_t *args;
    list_t *traceback;

    if (!func_init_1(&args, ERROR))
	return;

    if (!cur_frame->handler_info) {
	cthrow(error_id, "Request for handler info outside handler.");
	return;
    }

    /* Abort the current frame and propagate an error in the caller. */
    traceback = list_dup(cur_frame->handler_info->traceback);
    frame_return();
    propagate_error(traceback, args[0].u.error);
}

/*
// ------------------------------------------------------------------------
//
// Network Functions
//
// ------------------------------------------------------------------------
*/

/*
// -----------------------------------------------------------------
//
// If the current object has a connection, it will reassign that
// connection too the specified object.
//
*/

void func_reassign_connection(void) {
    data_t       * args;
    connection_t * c;

    /* Accept a objnum. */
    if (!func_init_1(&args, OBJNUM))
        return;

    c = find_connection(cur_frame->object);
    if (c) {
        c->objnum = args[0].u.objnum;
        pop(1);
        push_int(1);
    } else {
        pop(1);
        push_int(0);
    }
}

/*
// -----------------------------------------------------------------
*/
void func_bind_port(void) {
    data_t * args;

    /* Accept a port to bind to, and a objnum to handle connections. */
    if (!func_init_2(&args, INTEGER, OBJNUM))
        return;

    if (add_server(args[0].u.val, args[1].u.objnum))
        push_int(1);
    else if (server_failure_reason == socket_id)
        cthrow(socket_id, "Couldn't create server socket.");
    else /* (server_failure_reason == bind_id) */
        cthrow(bind_id, "Couldn't bind to port %d.", args[0].u.val);
}

/*
// -----------------------------------------------------------------
*/
void func_unbind_port(void) {
    data_t * args;

    /* Accept a port number. */
    if (!func_init_1(&args, INTEGER))
        return;

    if (!remove_server(args[0].u.val))
        cthrow(servnf_id, "No server socket on port %d.", args[0].u.val);
    else
        push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_open_connection(void) {
    data_t *args;
    char *address;
    int port;
    objnum_t receiver;
    long r;

    if (!func_init_3(&args, STRING, INTEGER, OBJNUM))
        return;

    address = string_chars(args[0].u.str);
    port = args[1].u.val;
    receiver = args[2].u.objnum;

    r = make_connection(address, port, receiver);
    if (r == address_id)
        cthrow(address_id, "Invalid address");
    else if (r == socket_id)
        cthrow(socket_id, "Couldn't create socket for connection");
    pop(3);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_close_connection(void) {
    /* Accept no arguments. */
    if (!func_init_0())
        return;

    /* Kick off anyone assigned to the current object. */
    push_int(boot(cur_frame->object));
}

/*
// -----------------------------------------------------------------
// Echo a buffer to the connection
*/
void func_cwrite(void) {
    data_t *args;

    /* Accept a buffer to write. */
    if (!func_init_1(&args, BUFFER))
        return;

    /* Write the string to any connection associated with this object.  */
    tell(cur_frame->object, args[0].u.buffer);

    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
// write a file to the connection
*/
void func_cwritef(void) {
    size_t        block, r;
    data_t      * args;
    FILE        * fp;
    Buffer      * buf;
    string_t    * str;
    struct stat   statbuf;
    int           nargs;

    /* Accept the name of a file to echo */
    if (!func_init_1_or_2(&args, &nargs, STRING, INTEGER))
        return;

    /* Initialize the file */
    str = build_path(args[0].u.str->s, &statbuf, DISALLOW_DIR);
    if (str == NULL)
        return;

    /* Open the file for reading. */
    fp = open_scratch_file(str->s, "rb");
    if (!fp) {
        cthrow(file_id, "Cannot open file \"%s\" for reading.", str->s);
        return;
    }

    /* how big of a chunk do we read at a time? */
    if (nargs == 2) {
        if (args[1].u.val == -1)
            block = statbuf.st_size;
        else
            block = (size_t) args[1].u.val;
    } else
        block = (size_t) DEF_BLOCKSIZE;

    /* Allocate a buffer to hold the block */
    buf = buffer_new(block);

    while (!feof(fp)) {
        r = fread(buf->s, sizeof(unsigned char), block, fp);
        if (r != block && !feof(fp)) {
            buffer_discard(buf);
            close_scratch_file(fp);
            cthrow(file_id, "Trouble reading file \"%s\": %s",
                   str->s, strerror(errno));
            return;
        }
        tell(cur_frame->object, buf);
    }

    /* Discard the buffer and close the file. */
    buffer_discard(buf);
    close_scratch_file(fp);

    pop(nargs);
    push_int(1);
}

/*
// -----------------------------------------------------------------
// return random info on the connection
*/
void func_connection(void) {
    list_t       * info;
    data_t       * list;
    connection_t * c;

    if (!func_init_0())
        return;

    c = find_connection(cur_frame->object);
    if (!c) {
        cthrow(net_id, "No connection established.");
        return;
    }

    info = list_new(4);
    list = list_empty_spaces(info, 4);

    list[0].type = INTEGER;
    list[0].u.val = (long) (c->flags.readable ? 1 : 0);
    list[1].type = INTEGER;
    list[1].u.val = (long) (c->flags.writable ? 1 : 0);
    list[2].type = INTEGER;
    list[2].u.val = (long) (c->flags.dead ? 1 : 0);
    list[3].type = INTEGER;
    list[3].u.val = (long) (c->fd);

    push_list(info);
    list_discard(info);
}

/*
// ------------------------------------------------------------------------
//
// File Functions
//
// ------------------------------------------------------------------------
*/

#define GET_FILE_CONTROLLER(__f) { \
        __f = find_file_controller(cur_frame->object); \
        if (__f == NULL) { \
            cthrow(file_id, "No file is bound to this object."); \
            return; \
        } \
    } \

/*
// -----------------------------------------------------------------
*/
void func_fopen(void) {
    data_t  * args;
    int       nargs;
    list_t  * stat; 
    filec_t * file;

    if (!func_init_1_or_2(&args, &nargs, STRING, STRING))
        return;

    file = find_file_controller(cur_frame->object);

    /* only one file at a time on an object */
    if (file != NULL) {
        cthrow(file_id, "A file (%s) is already open on this object.",
               file->path->s);
        return;
    }

    /* open the file, it will automagically be set on the current object,
       if we are sucessfull, otherwise our stat list is NULL */
    stat = open_file(args[0].u.str,
                     (nargs == 2 ? args[1].u.str : NULL),
                     cur_frame->object);

    if (stat == NULL)
        return;

    pop(nargs);
    push_list(stat);
    list_discard(stat);
}

/*
// -----------------------------------------------------------------
*/
void func_file(void) {
    filec_t * file;
    list_t  * info;
    data_t  * list;

    if (!func_init_0())
        return;

    GET_FILE_CONTROLLER(file)

    info = list_new(5);
    list = list_empty_spaces(info, 5);

    list[0].type = INTEGER;
    list[0].u.val = (long) (file->f.readable ? 1 : 0);
    list[1].type = INTEGER;
    list[1].u.val = (long) (file->f.writable ? 1 : 0);
    list[2].type = INTEGER;
    list[2].u.val = (long) (file->f.closed ? 1 : 0);
    list[3].type = INTEGER;
    list[3].u.val = (long) (file->f.binary ? 1 : 0);
    list[4].type = STRING;
    list[4].u.str = string_dup(file->path);

    push_list(info);
    list_discard(info);
}

/*
// -----------------------------------------------------------------
*/
void func_files(void) {
    data_t   * args;
    string_t * path,
             * name;
    list_t   * out;
    data_t     d;
    struct dirent * dent;
    DIR      * dp;
    struct stat sbuf;

    if (!func_init_1(&args, STRING))
        return;

    path = build_path(args[0].u.str->s, NULL, -1);
    if (!path)
        return;

    if (stat(path->s, &sbuf) == F_FAILURE) {
        cthrow(directory_id, "Unable to find directory \"%s\".", path->s);
        string_discard(path);
        return;
    }

    if (!S_ISDIR(sbuf.st_mode)) {
        cthrow(directory_id, "File \"%s\" is not a directory.", path->s);
        string_discard(path);
        return;
    }

    dp = opendir(path->s);
    out = list_new(0);
    d.type = STRING;
 
    while ((dent = readdir(dp)) != NULL) {
        if (strncmp(dent->d_name, ".", 1) == F_SUCCESS ||
            strncmp(dent->d_name, "..", 2) == F_SUCCESS)
            continue;

#ifdef HAVE_D_NAMLEN
        name = string_from_chars(dent->d_name, dent->d_namlen);
#else
        name = string_from_chars(dent->d_name, strlen(dent->d_name));
#endif
        d.u.str = name;
        out = list_add(out, &d);
        string_discard(name);
    }

    closedir(dp);

    pop(1);
    push_list(out);
    list_discard(out);
}

/*
// -----------------------------------------------------------------
*/
void func_fclose(void) {
    filec_t * file;
    int       err;

    if (!func_init_0())
        return;

    file = find_file_controller(cur_frame->object);

    if (file == NULL) {
        cthrow(file_id, "A file is not open on this object.");
        return;
    }

    err = close_file(file);
    file_discard(file, cur_frame->object);

    if (err) {
        cthrow(file_id, strerror(errno));
        return;
    }

    push_int(1);
}

/*
// -----------------------------------------------------------------
//
// NOTE: args are inverted from the stdio chmod() function call,
// This makes it easier defaulting to this() file.
//
*/
void func_fchmod(void) {
    filec_t  * file;
    data_t   * args;
    string_t * path;
    int        failed,
               nargs;
    long       mode;
    char     * p,
             * ep;

    if (!func_init_1_or_2(&args, &nargs, STRING, STRING))
        return;

    /* frob the string to a mode_t, somewhat taken from FreeBSD's chmod.c */
    p = args[0].u.str->s;

    errno = 0;
    mode = strtol(p, &ep, 8);

    if (*p < '0' || *p > '7' || mode > INT_MAX || mode < 0)
        errno = ERANGE;
    if (errno) {
        cthrow(file_id, "invalid file mode \"%s\": %s", p, strerror(errno));
        return;
    }

    /* don't allow SUID mods, incase somebody is being stupid and
       running the server as root; so it could actually happen */
    if (mode & S_ISUID || mode & S_ISGID || mode & S_ISVTX) {
        cthrow(file_id, "You cannot set sticky bits this way, sorry.");
        return;
    }

    if (nargs == 1) {
        GET_FILE_CONTROLLER(file)
        path = string_dup(file->path);
    } else {
        struct stat sbuf;

        path = build_path(args[1].u.str->s, &sbuf, ALLOW_DIR);
        if (path == NULL)
            return;
    }

    failed = chmod(path->s, mode);
    string_discard(path);

    if (failed) {
        cthrow(file_id, strerror(errno));
        return;
    }

    pop(2);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_frmdir(void) {
    data_t      * args;
    string_t    * path;
    int           err;
    struct stat   sbuf;

    if (!func_init_1(&args, STRING))
        return;

    path = build_path(args[0].u.str->s, &sbuf, ALLOW_DIR);
    if (!path)
        return;

    /* default the mode to 0700, they can chmod it later */
    err = rmdir(path->s);
    string_discard(path);
    if (err != F_SUCCESS) {
        cthrow(file_id, strerror(errno));
        return;
    }

    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_fmkdir(void) {
    data_t      * args;
    string_t    * path;
    int           err;
    struct stat   sbuf;

    if (!func_init_1(&args, STRING))
        return;

    path = build_path(args[0].u.str->s, NULL, -1);
    if (!path)
        return;

    if (stat(path->s, &sbuf) == F_SUCCESS) {
        cthrow(file_id,"A file or directory already exists as \"%s\".",path->s);
        string_discard(path);
        return;
    }

    /* default the mode to 0700, they can chmod it later */
    err = mkdir(path->s, 0700);
    string_discard(path);
    if (err != F_SUCCESS) {
        cthrow(file_id, strerror(errno));
        return;
    }

    pop(1);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_fremove(void) {
    data_t      * args;
    filec_t     * file;
    string_t    * path;
    int           nargs,
                  err;
    struct stat   sbuf;

    if (!func_init_0_or_1(&args, &nargs, STRING))
        return;

    if (nargs) {
        path = build_path(args[0].u.str->s, &sbuf, DISALLOW_DIR);
        if (!path)
            return;
    } else {
        GET_FILE_CONTROLLER(file)
        path = string_dup(file->path);
    }

    err = unlink(path->s);
    string_discard(path);
    if (err != F_SUCCESS) {
        cthrow(file_id, strerror(errno));
        return;
    }

    if (nargs)
        pop(1);

    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_fseek(void) {
    data_t  * args;
    filec_t * file;
    int       whence = SEEK_CUR;

    if (!func_init_2(&args, INTEGER, SYMBOL))
        return;

    GET_FILE_CONTROLLER(file)
 
    if (!file->f.readable || !file->f.writable) {
        cthrow(file_id,
               "File \"%s\" is not both readable and writable.",
               file->path->s);
        return;
    }

    if (strccmp(ident_name(args[1].u.symbol), "SEEK_SET"))
        whence = SEEK_SET;
    else if (strccmp(ident_name(args[1].u.symbol), "SEEK_CUR")) 
        whence = SEEK_CUR;
    else if (strccmp(ident_name(args[1].u.symbol), "SEEK_END")) 
        whence = SEEK_END;

    printf("whence: %d, %s\n", whence, ident_name(args[1].u.symbol));

    if (fseek(file->fp, args[0].u.val, whence) != F_SUCCESS) {
        cthrow(file_id, strerror(errno));
        return;
    }

    pop(2);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_frename(void) {
    string_t    * from,
                * to;
    data_t      * args;
    struct stat   sbuf;
    int           err;

    if (!func_init_2(&args, 0, STRING))
        return;

#if DISABLED
    /* bad behavior, changes the name of the file but doesn't update itself */
    /* somebody may want to add this in eventually, but i'm tired right now */
    if (args[0].type != STRING) {
        filec_t  * file;

        GET_FILE_CONTROLLER(file)
        from = string_dup(file->path);
    } else {

#endif
        from = build_path(args[0].u.str->s, &sbuf, ALLOW_DIR);
        if (!from)
            return;

    /* stat it seperately so that we can give a better error */
    to = build_path(args[1].u.str->s, NULL, ALLOW_DIR);
    if (stat(to->s, &sbuf) < 0) {
        cthrow(file_id, "Destination \"%s\" already exists.", to->s);
        string_discard(to);
        string_discard(from);
        return;
    }

    err = rename(from->s, to->s);
    string_discard(from);
    string_discard(to);
    if (err != F_SUCCESS) {
        cthrow(file_id, strerror(errno));
        return;
    }

    pop(2);
    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_fflush(void) {
    filec_t * file;

    if (!func_init_0())
        return;

    GET_FILE_CONTROLLER(file)

    if (fflush(file->fp) == EOF) {
        cthrow(file_id, strerror(errno));
        return;
    }

    push_int(1);
}

/*
// -----------------------------------------------------------------
*/
void func_feof(void) {
    filec_t * file;

    if (!func_init_0())
        return;

    GET_FILE_CONTROLLER(file);

    if (feof(file->fp))
        push_int(1);
    else
        push_int(0);
}

/*
// -----------------------------------------------------------------
*/
void func_fread(void) {
    data_t  * args;
    int       nargs;
    filec_t  * file;

    if (!func_init_0_or_1(&args, &nargs, INTEGER))
        return;

    GET_FILE_CONTROLLER(file)

    if (!file->f.readable) {
        cthrow(file_id, "File is not readable.");
        return;
    }

    if (file->f.binary) {
        Buffer * buf = NULL;
        int      block = DEF_BLOCKSIZE;

        if (nargs) {
            block = args[0].u.val;
            pop(1);
        }

        buf = read_binary_file(file, block);
   
        if (!buf)
            return;

        push_buffer(buf);
        buffer_discard(buf);
    } else {
        string_t * str = read_file(file);

        if (!str)
            return;

        if (nargs)
            pop(1);

        push_string(str);
        string_discard(str);
    }
}

/*
// -----------------------------------------------------------------
*/
void func_fwrite(void) {
    data_t   * args;
    int        count;
    filec_t  * file;

    if (!func_init_1(&args, 0))
        return;

    GET_FILE_CONTROLLER(file)

    if (!file->f.writable) {
        cthrow(perm_id, "File is not writable.");
        return;
    }

    if (file->f.binary) {
        if (args[0].type != BUFFER) {
            cthrow(type_id,"File type is binary, you may only fwrite buffers.");
            return;
        }
        count = fwrite(args[0].u.buffer->s,
                       sizeof(unsigned char),
                       args[0].u.buffer->len,
                       file->fp);
        count -= args[0].u.buffer->len;
    } else {
        if (args[0].type != STRING) {
            cthrow(type_id, "File type is text, you may only fwrite strings.");
            return;
        }
        count = fwrite(args[0].u.str->s,
                       sizeof(unsigned char),
                       args[0].u.str->len,
                       file->fp);
        count -= args[0].u.str->len;

        /* if we successfully wrote everything, drop a newline on it */
        if (!count)
            fputc((char) 10, file->fp);
    }

    pop(1);
    push_int(count);
}

/*
// -----------------------------------------------------------------
*/
void func_fstat(void) {
    struct stat    sbuf;
    list_t       * stat;
    data_t       * args;
    int            nargs;
    filec_t      * file;

    if (!func_init_0_or_1(&args, &nargs, STRING))
        return;

    if (!nargs) {
        GET_FILE_CONTROLLER(file)
        stat_file(file, &sbuf);
    } else {
        string_t * path = build_path(args[0].u.str->s, &sbuf, ALLOW_DIR);

        if (!path)
            return;

        string_discard(path);
    }

    stat = statbuf_to_list(&sbuf);

    push_list(stat);
    list_discard(stat);
}   


