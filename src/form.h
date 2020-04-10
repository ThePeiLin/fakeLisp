#ifndef FORM_H
#define FORM_H
#include"fakedef.h"
static cptr** dealArg(cptr*,int);
static int coutArg(cptr*);
static void deleteArg(cptr**,int);
static int isNil(cptr*);
errorStatus N_quote(cptr*,env*);
errorStatus N_car(cptr*,env*);
errorStatus N_cdr(cptr*,env*);
errorStatus N_cons(cptr*,env*);
errorStatus N_eq(cptr*,env*);
errorStatus N_lambda(cptr*,env*);
errorStatus N_print(cptr*,env*);
errorStatus N_list(cptr*,env*);
errorStatus N_let(cptr*,env*);
errorStatus N_atom(cptr*,env*);
errorStatus N_null(cptr*,env*);
errorStatus N_and(cptr*,env*);
errorStatus N_or(cptr*,env*);
errorStatus N_not(cptr*,env*);
errorStatus N_cond(cptr*,env*);
errorStatus N_define(cptr*,env*);
errorStatus N_defmacro(cptr*,env*);
errorStatus N_exit(cptr*,env*);
errorStatus N_defmacro(cptr*,env*);
errorStatus N_undef(cptr*,env*);
errorStatus N_add(cptr*,env*);
errorStatus N_sub(cptr*,env*);
errorStatus N_mul(cptr*,env*);
errorStatus N_div(cptr*,env*);
errorStatus N_mod(cptr*,env*);
errorStatus N_print(cptr*,env*);
#endif
