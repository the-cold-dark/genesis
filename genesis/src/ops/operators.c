/*
// Full copyright information is available in the file ../doc/CREDITS
//
// Generic operators
*/

#include "defs.h"

#include <string.h>
#include "operators.h"
#include "execute.h"
#include "lookup.h"
#include "util.h"

/*
// -----------------------------------------------------------------
//
// The following are basic syntax operations
//
*/

void op_comment(void) {
    /* Do nothing, just increment the program counter past the comment. */
    cur_frame->pc++;
    /* actually, increment the number of ticks left too, since comments
       really don't do anything */
    cur_frame->ticks++;
    /* decrement system tick */
    tick--;
}

void op_pop(void) {
    pop(1);
}

void op_set_local(void) {
    cData *var;

    /* Copy data in top of stack to variable. */
    var = &stack[cur_frame->var_start + cur_frame->opcodes[cur_frame->pc++]];
    data_discard(var);
    data_dup(var, &stack[stack_pos - 1]);
}

void op_set_obj_var(void) {
    Long ind, id, result;
    cData *val;

    ind = cur_frame->opcodes[cur_frame->pc++];
    id = object_get_ident(cur_frame->method->object, ind);
    val = &stack[stack_pos - 1];
    result = object_assign_var(cur_frame->object, cur_frame->method->object,
			       id, val);
    if (result == varnf_id)
	cthrow(varnf_id, "Object variable %I not found.", id);
}

void op_if(void) {
    /* Jump if the condition is false. */
    if (!data_true(&stack[stack_pos - 1]))
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    else
	cur_frame->pc++;
    pop(1);
}

void op_else(void) {
    cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
}

void op_for_range(void) {
    Int var;
    cData *range;

    var = cur_frame->var_start + cur_frame->opcodes[cur_frame->pc + 1];
    range = &stack[stack_pos - 2];

    /* Make sure we have an integer range. */
    if (range[0].type != INTEGER || range[1].type != INTEGER) {
	cthrow(type_id, "Range bounds (%D, %D) are not both integers.",
	      &range[0], &range[1]);
	return;
    }

    if (range[0].u.val > range[1].u.val) {
	/* We're finished; pop the range and jump to the end. */
	pop(2);
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    } else {
	/* Replace the index variable with the lower range bound, increment the
	 * range, and continue. */
	data_discard(&stack[var]);
	stack[var] = range[0];
	range[0].u.val++;
	cur_frame->pc += 2;
    }
}

void op_for_list(void) {
    cData *counter;
    cData *domain;
    Int var, len;
    cList *pair;

    counter = &stack[stack_pos - 1];
    domain = &stack[stack_pos - 2];
    var = cur_frame->var_start + cur_frame->opcodes[cur_frame->pc + 1];

    /* Make sure we're iterating over a list.  We know the counter is okay. */
    if (domain->type != LIST && domain->type != DICT) {
	cthrow(type_id, "Domain (%D) is not a list or dictionary.", domain);
	return;
    }

    len = (domain->type == LIST) ? list_length(domain->u.list)
				 : dict_size(domain->u.dict);

    if (counter->u.val >= len) {
	/* We're finished; pop the list and counter and jump to the end. */
	pop(2);
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
	return;
    }

    /* Replace the index variable with the next list element and increment
     * the counter. */
    data_discard(&stack[var]);
    if (domain->type == LIST) {
	data_dup(&stack[var], list_elem(domain->u.list, counter->u.val));
    } else {
	pair = dict_key_value_pair(domain->u.dict, counter->u.val);
	stack[var].type = LIST;
	stack[var].u.list = pair;
    }
    counter->u.val++;
    cur_frame->pc += 2;
}

void op_while(void) {
    if (!data_true(&stack[stack_pos - 1])) {
	/* The condition expression is false.  Jump to the end of the loop. */
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    } else {
	/* The condition expression is true; continue. */
	cur_frame->pc += 2;
    }
    pop(1);
}

void op_switch(void) {
    /* This opcode doesn't actually do anything; it just provides a place-
     * holder for a break statement. */
    cur_frame->pc++;
}

void op_case_value(void) {
    /* There are two expression values on the stack: the controlling expression
     * for the switch statement, and the value for this case.  If they are
     * equal, pop them off the stack and jump to the body of this case.
     * Otherwise, just pop the value for this case, and go on. */
    if (data_cmp(&stack[stack_pos - 2], &stack[stack_pos - 1]) == 0) {
	pop(2);
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    } else {
	pop(1);
	cur_frame->pc++;
    }
}

void op_case_range(void) {
    cData *switch_expr, *range;
    Int is_match;

    switch_expr = &stack[stack_pos - 3];
    range = &stack[stack_pos - 2];

    /* Verify that range[0] and range[1] make a value type. */
    if (range[0].type != range[1].type) {
	cthrow(type_id, "%D and %D are not of the same type.",
	      &range[0], &range[1]);
	return;
    } else if (range[0].type != INTEGER && range[0].type != STRING) {
	cthrow(type_id, "%D and %D are not integers or strings.", &range[0],
	      &range[1]);
	return;
    }

    /* Decide if this is a match.  In order for it to be a match, switch_expr
     * must be of the same type as the range expressions, must be greater than
     * or equal to the lower bound of the range, and must be less than or equal
     * to the upper bound of the range. */
    is_match = (switch_expr->type == range[0].type);
    is_match = (is_match) && (data_cmp(switch_expr, &range[0]) >= 0);
    is_match = (is_match) && (data_cmp(switch_expr, &range[1]) <= 0);

    /* If it's a match, pop all three expressions and jump to the case body.
     * Otherwise, just pop the range and go on. */
    if (is_match) {
	pop(3);
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    } else {
	pop(2);
	cur_frame->pc++;
    }
}

void op_last_case_value(void) {
    /* There are two expression values on the stack: the controlling expression
     * for the switch statement, and the value for this case.  If they are
     * equal, pop them off the stack and go on.  Otherwise, just pop the value
     * for this case, and jump to the next case. */
    if (data_cmp(&stack[stack_pos - 2], &stack[stack_pos - 1]) == 0) {
	pop(2);
	cur_frame->pc++;
    } else {
	pop(1);
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    }
}

void op_last_case_range(void) {
    cData *switch_expr, *range;
    Int is_match;

    switch_expr = &stack[stack_pos - 3];
    range = &stack[stack_pos - 2];

    /* Verify that range[0] and range[1] make a value type. */
    if (range[0].type != range[1].type) {
	cthrow(type_id, "%D and %D are not of the same type.",
	      &range[0], &range[1]);
	return;
    } else if (range[0].type != INTEGER && range[0].type != STRING) {
	cthrow(type_id, "%D and %D are not integers or strings.", &range[0],
	      &range[1]);
	return;
    }

    /* Decide if this is a match.  In order for it to be a match, switch_expr
     * must be of the same type as the range expressions, must be greater than
     * or equal to the lower bound of the range, and must be less than or equal
     * to the upper bound of the range. */
    is_match = (switch_expr->type == range[0].type);
    is_match = (is_match) && (data_cmp(switch_expr, &range[0]) >= 0);
    is_match = (is_match) && (data_cmp(switch_expr, &range[1]) <= 0);

    /* If it's a match, pop all three expressions and go on.  Otherwise, just
     * pop the range and jump to the next case. */
    if (is_match) {
	pop(3);
	cur_frame->pc++;
    } else {
	pop(2);
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    }
}

void op_end_case(void) {
    /* Jump to end of switch statement. */
    cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
}

void op_default(void) {
    /* Pop the controlling switch expression. */
    pop(1);
}

void op_end(void) {
    /* Jump to the beginning of the loop or condition expression. */
    cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
}

void op_break(void) {
    Int n, op;

    /* Get loop instruction from argument. */
    n = cur_frame->opcodes[cur_frame->pc];

    /* If it's a for loop, pop the loop information on the stack (either a list
     * and an index, or two range bounds. */
    op = cur_frame->opcodes[n];
    if (op == FOR_LIST || op == FOR_RANGE)
	pop(2);

    /* Jump to the end of the loop. */
    cur_frame->pc = cur_frame->opcodes[n + 1];
}

void op_continue(void) {
    /* Jump back to the beginning of the loop.  If it's a WHILE loop, jump back
     * to the beginning of the condition expression. */
    cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    if (cur_frame->opcodes[cur_frame->pc] == WHILE)
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc + 2];
}

void op_return(void) {
    Long objnum;

    objnum = cur_frame->object->objnum;
    frame_return();
    if (cur_frame)
	push_objnum(objnum);
}

void op_return_expr(void) {
    cData *val;

    /* Return, and push frame onto caller stack.  Transfers reference count to
     * caller stack.  Assumes (correctly) that there is space on the caller
     * stack. */
    val = &stack[--stack_pos];
    frame_return();
    if (cur_frame) {
	stack[stack_pos] = *val;
	stack_pos++;
    } else {
	data_discard(val);
    }
}

void op_catch(void) {
    Error_action_specifier *spec;

    /* Make a new error action specifier and push it onto the stack. */
    spec = EMALLOC(Error_action_specifier, 1);
    spec->type = CATCH;
    spec->stack_pos = stack_pos;
    spec->arg_pos = arg_pos;
    spec->u.ccatch.handler = cur_frame->opcodes[cur_frame->pc++];
    spec->u.ccatch.error_list = cur_frame->opcodes[cur_frame->pc++];
    spec->next = cur_frame->specifiers;
    cur_frame->specifiers = spec;
}

void op_catch_end(void) {
    /* Pop the error action specifier for the catch statement, and jump past
     * the handler. */
    pop_error_action_specifier();
    cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
}

void op_handler_end(void) {
    pop_handler_info();
}

void op_zero(void) {
    /* Push a zero. */
    push_int(0);
}

void op_one(void) {
    /* Push a one. */
    push_int(1);
}

void op_integer(void) {
    push_int(cur_frame->opcodes[cur_frame->pc++]);
}

void op_float(void) {
    push_float(*((float*)(&cur_frame->opcodes[cur_frame->pc++])));
}

void op_string(void) {
    cStr *str;
    Int ind = cur_frame->opcodes[cur_frame->pc++];

    str = object_get_string(cur_frame->method->object, ind);
    push_string(str);
}

void op_objnum(void) {
    Int id;

    id = cur_frame->opcodes[cur_frame->pc++];
    push_objnum(id);
}

void op_symbol(void) {
    Int ind, id;

    ind = cur_frame->opcodes[cur_frame->pc++];
    id = object_get_ident(cur_frame->method->object, ind);
    push_symbol(id);
}

void op_error(void) {
    Int ind, id;

    ind = cur_frame->opcodes[cur_frame->pc++];
    id = object_get_ident(cur_frame->method->object, ind);
    push_error(id);
}

void op_objname(void) {
    Int ind, id;
    Long objnum;

    ind = cur_frame->opcodes[cur_frame->pc++];
    id = object_get_ident(cur_frame->method->object, ind);
    if (lookup_retrieve_name(id, &objnum))
	push_objnum(objnum);
    else
	cthrow(namenf_id, "Can't find object name %I.", id);
}

void op_get_local(void) {
    Int var;

    /* Push value of local variable on stack. */
    var = cur_frame->var_start + cur_frame->opcodes[cur_frame->pc++];
    check_stack(1);
    data_dup(&stack[stack_pos], &stack[var]);
    stack_pos++;
}

void op_get_obj_var(void) {
    Long ind, id, result;
    cData val;

    /* Look for variable, and push it onto the stack if we find it. */
    ind = cur_frame->opcodes[cur_frame->pc++];
    id = object_get_ident(cur_frame->method->object, ind);
    result = object_retrieve_var(cur_frame->object, cur_frame->method->object,
				 id, &val);
    if (result == varnf_id) {
	cthrow(varnf_id, "Object variable %I not found.", id);
    } else {
	check_stack(1);
	stack[stack_pos] = val;
	stack_pos++;
    }
}

void op_start_args(void) {
    /* Resize argument stack if necessary. */
    if (arg_pos == arg_size) {
	arg_size = arg_size * 2 + ARG_STACK_MALLOC_DELTA;
	arg_starts = EREALLOC(arg_starts, Int, arg_size);
    }

    /* Push stack position onto argument start stack. */
    arg_starts[arg_pos] = stack_pos;
    arg_pos++;
}

/* this is redundant, but 99% of the time we dont need it assigned, so
   assigning it always would be wasteful */
#define setobj(__o) ( d.type = OBJNUM, d.u.objnum = __o )

INTERNAL void handle_method_call(Int result, cObjnum objnum, Ident message) {
    cData d;

    switch (result) {
        case CALL_OK:
        case CALL_NATIVE:
            break;
        case CALL_NUMARGS:
            interp_error(numargs_id, numargs_str);
            break;
        case CALL_MAXDEPTH:
            setobj(objnum);
            cthrow(maxdepth_id, "Maximum call depth exceeded.");
            break;
        case CALL_OBJNF:
            setobj(objnum);
            cthrow(objnf_id, "Target (%D) not found.", &d);
            break;
        case CALL_METHNF:
            setobj(objnum);
            cthrow(methodnf_id, "%D.%I not found.", &d, message);
            break;
        case CALL_PRIVATE:
            setobj(objnum);
            cthrow(private_id, "%D.%I is private.", &d, message);
            break;
        case CALL_PROT:
            setobj(objnum);
            cthrow(protected_id, "%D.%I is protected.", &d, message);
            break;
        case CALL_ROOT:
            setobj(objnum);
            cthrow(root_id, "%D.%I can only be called by $root.", &d, message);
            break;
        case CALL_DRIVER:
            setobj(objnum);
            cthrow(driver_id, "%D.%I can only be by the driver.", &d, message);
            break;
    }
}

void op_pass(void) {
    Int arg_start;

    arg_start = arg_starts[--arg_pos];

    /* Attempt to pass the message we're processing. */
    handle_method_call(pass_method(arg_start, arg_start),
                       cur_frame->object->objnum,
                       cur_frame->method->name);
}

void op_message(void) {
    Int arg_start, ind;
    cData *target;
    Long message, objnum;
    cFrob *frob;

    ind = cur_frame->opcodes[cur_frame->pc++];
    message = object_get_ident(cur_frame->method->object, ind);

    /* figure up the start of the args in the stack */
    arg_start = arg_starts[--arg_pos];

    /* our target 'object' or data */
    target = &stack[arg_start - 1];

    switch (target->type) {
        case OBJNUM:
            objnum = target->u.objnum;
            break;
        case FROB:
            /* Convert the frob to its rep and pass as first argument. */
            frob = target->u.frob;
            objnum = frob->cclass;
            *target = frob->rep;
            arg_start--;
            TFREE(frob, 1);
            break;
        default:
            if (!lookup_retrieve_name(data_type_id(target->type), &objnum)) {
                cthrow(objnf_id, "No object for data type %I.",
                       data_type_id(target->type));
                return;
            }
            arg_start--;
            break;
    }

    /* Attempt to send the message. */
    ident_dup(message);

    handle_method_call(call_method(objnum, message, target - stack, arg_start),
                       objnum, message);

    ident_discard(message);
}

void op_expr_message(void) {
    Int arg_start;
    cData *target, *message_data;
    Long objnum, message;

    arg_start = arg_starts[--arg_pos];
    target = &stack[arg_start - 2];

    message_data = &stack[arg_start - 1];

    if (message_data->type != SYMBOL) {
	cthrow(type_id, "Message (%D) is not a symbol.", message_data);
	return;
    }

    message = ident_dup(message_data->u.symbol);

    switch (target->type) {
        case OBJNUM:
            objnum = target->u.objnum;
            break;
        case FROB:
            objnum = target->u.frob->cclass;

            /* Pass frob rep as first argument (where the method data is now) */
            data_discard(message_data);
            *message_data = target->u.frob->rep;
            arg_start--;

            /* Discard the frob and replace it with a dummy value. */
            TFREE(target->u.frob, 1);
            target->type = INTEGER;
            target->u.val = 0;
            break;
        default:
            if (!lookup_retrieve_name(data_type_id(target->type), &objnum)) {
                cthrow(objnf_id,
                       "No object for data type %I",
                       data_type_id(target->type));
                ident_discard(message);
                return;
            }
            arg_start--;
            data_discard(message_data);
            data_dup(&stack[arg_start], target);
            break;
    }

    /* Attempt to send the message. */
    ident_dup(message);
    
    handle_method_call(call_method(objnum, message, target - stack, arg_start),
                       objnum, message);

    ident_discard(message);
}

void op_list(void) {
    Int start, len;
    cList *list;
    cData *d;

    start = arg_starts[--arg_pos];
    len = stack_pos - start;

    /* Move the elements into a list. */
    list = list_new(len);
    d = list_empty_spaces(list, len);
    MEMCPY(d, &stack[start], len);
    stack_pos = start;

    /* Push the list onto the stack where elements began. */
    push_list(list);
    list_discard(list);
}

void op_dict(void) {
    Int start, len;
    cList *list;
    cData *d;
    cDict *dict;

    start = arg_starts[--arg_pos];
    len = stack_pos - start;

    /* Move the elements into a list. */
    list = list_new(len);
    d = list_empty_spaces(list, len);
    MEMCPY(d, &stack[start], len);
    stack_pos = start;

    /* Construct a dictionary from the list. */
    dict = dict_from_slices(list);
    list_discard(list);
    if (!dict) {
	cthrow(type_id, "Arguments were not all two-element lists.");
    } else {
	push_dict(dict);
	dict_discard(dict);
    }
}

void op_buffer(void) {
    Int start, len, i;
    cBuf *buf;

    start = arg_starts[--arg_pos];
    len = stack_pos - start;
    for (i = 0; i < len; i++) {
	if (stack[start + i].type != INTEGER) {
	    cthrow(type_id, "Element %d (%D) is not an integer.", i + 1,
		  &stack[start + i]);
	    return;
	}
    }
    buf = buffer_new(len);
    for (i = 0; i < len; i++)
	buf->s[i] = ((uLong) stack[start + i].u.val) % (1 << 8);
    stack_pos = start;
    push_buffer(buf);
    buffer_discard(buf);
}

void op_frob(void) {
    cData *cclass, *rep;

    cclass = &stack[stack_pos - 2];
    rep = &stack[stack_pos - 1];
    if (cclass->type != OBJNUM) {
	cthrow(type_id, "Class (%D) is not a objnum.", cclass);
    } else if (rep->type != LIST && rep->type != DICT) {
	cthrow(type_id, "Rep (%D) is not a list or dictionary.", rep);
    } else {
      cObjnum objnum = cclass->u.objnum;
      cclass->type = FROB;
      cclass->u.frob = TMALLOC(cFrob, 1);
      cclass->u.frob->cclass = objnum;
      data_dup(&cclass->u.frob->rep, rep);
      pop(1);
    }
}

#define _CHECK_TYPE {\
        if (ind->type != INTEGER) {\
            cthrow(type_id, "Offset (%D) is not an integer.", ind);\
            return;\
        }\
    }
#define _CHECK_LENGTH(len) {\
        i = ind->u.val - 1;\
        if (i < 0) {\
            cthrow(range_id, "Index (%d) is less than one.", i + 1);\
            return;\
        } else if (i > len - 1) {\
            cthrow(range_id, "Index (%d) is greater than length (%d)",\
                  i + 1, len);\
            return;\
        }\
    }

void op_index(void) {
    cData *d, *ind, element;
    Int i;
    cStr *str;

    d = &stack[stack_pos - 2];
    ind = &stack[stack_pos - 1];

    switch (d->type) {
        case LIST:
            _CHECK_TYPE
            _CHECK_LENGTH(list_length(d->u.list))
	    data_dup(&element, list_elem(d->u.list, i));
	    pop(2);
	    stack[stack_pos] = element;
	    stack_pos++;
            return;
        case STRING:
            _CHECK_TYPE
            _CHECK_LENGTH(string_length(d->u.str))
	    str = string_from_chars(string_chars(d->u.str) + i, 1);
	    pop(2);
	    push_string(str);
	    string_discard(str);
            return;
        case DICT:
            /* Get the value corresponding to a key. */
            if (dict_find(d->u.dict, ind, &element) == keynf_id) {
                cthrow(keynf_id, "Key (%D) is not in the dictionary.", ind);
            } else {
                pop(1);
                data_discard(d);
                *d = element;
            }
            return;
        case BUFFER:
            _CHECK_TYPE
            _CHECK_LENGTH(buffer_len(d->u.buffer))
            i = buffer_retrieve(d->u.buffer, i);
            pop(2);
            push_int(i);
            return;
        default:
            cthrow(type_id, "Data (%D) cannot be indexed with []", d);
            return;
    }
}

void op_and(void) {
    /* Short-circuit if left side is false; otherwise discard. */
    if (!data_true(&stack[stack_pos - 1])) {
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    } else {
	cur_frame->pc++;
	pop(1);
    }
}

void op_or(void) {
    /* Short-circuit if left side is true; otherwise discard. */
    if (data_true(&stack[stack_pos - 1])) {
	cur_frame->pc = cur_frame->opcodes[cur_frame->pc];
    } else {
	cur_frame->pc++;
	pop(1);
    }
}

void op_splice(void) {
    Int i;
    cList *list;
    cData *d;

    if (stack[stack_pos - 1].type != LIST) {
	cthrow(type_id, "%D is not a list.", &stack[stack_pos - 1]);
	return;
    }
    list = stack[stack_pos - 1].u.list;

    /* Splice the list onto the stack, overwriting the list. */
    check_stack(list_length(list) - 1);
    for (d = list_first(list), i=0; d; d = list_next(list, d), i++)
	data_dup(&stack[stack_pos - 1 + i], d);
    stack_pos += list_length(list) - 1;

    list_discard(list);
}

void op_critical(void) {
    Error_action_specifier *spec;

    /* Make an error action specifier for the critical expression, and push it
     * onto the stack. */
    spec = EMALLOC(Error_action_specifier, 1);
    spec->type = CRITICAL;
    spec->stack_pos = stack_pos;
    spec->arg_pos = arg_pos;
    spec->u.critical.end = cur_frame->opcodes[cur_frame->pc++];
    spec->next = cur_frame->specifiers;
    cur_frame->specifiers = spec;
}

void op_critical_end(void) {
    pop_error_action_specifier();
}

void op_propagate(void) {
    Error_action_specifier *spec;

    /* Make an error action specifier for the critical expression, and push it
     * onto the stack. */
    spec = EMALLOC(Error_action_specifier, 1);
    spec->type = PROPAGATE;
    spec->stack_pos = stack_pos;
    spec->u.propagate.end = cur_frame->opcodes[cur_frame->pc++];
    spec->next = cur_frame->specifiers;
    cur_frame->specifiers = spec;
}

void op_propagate_end(void) {
    pop_error_action_specifier();
}

/*
// -----------------------------------------------------------------
//
// The following are extended operations, math and the like
//
*/

/* All of the following functions are interpreter opcodes, so they require
   that the interpreter data (the globals in execute.c) be in a state
   consistent with interpretation.  They may modify the interpreter data
   by pushing and popping the data stack or by throwing exceptions. */

/* Effects: Pops the top value on the stack and pushes its logical inverse. */
void op_not(void) {
    cData *d = &stack[stack_pos - 1];
    Int val = !data_true(d);

    /* Replace d with the inverse of its truth value. */
    data_discard(d);
    d->type = INTEGER;
    d->u.val = val;
}

/* Effects: If the top value on the stack is an integer, pops it and pushes its
 *	    its arithmetic inverse. */
void op_negate(void) {
    cData *d = &stack[stack_pos - 1];

    /* Replace d with -d. */
    if (d->type == INTEGER) {
        d->u.val = -(d->u.val);
    } else if (d->type == FLOAT) {
        d->u.fval = -(d->u.fval);
    } else {
	cthrow(type_id, "Argument (%D) is not an integer or float.", d);
    }
}

/* Effects: If the top two values on the stack are integers, pops them and
 *	    pushes their product. */

void op_multiply(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    switch (d1->type) {
        case FLOAT:
            switch (d2->type) {
                case INTEGER:
                    d2->type = FLOAT;
                    d2->u.fval = (float) d2->u.val;
                case FLOAT:
                    break;
                default:
                    goto error;
            }
    
          float_label:
            d1->u.fval *= d2->u.fval;
            break;

        case INTEGER:

            switch (d2->type) {
                case INTEGER:
                    break;
                case FLOAT:
                    d1->type = FLOAT;
                    d1->u.fval = (float) d1->u.val;
                    goto float_label;
                default:
                    goto error;
            }

            d1->u.val *= d2->u.val;
            break;

        default:
        error:
            cthrow(type_id, "%D and %D are not integers or floats.", d1, d2);
            return;
    }

    pop(1);
}

void op_doeq_multiply(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    switch (d1->type) {
        case FLOAT:
            switch (d2->type) {
                case INTEGER:
                    d2->type = FLOAT;
                    d2->u.fval = (float) d2->u.val;
                case FLOAT:
                    break;
                default:
                    goto error;
            }
    
          float_label:
            d1->u.fval = d2->u.fval * d1->u.fval;
            break;

        case INTEGER:

            switch (d2->type) {
                case INTEGER:
                    break;
                case FLOAT:
                    d1->type = FLOAT;
                    d1->u.fval = (float) d1->u.val;
                    goto float_label;
                default:
                    goto error;
            }

            d1->u.val = d2->u.val * d1->u.val;
            break;

        default:
        error:
            cthrow(type_id, "%D and %D are not integers or floats.", d1, d2);
            return;
    }

    pop(1);
}


/* Effects: If the top two values on the stack are integers and the second is
 *	    not zero, pops them, divides the first by the second, and pushes
 *	    the quotient. */
void op_divide(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    switch (d1->type) {
        case FLOAT:
            switch (d2->type) {
                case INTEGER:
                    d2->type = FLOAT;
                    d2->u.fval = (float) d2->u.val;
                case FLOAT:
                    break;
                default:
                    goto error;
            }
    
          float_label:
            if (d2->u.fval == 0.0) {
                cthrow(div_id, "Attempt to divide %D by zero.", d1);
                return;
            }
            d1->u.fval /= d2->u.fval;
            break;

        case INTEGER:

            switch (d2->type) {
                case INTEGER:
                    break;
                case FLOAT:
                    d1->type = FLOAT;
                    d1->u.fval = (float) d1->u.val;
                    goto float_label;
                default:
                    goto error;
            }

            if (d2->u.val == 0) {
                cthrow(div_id, "Attempt to divide %D by zero.", d1);
                return;
            }
            d1->u.val /= d2->u.val;
            break;

        default:
        error:
            cthrow(type_id, "%D and %D are not integers or floats.", d1, d2);
            return;
    }

    pop(1);
}

void op_doeq_divide(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    switch (d1->type) {
        case FLOAT:
            switch (d2->type) {
                case INTEGER:
                    d2->type = FLOAT;
                    d2->u.fval = (float) d2->u.val;
                case FLOAT:
                    break;
                default:
                    goto error;
            }
    
          float_label:
            if (d2->u.fval == 0.0) {
                cthrow(div_id, "Attempt to divide %D by zero.", d1);
                return;
            }
            d1->u.fval = d2->u.fval / d1->u.fval;
            break;

        case INTEGER:

            switch (d2->type) {
                case INTEGER:
                    break;
                case FLOAT:
                    d1->type = FLOAT;
                    d1->u.fval = (float) d1->u.val;
                    goto float_label;
                default:
                    goto error;
            }

            if (d2->u.val == 0) {
                cthrow(div_id, "Attempt to divide %D by zero.", d1);
                return;
            }
            d1->u.val = d2->u.val / d1->u.val;
            break;

        default:
        error:
            cthrow(type_id, "%D and %D are not integers or floats.", d1, d2);
            return;
    }

    pop(1);
}

/* Effects: If the top two values on the stack are integers and the second is
 *	    not zero, pops them, divides the first by the second, and pushes
 *	    the remainder. */
void op_modulo(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    /* Make sure we're multiplying two integers. */
    if (d1->type != INTEGER || d2->type != INTEGER) {
	cthrow(type_id, "Both sides of the modulo must be integers.");
    } else if (d2->u.val == 0) {
	cthrow(div_id, "Attempt to divide %D by zero.", d1);
    } else {
	/* Replace d1 with d1 % d2, and pop d2. */
	d1->u.val %= d2->u.val;
	pop(1);
    }
}

/* Effects: If the top two values on the stack are integers, pops them and
 *	    pushes their sum.  If the top two values are strings, pops them,
 *	    concatenates the second onto the first, and pushes the result. */
void op_add(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    switch (d1->type) {
      case INTEGER:

        switch (d2->type) {
            case FLOAT:
                d1->type = FLOAT;
                d1->u.fval = (float) d1->u.val;
                goto float_label;
            case STRING:
                d1->u.str = data_tostr(d1);
                d1->type = STRING;
                goto string;
            case INTEGER:
                d1->u.val += d2->u.val;
                break;
            default:
                goto error;
        }

        break;

      case FLOAT:

        switch (d2->type) {
            case INTEGER:
                d2->type = FLOAT;
                d2->u.fval = (float) d2->u.val;
            case FLOAT:
                goto float_label;
            case STRING:
                d1->u.str = data_tostr(d1);
                d1->type = STRING;
                goto string;
            default:
                goto error;
        }

      float_label:
        d1->u.fval += d2->u.fval;
        break;

      case STRING: {
        cStr * str;

        switch (d2->type) {
            case STRING:
                break;
            case SYMBOL:
                str = data_tostr(d2);
                data_discard(d2);
                d2->type = STRING;
                d2->u.str = str;
                break;
            default:
                str = data_to_literal(d2);
                data_discard(d2);
                d2->type = STRING;
                d2->u.str = str;
        }

      string:                                                  /* string: */

	anticipate_assignment();
	d1->u.str = string_add(d1->u.str, d2->u.str);
        break;

      }

      case LIST:

        switch (d2->type) {
            case LIST:
	        anticipate_assignment();
        	d1->u.list = list_append(d1->u.list, d2->u.list);
                break;
            case STRING: {
                cStr * str = data_to_literal(d1);
                data_discard(d1);
                d1->type = STRING;
                d1->u.str = str;
                goto string;
            }
            default:
                goto error;
        }
        break;

      case BUFFER:

        if (d2->type == BUFFER) {
	    anticipate_assignment();
            d1->u.buffer = buffer_append(d1->u.buffer, d2->u.buffer);
            break;
        }

      default:

        if (d2->type == STRING) {
            cStr * str;

            if (d1->type == SYMBOL) {
                str = data_tostr(d1);
                data_discard(d1);
                d1->type = STRING;
                d1->u.str = str;
            } else {
                str = data_to_literal(d1);
                data_discard(d1);
                d1->type = STRING;
                d1->u.str = str;
            }

            goto string;
        }

      error:

	cthrow(type_id, "Cannot add %D and %D.", d1, d2);
	return;
    }

    pop(1);
}

void op_doeq_add(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    switch (d1->type) {
      case INTEGER:

        switch (d2->type) {
            case FLOAT:
                d1->type = FLOAT;
                d1->u.fval = (float) d1->u.val;
                goto float_label;
            case STRING:
                d1->u.str = data_tostr(d1);
                d1->type = STRING;
                goto string;
            case INTEGER:
                d1->u.val = d2->u.val + d1->u.val;
                pop(1);
                return;
            default:
                goto error;
        }

        break;

      case FLOAT:

        switch (d2->type) {
            case INTEGER:
                d2->type = FLOAT;
                d2->u.fval = (float) d2->u.val;
            case FLOAT:
                goto float_label;
            case STRING:
                d1->u.str = data_tostr(d1);
                d1->type = STRING;
                goto string;
            default:
                goto error;
        }

      float_label:
        d1->u.fval = d2->u.fval + d1->u.fval;
        pop(1);
        return;

      case STRING: {
        cStr * str;

        switch (d2->type) {
            case STRING:
                break;
            case SYMBOL:
                str = data_tostr(d2);
                data_discard(d2);
                d2->type = STRING;
                d2->u.str = str;
                break;
            default:
                str = data_to_literal(d2);
                data_discard(d2);
                d2->type = STRING;
                d2->u.str = str;
        }

      string:                                                  /* string: */

        /* straighten things out by swapping strings */
	anticipate_assignment();
        str = d2->u.str;
        d2->u.str = d1->u.str;
	d1->u.str = string_add(str, d2->u.str);
        pop(1);
        return;

      }

      case LIST:

        switch (d2->type) {
            case LIST: {
                cList * list = d2->u.list;
	        anticipate_assignment();
                d2->u.list = d1->u.list;
        	d1->u.list = list_append(list, d2->u.list);
                pop(1);
                return;
            }
            case STRING: {
                cStr * str = data_to_literal(d1);
                data_discard(d1);
                d1->type = STRING;
                d1->u.str = str;
                goto string;
            }
            default:
                goto error;
        }
        break;

      case BUFFER:

        if (d2->type == BUFFER) {
            cBuf * buf = d2->u.buffer;

	    anticipate_assignment();
            d2->u.buffer = d1->u.buffer;
            d1->u.buffer = buffer_append(buf, d2->u.buffer);
            pop(1);
            return;
        }

      default:

        if (d2->type == STRING) {
            cStr * str;

            if (d1->type == SYMBOL) {
                str = data_tostr(d1);
                data_discard(d1);
                d1->type = STRING;
                d1->u.str = str;
            } else {
                str = data_to_literal(d1);
                data_discard(d1);
                d1->type = STRING;
                d1->u.str = str;
            }

            goto string;
        }

      error:

	cthrow(type_id, "Cannot add %D and %D.", d1, d2);
	return;
    }
}

/* Effects: Adds two lists.  (This is used for [@foo, ...];) */
void op_splice_add(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    /* No need to check if d2 is a list, due to code generation. */
    if (d1->type != LIST) {
	cthrow(type_id, "%D is not a list.", d1);
	return;
    }

    anticipate_assignment();
    d1->u.list = list_append(d1->u.list, d2->u.list);
    pop(1);
}

/* Effects: If the top two values on the stack are integers, pops them and
 *	    pushes their difference. */

void op_subtract(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    switch (d1->type) {
        case FLOAT:
            switch (d2->type) {
                case INTEGER:
                    d2->type = FLOAT;
                    d2->u.fval = (float) d2->u.val;
                case FLOAT:
                    break;
                default:
                    goto error;
            }
    
          float_label:
            d1->u.fval -= d2->u.fval;
            break;

        case INTEGER:

            switch (d2->type) {
                case INTEGER:
                    break;
                case FLOAT:
                    d1->type = FLOAT;
                    d1->u.fval = (float) d1->u.val;
                    goto float_label;
                default:
                    goto error;
            }

            d1->u.val -= d2->u.val;
            break;

        default:
        error:
            cthrow(type_id, "%D and %D are not integers or floats.", d1, d2);
            return;
    }

    pop(1);
}

void op_doeq_subtract(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    switch (d1->type) {
        case FLOAT:
            switch (d2->type) {
                case INTEGER:
                    d2->type = FLOAT;
                    d2->u.fval = (float) d2->u.val;
                case FLOAT:
                    break;
                default:
                    goto error;
            }
    
          float_label:
            d1->u.fval = d2->u.fval - d1->u.fval;
            break;

        case INTEGER:

            switch (d2->type) {
                case INTEGER:
                    break;
                case FLOAT:
                    d1->type = FLOAT;
                    d1->u.fval = (float) d1->u.val;
                    goto float_label;
                default:
                    goto error;
            }

            d1->u.val = d2->u.val - d1->u.val;
            break;

        default:
        error:
            cthrow(type_id, "%D and %D are not integers or floats.", d1, d2);
            return;
    }

    pop(1);
}

/* Effects: If the top value on the stack is an integer or float,
 *          it is incremented by one. */
void op_increment(void) {
    cData * v, * sd = &stack[stack_pos - 1];

    if (sd->type != FLOAT && sd->type != INTEGER) {
        cthrow(type_id, "%D is not an integer or float.", sd);
        return;
    }
    
    switch (cur_frame->opcodes[cur_frame->pc]) {
        case SET_LOCAL:
            cur_frame->pc++;
            v=&stack[cur_frame->var_start+cur_frame->opcodes[cur_frame->pc++]];
            data_discard(v);
            if (sd->type == FLOAT)
                v->u.fval = sd->u.fval + 1;
            else
                v->u.val = sd->u.val + 1;
            break;
        case SET_OBJ_VAR: {
            Long ind, id, result;
            cData d;

            cur_frame->pc++;
            ind = cur_frame->opcodes[cur_frame->pc++];
            id  = object_get_ident(cur_frame->method->object, ind);
            if (sd->type == FLOAT) {
                d.type = FLOAT;
                d.u.fval = sd->u.fval + 1;
            } else {
                d.type = INTEGER;
                d.u.val = sd->u.val + 1;
            }
            result = object_assign_var(cur_frame->object,
                                       cur_frame->method->object,
                                       id, &d);
            if (result == varnf_id)
        	cthrow(varnf_id, "Object variable %I not found.", id);
            break;
        }
    }
}

void op_p_increment(void) {
    cData *d1 = &stack[stack_pos - 1];

    switch (d1->type) {
        case FLOAT:
            d1->u.fval++;
            break;
        case INTEGER:
            d1->u.val++;
            break;
        default:
            cthrow(type_id, "%D is not an integer or float.", d1);
            return;
    }
}

/* Effects: If the top value on the stack is an integer or float,
 *          it is decrimented by one. */
void op_decrement(void) {
    cData * v, * sd = &stack[stack_pos - 1];

    if (sd->type != FLOAT && sd->type != INTEGER) {
        cthrow(type_id, "%D is not an integer or float.", sd);
        return;
    }
    
    switch (cur_frame->opcodes[cur_frame->pc]) {
        case SET_LOCAL:
            cur_frame->pc++;
            v=&stack[cur_frame->var_start+cur_frame->opcodes[cur_frame->pc++]];
            data_discard(v);
            if (sd->type == FLOAT)
                v->u.fval = sd->u.fval - 1;
            else
                v->u.val = sd->u.val - 1;
            break;
        case SET_OBJ_VAR: {
            Long ind, id, result;
            cData d;

            cur_frame->pc++;
            ind = cur_frame->opcodes[cur_frame->pc++];
            id  = object_get_ident(cur_frame->method->object, ind);
            if (sd->type == FLOAT) {
                d.type = FLOAT;
                d.u.fval = sd->u.fval - 1;
            } else {
                d.type = INTEGER;
                d.u.val = sd->u.val - 1;
            }
            result = object_assign_var(cur_frame->object,
                                       cur_frame->method->object,
                                       id, &d);
            if (result == varnf_id)
        	cthrow(varnf_id, "Object variable %I not found.", id);
            break;
        }
    }
}

void op_p_decrement(void) {
    cData *d1 = &stack[stack_pos - 1];

    switch (d1->type) {
        case FLOAT:
            d1->u.fval--;
            break;
        case INTEGER:
            d1->u.val--;
            break;
        default:
            cthrow(type_id, "%D is not an integer or float.", d1);
            return;
    }
}

/* Effects: Pops the top two values on the stack and pushes 1 if they are
 *	    equal, 0 if not. */
void op_equal(void)
{
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];
    Int val = (data_cmp(d1, d2) == 0);

    pop(2);
    push_int(val);
}

/* Effects: Pops the top two values on the stack and returns 1 if they are
 *	    unequal, 0 if they are equal. */   
void op_not_equal(void)
{
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];
    Int val = (data_cmp(d1, d2) != 0);

    pop(2);
    push_int(val);
}

/* Definition: Two values are comparable if they are of the same type and that
 * 	       type is integer or string. */

/* Effects: If the top two values on the stack are comparable, pops them and
 *	    pushes 1 if the first is greater than the second, 0 if not. */
void op_greater(void)
{
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];
    Int val, t = d1->type;

    if (d1->type == FLOAT && d2->type == INTEGER) {
        d2->type = FLOAT;
        d2->u.fval = (float) d2->u.val;
    } else if (d2->type == FLOAT && d1->type == INTEGER) {
        d1->type = FLOAT;
        d1->u.fval = (float) d1->u.val;
    }

    if (d1->type != d2->type) {
	cthrow(type_id, "%D and %D are not of the same type.", d1, d2);
    } else if (t != INTEGER && t != STRING && t != FLOAT) {
	cthrow(type_id,"%D and %D are not integers, floats or strings.",d1,d2);
    } else {
	/* Discard d1 and d2 and push the appropriate truth value. */
	val = (data_cmp(d1, d2) > 0);
	pop(2);
	push_int(val);
    }
}

/* Effects: If the top two values on the stack are comparable, pops them and
 *	    pushes 1 if the first is greater than or equal to the second, 0 if
 *	    not. */
void op_greater_or_equal(void)
{
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];
    Int val, t = d1->type;

    if (d1->type == FLOAT && d2->type == INTEGER) {
        d2->type = FLOAT;
        d2->u.fval = (float) d2->u.val;
    } else if (d1->type == INTEGER && d2->type == FLOAT) {
        d1->type = FLOAT;
        d1->u.fval = (float) d1->u.val;
    }

    if (d1->type != d2->type) {
	cthrow(type_id, "%D and %D are not of the same type.", d1, d2);
    } else if (t != INTEGER && t != FLOAT && t != STRING) {
	cthrow(type_id,"%D and %D are not integers, floats or strings.",d1,d2);
    } else {
	/* Discard d1 and d2 and push the appropriate truth value. */
	val = (data_cmp(d1, d2) >= 0);
	pop(2);
	push_int(val);
    }
}

/* Effects: If the top two values on the stack are comparable, pops them and
 *	    pushes 1 if the first is less than the second, 0 if not. */
void op_less(void)
{
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];
    Int val, t = d1->type;

    if (d1->type == FLOAT && d2->type == INTEGER) {
        d2->type = FLOAT;
        d2->u.fval = (float) d2->u.val;
    } else if (d1->type == INTEGER && d2->type == FLOAT) {
        d1->type = FLOAT;
        d1->u.fval = (float) d1->u.val;
    }

    if (d1->type != d2->type) {
	cthrow(type_id, "%D and %D are not of the same type.", d1, d2);
    } else if (t != INTEGER && t != FLOAT && t != STRING) {
	cthrow(type_id,"%D and %D are not integers, floats or strings.",d1,d2);
    } else {
	/* Discard d1 and d2 and push the appropriate truth value. */
	val = (data_cmp(d1, d2) < 0);
	pop(2);
	push_int(val);
    }
}

/* Effects: If the top two values on the stack are comparable, pops them and
 *	    pushes 1 if the first is greater than or equal to the second, 0 if
 *	    not. */
void op_less_or_equal(void)
{
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];
    Int val, t = d1->type;

    if (d1->type == FLOAT && d2->type == INTEGER) {
        d2->type = FLOAT;
        d2->u.fval = (float) d2->u.val;
    } else if (d1->type == INTEGER && d2->type == FLOAT) {
        d1->type = FLOAT;
        d1->u.fval = (float) d1->u.val;
    }

    if (d1->type != d2->type) {
	cthrow(type_id, "%D and %D are not of the same type.", d1, d2);
    } else if (t != INTEGER && t != FLOAT && t != STRING) {
	cthrow(type_id,"%D and %D are not integers, floats or strings.",d1,d2);
    } else {
	/* Discard d1 and d2 and push the appropriate truth value. */
	val = (data_cmp(d1, d2) <= 0);
	pop(2);
	push_int(val);
    }
}

/* Effects: If the top value on the stack is a string or a list, pops the top
 *	    two values on the stack and pushes the location of the first value
 *	    in the second (where the first element is 1), or 0 if the first
 *	    value does not exist in the second. */
#define uchar unsigned char
#if 0
#define BFIND(__buf, __char) \
    ((unsigned char *) memchr(__buf->s, (unsigned char) __char, __buf->len))
#endif

void op_in(void)
{
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];
    Int pos = -1;

    switch (d2->type) {
        case LIST:
            pos = list_search(d2->u.list, d1);
            break;
        case STRING: {
            char * s;

            if (d1->type != STRING)
                goto error;
            s = strcstr(string_chars(d2->u.str), string_chars(d1->u.str));
            if (s)
                pos = s - string_chars(d2->u.str);
            break;
        }
        case BUFFER: {
            uchar * s;
            cBuf * buf = d2->u.buffer;

            if (d1->type == INTEGER) {
                s = (uchar *) memchr(buf->s, (uchar) d1->u.val, buf->len);
                if (s)
                    pos = s - buf->s;
            } else if (d2->type == BUFFER) {
                Int    len = d1->u.buffer->len - 1;
                uchar  * p = d1->u.buffer->s;

                s = (uchar *) memchr(buf->s, *p, buf->len);

                if (s && MEMCMP(s + 1, p + 1, len) == 0)
                    pos = s - buf->s;
            } else
                goto error;

            break;
        }
        default:
        error:
            cthrow(type_id, "Cannot search for %D in %D.", d1, d2);
            return;
    }

    pop(2);
    push_int(pos + 1);
}

/*
// ----------------------------------------------------------------
// Bitwise integer operators.
//
// Added by Jeff Kesselman, March 1995
// ----------------------------------------------------------------
*/

/*
// Effects: If the top two values on the stack are integers 
//	    pops them, bitwise ands them, and pushes
//	    the result.
*/
void op_bwand(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    /* Make sure we're multiplying two integers. */
    if (d1->type != INTEGER) {
        cthrow(type_id, "Left side (%D) is not an integer.", d1);
    } else if (d2->type != INTEGER) {
        cthrow(type_id, "Right side (%D) is not an integer.", d2);
    } else if (d2->u.val == 0) {
        cthrow(div_id, "Attempt to divide %D by zero.", d1);
    } else {
        /* Replace d1 with d1 / d2, and pop d2. */
        d1->u.val &= d2->u.val;
        pop(1);
    }
}


/*
// Effects: If the top two values on the stack are integers 
//          pops them, bitwise ors them, and pushes
//          the result.
*/
void op_bwor(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    /* Make sure we're multiplying two integers. */
    if (d1->type != INTEGER) {
        cthrow(type_id, "Left side (%D) is not an integer.", d1);
    } else if (d2->type != INTEGER) {
        cthrow(type_id, "Right side (%D) is not an integer.", d2);
    } else if (d2->u.val == 0) {
        cthrow(div_id, "Attempt to divide %D by zero.", d1);
    } else {
        /* Replace d1 with d1 / d2, and pop d2. */
        d1->u.val |= d2->u.val;
        pop(1);
    }
}

/*
// Effects: If the top two values on the stack are integers 
//          pops them, shifts the left operand to the right
//          right-operand times, and pushes the result.
*/
void op_bwshr(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    /* Make sure we're multiplying two integers. */
    if (d1->type != INTEGER) {
        cthrow(type_id, "Left side (%D) is not an integer.", d1);
    } else if (d2->type != INTEGER) {
        cthrow(type_id, "Right side (%D) is not an integer.", d2);
    } else if (d2->u.val == 0) {
        cthrow(div_id, "Attempt to divide %D by zero.", d1);
    } else {
        /* Replace d1 with d1 / d2, and pop d2. */
        d1->u.val >>= d2->u.val;
        pop(1);
    }
}

/*
// Effects: If the top two values on the stack are integers 
//          pops them, shifts the left operand to the left
//          right-operand times, and pushes  the result.
*/
void op_bwshl(void) {
    cData *d1 = &stack[stack_pos - 2];
    cData *d2 = &stack[stack_pos - 1];

    /* Make sure we're multiplying two integers. */
    if (d1->type != INTEGER) {
        cthrow(type_id, "Left side (%D) is not an integer.", d1);
    } else if (d2->type != INTEGER) {
        cthrow(type_id, "Right side (%D) is not an integer.", d2);
    } else if (d2->u.val == 0) {
        cthrow(div_id, "Attempt to divide %D by zero.", d1);
    } else {
        /* Replace d1 with d1 / d2, and pop d2. */
        d1->u.val <<= d2->u.val;
        pop(1);
    }
}

