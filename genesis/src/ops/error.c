/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
*/

#include "config.h"
#include "defs.h"

#if 0
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>  /* func_files() */
#include <fcntl.h>
#include "lookup.h"
#include "functions.h"
#endif
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
