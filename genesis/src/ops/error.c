/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#include "defs.h"

#include "execute.h"
#include "grammar.h"
#include "cache.h"
#include "opcodes.h"

COLDC_FUNC(error) {
    if (!func_init_0())
	return;

    if (!cur_frame->handler_info) {
	cthrow(error_id, "Request for handler info outside handler.");
	return;
    }

    push_error(cur_frame->handler_info->error);
}

COLDC_FUNC(error_message) {
    if (!func_init_0())
        return;

    if (!cur_frame->handler_info) {
        cthrow(error_id, "Request for handler info outside handler.");
    }

    push_string(cur_frame->handler_info->error_message);
}

COLDC_FUNC(error_data) {
    if (!func_init_0())
        return;

    if (!cur_frame->handler_info) {
        cthrow(error_id, "Request for handler info outside handler.");
    }

    /* need a push_data() type thing here for
     * cur_frame->handler_info->error_data */

}

COLDC_FUNC(traceback) {
    cList * tb;

    if (!func_init_0())
	return;

    if (!cur_frame->handler_info) {
	cthrow(error_id, "Request for handler info outside handler.");
	return;
    }

    if (cur_frame->handler_info->cached_traceback != NULL) {
        push_list(cur_frame->handler_info->cached_traceback);
    } else {
        tb = generate_traceback(cur_frame->handler_info->traceback);
        cur_frame->handler_info->cached_traceback = tb;
        push_list(tb);
    }

}

COLDC_FUNC(throw) {
    cData *args, *error_arg;
    Int num_args;
    cStr *str;

    if (!func_init_2_or_3(&args, &num_args, T_ERROR, STRING, 0))
	return;

    /* Throw the error. */
    str = string_dup(args[1].u.str);
    if (num_args == 3) {
        error_arg = (cData *)emalloc(sizeof(cData));
	data_dup(error_arg, &args[2]);
	user_error(args[0].u.error, str, error_arg);
	data_discard(error_arg);
        efree(error_arg);
    } else {
	user_error(args[0].u.error, str, NULL);
    }
    string_discard(str);
}

COLDC_FUNC(rethrow) {
    cData          * args;
    Traceback_info * traceback;
    cStr           * explanation;
    cData          * arg;

    if (!func_init_1(&args, T_ERROR))
	return;

    if (!cur_frame->handler_info) {
	cthrow(error_id, "Request for handler info outside handler.");
	return;
    }

    /* Abort the current frame and propagate an error in the caller. */
    traceback = traceback_info_dup(cur_frame->handler_info->traceback);
    explanation = string_dup(cur_frame->handler_info->error_message);
    arg = (cData *)emalloc(sizeof(cData));
    data_dup(arg, cur_frame->handler_info->error_data);
    frame_return();
    propagate_error(traceback, ERR1, explanation, arg);
    string_discard(explanation);
    data_discard(arg);
    efree(arg);
}

