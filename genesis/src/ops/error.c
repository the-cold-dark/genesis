/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include "execute.h"
#include "grammar.h"
#include "cache.h"
#include "opcodes.h"

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
    cData *args, error_arg;
    Int num_args;
    cStr *str;

    if (!func_init_2_or_3(&args, &num_args, T_ERROR, STRING, 0))
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
    cData *args;
    cList *traceback;

    if (!func_init_1(&args, T_ERROR))
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
