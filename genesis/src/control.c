/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: control.c
// ---
//
*/

#define _control_

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "defs.h"

/*
// --------------------------------------------------------------------
*/
int main(int argc, char **argv) {
    printf("Hey, imagine that, your running %s.\n", SYSTEM_TYPE);
    return 0;
}

#undef _control_
