#ifndef FORM_H
#define FORM_H
#include"fakedef.h"
static Cptr** dealArg(Cptr*,int);
static int coutArg(Cptr*);
static void deleteArg(Cptr**,int);
static int isFalse(const Cptr*);
ErrorStatus N_quote(Cptr*,env*);
ErrorStatus N_car(Cptr*,env*);
ErrorStatus N_cdr(Cptr*,env*);
ErrorStatus N_cons(Cptr*,env*);
ErrorStatus N_eq(Cptr*,env*);
//ErrorStatus N_lambda(Cptr*,env*);
//ErrorStatus N_print(Cptr*,env*);
ErrorStatus N_list(Cptr*,env*);
ErrorStatus N_let(Cptr*,env*);
ErrorStatus N_ANS_atom(Cptr*,env*);
ErrorStatus N_null(Cptr*,env*);
ErrorStatus N_and(Cptr*,env*);
ErrorStatus N_or(Cptr*,env*);
ErrorStatus N_not(Cptr*,env*);
ErrorStatus N_cond(Cptr*,env*);
ErrorStatus N_define(Cptr*,env*);
ErrorStatus N_defmacro(Cptr*,env*);
ErrorStatus N_exit(Cptr*,env*);
ErrorStatus N_defmacro(Cptr*,env*);
//ErrorStatus N_undef(Cptr*,env*);
ErrorStatus N_append(Cptr*,env*);
ErrorStatus N_extend(Cptr*,env*);
ErrorStatus N_add(Cptr*,env*);
ErrorStatus N_sub(Cptr*,env*);
ErrorStatus N_mul(Cptr*,env*);
ErrorStatus N_div(Cptr*,env*);
ErrorStatus N_mod(Cptr*,env*);
ErrorStatus N_print(Cptr*,env*);
#endif
