/*
// ColdMUD was created and is copyright 1993, 1994 by Greg Hudson
//
// Genesis is a derivitive work, and is copyright 1995 by Brandon Gillespie.
// Full details and copyright information can be found in the file doc/CREDITS
//
// File: modules/cdc_float.c
// ---
// Floating point functions (including trig)
// Module written by Andy Selle (andy@positronic.res.cmu.edu)
*/

#include "config.h"
#include "defs.h"

#include <time.h>
#include <sys/time.h>    /* for mtime() */
#include "cdc_types.h"
#include "operators.h"
#include "execute.h"
#include "util.h"
#include <math.h>

#define PI 3.141592654
void func_sin(void)
{
	data_t *args;
	double val=0,r=0;	
	
	if(!func_init_1(&args, 0))
		return;


/* If it is not a float return or if it is an int convert */
	if (args[0].type == INTEGER) 
		val=(double)args[0].u.val;
	else if (args[0].type == FLOAT)
		val=(double)args[0].u.fval;
	else
		cthrow(type_id,"Must be a float or an int");

	r=sin(val);

	pop(1);
	push_float((float)r);
}

void func_cos(void)
{
	data_t *args;
	double val=0,r=0;
	
	if (!func_init_1(&args,0))
		return;

	if (args[0].type == INTEGER)
		val=(double)args[0].u.val;
	else if (args[0].type==FLOAT)
		val=(double)args[0].u.fval;
	else
		cthrow(type_id, "must be a float or an int");
	
	r=cos(val);
	
	pop(1);
	push_float((float)r);
}

void func_tan(void)
{
	data_t *args;
	double val=0,r=0;
	
	if (!func_init_1(&args,0))
		return;

	if (args[0].type == INTEGER)
		val=(double)args[0].u.val;
	else if (args[0].type==FLOAT)
		val=(double)args[0].u.fval;
	else
		cthrow(type_id, "Argument must be a float or an INTEGER.");

	r=tan(val);

	pop(1);
	push_float((float)r);
}

void func_sqrt(void)
{
	data_t *args;
	double val=0,r=0;

	if (!func_init_1(&args,0))
		return;
	
	if (args[0].type==INTEGER)
		val=(double)args[0].u.val;
	else if (args[0].type==FLOAT)
		val=(double)args[0].u.fval;
	else
		cthrow(type_id, "Argument must either be type FLOAT or type INTEGER.");

	r=sqrt(val);

	pop(1);
	push_float((float)r);
}

void func_asin(void)
{
	data_t *args;
	double val=0,r=0;
	if(!func_init_1(&args,0))
		return;
	
	if (args[0].type==INTEGER)
		val=(double)args[0].u.val;
	else if (args[0].type==FLOAT)
		val=(double)args[0].u.fval;
	else
		cthrow (type_id, "Argument must either be FLOAT or INTEGER.");

	r=asin(val);
	
	pop(1);
	push_float((float)r);
}

void func_acos(void)
{
	data_t * args;
	double val=0,r=0;
	if (!func_init_1(&args,0))
		return;

	if (args[0].type==INTEGER)
		val=(double)args[0].u.val;
	else if (args[0].type==FLOAT)
		val=(double)args[0].u.fval;
	else
		cthrow(type_id, "Arg must be either a float or an integer.");

	r=acos(val);

	pop(1);
	push_float((float)r);
}

void func_atan(void)
{
	data_t * args;
	double val=0,r=0;
	if (!func_init_1(&args,0))
		return;

	if (args[0].type==INTEGER)
		val=(double)args[0].u.val;
	else if (args[0].type==FLOAT)
		val=(double)args[0].u.fval;
	else
		cthrow (type_id, "Arg must be either a float or an int.");

	r=atan(val);

	pop(1);
	push_float((float)r);
}

void func_atan2(void)
{
	data_t *args;
	double r=0;

	if (!func_init_2(&args, FLOAT, FLOAT))
		return;
	
	r=atan2((double)args[0].u.fval, (double)args[1].u.fval);

	pop(1);
	push_float((float)r);
}



void func_absf(void)
{
	data_t *args;
	double r=0, arg=0;	
	if (!func_init_1(&args, 0))
		return;

	if (args[0].type==FLOAT)
		arg=(double)args[0].u.fval;
	else if (args[0].type==INTEGER)
		arg=(double)args[0].u.val;
	else
		cthrow(type_id, "The arg must be either a float or an int");

	r=fabs(arg);

	pop(1);
	push_float((float)r);
}

void func_logf(void)
{
	data_t *args;
	double r=0, arg=0;
	if (!func_init_1 (&args, 0))
		return;

	if (args[0].type==FLOAT)
		arg=(double)args[0].u.fval;
	else if (args[0].type==INTEGER)
		arg=(double)args[0].u.val;
	else
		cthrow (type_id, "The arg must be either a float or an int.");
	
	r=log(arg);

	pop(1);
	push_float((float)r);
}


void func_deg2rad(void)
{
	data_t *args;
	float r=0, arg=0;

	if (!func_init_1 (&args, 0))
		return;
	
	if (args[0].type==FLOAT)
		arg=args[0].u.fval;
	else if (args[0].type==INTEGER)
		arg=args[0].u.val;
	else 
		cthrow(type_id, "The arg must either be a float or an int");
	
	r=(PI/180)*arg;
	pop(1);
	push_float(r);
}


void func_rad2deg(void)
{
	data_t *args;
	float r=0, arg=0;
	if (!func_init_1 (&args, 0))
		return;
	if (args[0].type==FLOAT)
		arg=args[0].u.fval;
	else if (args[0].type==INTEGER)
		arg=args[0].u.val;
	else
		cthrow (type_id, "The arg must either be a float or an int.");
	
	r=arg/(PI/180);
	pop(1);
	push_float(r);
}

