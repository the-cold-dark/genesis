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
*/

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "config.h"
#include "defs.h"
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
#include "log.h" /* op_log() */
#include "dump.h" /* func_text_dump(), func_backup() */

void func_null_function(void) {
    if (!func_init_0())
        return;
    push_int(1);
}

void func_add_parameter(void) {
    data_t * args;
    long     result;

    /* Accept a symbol argument a data value to assign to the variable. */
    if (!func_init_1(&args, SYMBOL))
	return;

    result = object_add_param(cur_frame->object, args[0].u.symbol);
    if (result == paramexists_id) {
	cthrow(paramexists_id,
	      "Parameter %I already exists.", args[0].u.symbol);
    } else {
	pop(1);
	push_int(1);
    }
}

void func_del_parameter(void) {
    data_t * args;
    long     result;

    /* Accept one symbol argument. */
    if (!func_init_1(&args, SYMBOL))
	return;

    result = object_del_param(cur_frame->object, args[0].u.symbol);
    if (result == paramnf_id) {
	cthrow(paramnf_id, "Parameter %I does not exist.", args[0].u.symbol);
    } else {
	pop(1);
	push_int(1);
    }
}

void func_parameters(void) {
    list_t   * params;
    object_t * obj;
    int        i;
    Var      * var;
    data_t     d;

    /* Accept no arguments. */
    if (!func_init_0())
	return;

    /* Construct the list of variable names. */
    obj = cur_frame->object;
    params = list_new(0);
    d.type = SYMBOL;
    for (i = 0; i < obj->vars.size; i++) {
	var = &obj->vars.tab[i];
	if (var->name != -1 && var->cclass == obj->dbref) {
	    d.u.symbol = var->name;
	    params = list_add(params, &d);
	}
    }

    /* Push the list onto the stack. */
    push_list(params);
    list_discard(params);
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
    if (result == paramnf_id) {
	cthrow(paramnf_id, "No such parameter %I.", args[0].u.symbol);
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
    if (result == paramnf_id) {
	cthrow(paramnf_id, "No such parameter %I.", args[0].u.symbol);
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

    if (result == paramnf_id) {
        cthrow(paramnf_id, "No such parameter %I.", args[0].u.symbol);
    } else {
        pop(1);
        push_int(1);
    }
}

void func_compile_to_method(void) {
    data_t   * args,
             * d;
    method_t * method;
    list_t   * code,
             * errors;
    int        flags=-1, state=-1;

    /* Accept a list of lines of code and a symbol for the name. */
    if (!func_init_2(&args, LIST, SYMBOL))
	return;

    method = object_find_method(cur_frame->object->dbref, args[1].u.symbol);
    if (method && (method->m_flags & MF_LOCK)) {
        cthrow(perm_id, "Method is locked, and cannot be changed.");
        return;
    }

    /* keep these for later reference, if its already around */
    if (method) {
        flags = method->m_flags;
        state = method->m_state;
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
        if (state != -1)
            method->m_state = state;
	object_add_method(cur_frame->object, args[1].u.symbol, method);
	method_discard(method);
    }

    pop(2);
    push_list(errors);
    list_discard(errors);
}

#define LADD(__s) { \
        d.u.symbol = __s; \
        list = list_add(list, &d); \
    }

void func_method_flags(void) {
    data_t  * args,
              d;
    list_t  * list;
    int       flags;

    if (!func_init_1(&args, SYMBOL))
        return;

    flags = object_get_method_flags(cur_frame->object, args[0].u.symbol);
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

    pop(1);
    push_list(list);
    list_discard(list);
}

#undef LADD(__s)

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

void func_method_state(void) {
    int       state;
    data_t  * args;

    if (!func_init_1(&args, SYMBOL))
        return;

    state = object_get_method_state(cur_frame->object, args[0].u.symbol);

    pop(1);

    switch(state) {
        case MS_PUBLIC:    push_symbol(public_id);    break;
        case MS_PROTECTED: push_symbol(protected_id); break;
        case MS_PRIVATE:   push_symbol(private_id);   break;
        case MS_ROOT:      push_symbol(root_id);      break;
        case MS_DRIVER:    push_symbol(driver_id);    break;
        default:           push_int(0);               break;
    }
}

void func_set_method_state(void) {
    int       state = 0;
    data_t  * args;
    Ident     sym;

    if (!func_init_2(&args, SYMBOL, SYMBOL))
        return;

    sym = args[1].u.symbol;
    if (sym == public_id)
        state = MS_PUBLIC;
    else if (sym == protected_id)
        state = MS_PROTECTED;
    else if (sym == private_id)
        state = MS_PRIVATE;
    else if (sym == root_id)
        state = MS_ROOT;
    else if (sym == driver_id)
        state = MS_DRIVER;
    else
        cthrow(type_id, "Invalid method state flag.");

    object_set_method_state(cur_frame->object, args[0].u.symbol, state);

    if (state == -1)
        cthrow(type_id, "Method %D not found.", args[0]);

    pop(2);
    push_int(state);
}

void func_method_args(void) {
    data_t   * args;
    method_t * method;
    string_t * str;
    char     * s;
    int        i;

    /* A symbol for the Method name. */
    if (!func_init_1(&args, SYMBOL))
	return;

    method = object_find_method(cur_frame->object->dbref, args[0].u.symbol);
    if (!method) {
        cthrow(methodnf_id, "Method not found.");
        return;
    }

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
        str = string_addc(str, ';');
    }

    pop(1);
    push_string(str);
    string_discard(str);
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
    method = object_find_method(cur_frame->object->dbref, args[0].u.symbol);
    pop(1);
    if (method) {
	push_dbref(method->object->dbref);
	cache_discard(method->object);
    } else {
	cthrow(methodnf_id, "Method %s not found.",
	      ident_name(args[0].u.symbol));
    }
}

void func_find_next_method(void) {
    data_t   * args;
    method_t * method;

    /* Accept a symbol argument giving the method name, and a dbref giving the
     * object to search past. */
    if (!func_init_2(&args, SYMBOL, DBREF))
	return;

    /* Look for the method on the current object. */
    method = object_find_next_method(cur_frame->object->dbref,
				     args[0].u.symbol, args[1].u.dbref);
    if (method) {
	push_dbref(method->object->dbref);
	cache_discard(method->object);
    } else {
	cthrow(methodnf_id, "Method %s not found.",
	      ident_name(args[0].u.symbol));
    }
}

void func_list_method(void) {
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
	cthrow(methodnf_id, "Method %s not found.",
	      ident_name(args[0].u.symbol));
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
	cthrow(methodnf_id, "No method named %s was found.",
	      ident_name(args[0].u.symbol));
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

void func_ancestors(void) {
    list_t * ancestors;

    /* Accept no arguments. */
    if (!func_init_0())
	return;

    /* Get an ancestors list from the object. */
    ancestors = object_ancestors(cur_frame->object->dbref);
    push_list(ancestors);
    list_discard(ancestors);
}

void func_has_ancestor(void) {
    data_t * args;
    int result;

    /* Accept a dbref to check as an ancestor. */
    if (!func_init_1(&args, DBREF))
	return;

    result = object_has_ancestor(cur_frame->object->dbref, args[0].u.dbref);
    pop(1);
    push_int(result);
}

void func_size(void) {
    /* Accept no arguments. */
    if (!func_init_0())
	return;

    /* Push size of current object. */
    push_int(size_object(cur_frame->object));
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

    /* Verify that all parents are dbrefs. */
    for (d = list_first(parents); d; d = list_next(parents, d)) {
        if (d->type != DBREF) {
            cthrow(type_id, "Parent %D is not a dbref.", d);
            return;
        } else if (!cache_check(d->u.dbref)) {
            cthrow(objnf_id, "Parent %D does not refer to an object.", d);
            return;
        }
    }

    /* Create the new object. */
    obj = object_new(-1, parents);

    pop(1);
    push_dbref(obj->dbref);
    cache_discard(obj);
}

void func_chparents(void) {
    data_t   * args,
             * d;
    object_t * obj;
    int        wrong;

    /* Accept a dbref and a list of parents to change to. */
    if (!func_init_2(&args, DBREF, LIST))
        return;

    if (args[0].u.dbref == ROOT_DBREF) {
        cthrow(perm_id, "You cannot change the root object's parents.");
        return;
    }

    obj = cache_retrieve(args[0].u.dbref);
    if (!obj) {
        cthrow(objnf_id, "Object #%l not found.", args[0].u.dbref);
        return;
    }

    if (!list_length(args[1].u.list)) {
        cthrow(perm_id, "You must specify at least one parent.");
        return;
    }

    /* Call object_change_parents().  This will return the number of a
     * parent which was invalid, or -1 if they were all okay. */
    wrong = object_change_parents(obj, args[1].u.list);
    if (wrong >= 0) {
        d = list_elem(args[1].u.list, wrong);
        if (d->type != DBREF) {
            cthrow(type_id, "New parent %D is not a dbref.", d);
        } else if (d->u.dbref == args[0].u.dbref) {
            cthrow(parent_id, "New parent %D is the same as %D.", d, &args[0]);
        } else if (!cache_check(d->u.dbref)) {
            cthrow(objnf_id, "New parent %D does not exist.", d);
        } else {
            cthrow(parent_id, "New parent %D is a descendent of %D.", d,
                   &args[0]);
        }
    } else {
        pop(2);
        push_int(1);
    }

    cache_discard(obj);
}

void func_destroy(void) {
    data_t   * args;
    object_t * obj;

    /* Accept a dbref to destroy. */
    if (!func_init_1(&args, DBREF))
        return;

    if (args[0].u.dbref == ROOT_DBREF) {
        cthrow(perm_id, "You can't destroy the root object.");
    } else if (args[0].u.dbref == SYSTEM_DBREF) {
        cthrow(perm_id, "You can't destroy the system object.");
    } else {
        obj = cache_retrieve(args[0].u.dbref);
        if (!obj) {
            cthrow(objnf_id, "Object #%l not found.", args[0].u.dbref);
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

    if (binary_dump()) {
        strcpy(buf1, c_dir_binary);
        strcat(buf1, ".bak");

        /* blast old backups */
        if (stat(buf1, &statbuf) != F_FAILURE) {
            sprintf(buf2, "rm -rf %s", buf1);
            if (system(buf2) == SHELL_FAILURE)
                push_int(0);
        }
        sprintf(buf2, "cp -r %s %s", c_dir_binary, buf1);
        if (system(buf2) != SHELL_FAILURE) {
            push_int(1);
        } else {
            push_int(-1);
        }
    } else {
        push_int(0);
    }
}

#undef SHELL_FAILURE

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

void func_binary_dump(void) {

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    push_int(binary_dump());
}

/*
// -----------------------------------------------------------------
//
// Modifies: The object cache and binary database files via cache_sync()
//           and two sweeps through the database.  Modifies the internal
//           dbm state use by dbm_firstkey() and dbm_nextkey().
// Effects: If called by the system object with no arguments, performs a
//          text dump, creating a file 'textdump' which contains a
//          representation of the database in terms of a few simple
//          commands and the ColdC language.  Returns 1 if the text dump
//          succeeds, or 0 if it fails.
//
*/

void func_text_dump(void) {

    /* Accept no arguments. */
    if (!func_init_0())
        return;

    push_int(text_dump());
}

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
    data_t *args, key, value;
    object_t *obj;
    Dict *dict;
    int i;

    if (!func_init_1(&args, DBREF))
        return;

    obj = cache_retrieve(args[0].u.dbref);
    if (!obj) {
        cthrow(objnf_id, "No such object #%l", args[0].u.dbref);
        return;
    }

    /* Construct the dictionary. */
    dict = dict_new_empty();
    for (i = 0; i < obj->vars.size; i++) {
        if (obj->vars.tab[i].name == -1)
            continue;
        key.type = DBREF;
        key.u.dbref = obj->vars.tab[i].cclass;
        if (dict_find(dict, &key, &value) == keynf_id) {
            value.type = DICT;
            value.u.dict = dict_new_empty();
            dict = dict_add(dict, &key, &value);
        }
        key.type = SYMBOL;
        key.u.symbol = obj->vars.tab[i].name;
        value.u.dict = dict_add(value.u.dict, &key, &obj->vars.tab[i].val);
        key.type = DBREF;
        key.u.dbref = obj->vars.tab[i].cclass;
        dict = dict_add(dict, &key, &value);
        dict_discard(value.u.dict);
    }

    cache_discard(obj);
    pop(1);
    push_dict(dict);
    dict_discard(dict);
}

/*
// -----------------------------------------------------------------
*/

void func_add_objname(void) {
    data_t *args;
    int result;

    if (!func_init_2(&args, SYMBOL, DBREF))
        return;

    result = lookup_store_name(args[0].u.symbol, args[1].u.dbref);

    pop(2);
    push_int(result);
}

/*
// -----------------------------------------------------------------
*/

void func_del_objname(void) {
    data_t *args;

    if (!func_init_1(&args, SYMBOL))
        return;

    if (!lookup_remove_name(args[0].u.symbol)) {
        cthrow(namenf_id, "Can't find object name %I.", args[0].u.symbol);
        return;
    }

    pop(1);
    push_int(1);
}

/* ----------------------------------------------------------------- */
/* cancel a suspended task                                           */
void func_cancel(void) {
    data_t *args;
 
    if (!func_init_1(&args, INTEGER))
        return;


    if (!task_lookup(args[0].u.val)) {
        cthrow(type_id, "No such task");
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
        cthrow(type_id, "No such task");
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

    push_int(1);

    task_pause();
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
void func_callers(void) {
    list_t * list;

    if (!func_init_0())
        return;

    list = task_callers();
    push_list(list);
    list_discard(list);
}

void func_bind_function(void) {
    data_t * args;
    int      opcode;

    /* accept a symbol and dbref */
    if (!func_init_2(&args, SYMBOL, DBREF))
        return;

    opcode = find_function(ident_name(args[0].u.symbol));

    if (opcode == -1) {
        cthrow(perm_id, "Attempt to bind function which does not exist.");
        return;
    }

    op_table[opcode].binding = args[1].u.dbref;

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
    /* Accept no arguments, and push the dbref of the current object. */
    if (!func_init_0())
        return;
    push_dbref(cur_frame->object->dbref);
}

void func_definer(void) {
    /* Accept no arguments, and push the dbref of the method definer. */
    if (!func_init_0())
        return;
    push_dbref(cur_frame->method->object->dbref);
}

void func_sender(void) {
    /* Accept no arguments, and push the dbref of the sending object. */
    if (!func_init_0())
        return;
    if (cur_frame->sender == NOT_AN_IDENT)
        push_int(0);
    else
        push_dbref(cur_frame->sender);
}

void func_caller(void) {
    /* Accept no arguments, and push the dbref of the calling method's
     * definer. */
    if (!func_init_0())
        return;
    if (cur_frame->caller == NOT_AN_IDENT)
        push_int(0);
    else
        push_dbref(cur_frame->caller);
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
    push_dbref(cclass);
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
    } else if (args[0].type == DBREF) {
	val = args[0].u.dbref;
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
      } else if (args[0].type == DBREF) {
  	val = args[0].u.dbref;
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

void func_todbref(void) {
    data_t *args;

    /* Accept an integer to convert into a dbref. */
    if (!func_init_1(&args, INTEGER))
	return;

    if (args[0].u.val < 0)
        cthrow(type_id, "objnums must be 0 or greater");

    args[0].u.dbref = args[0].u.val;
    args[0].type = DBREF;
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

    /* Accept one argument of any type (only dbrefs can be valid, though). */
    if (!func_init_1(&args, 0))
	return;

    is_valid = (args[0].type == DBREF && cache_check(args[0].u.dbref));
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

