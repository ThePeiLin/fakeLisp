#ifndef FORM_H
#define FORM_H
#include"fakedef.h"
static ANS_cptr** dealArg(ANS_cptr*,int);
static int coutArg(ANS_cptr*);
static void deleteArg(ANS_cptr**,int);
static int isFalse(const ANS_cptr*);
ErrorStatus N_quote(ANS_cptr*,env*);
ErrorStatus N_car(ANS_cptr*,env*);
ErrorStatus N_cdr(ANS_cptr*,env*);
ErrorStatus N_cons(ANS_cptr*,env*);
ErrorStatus N_eq(ANS_cptr*,env*);
//ErrorStatus N_lambda(ANS_cptr*,env*);
//ErrorStatus N_print(ANS_cptr*,env*);
ErrorStatus N_list(ANS_cptr*,env*);
ErrorStatus N_let(ANS_cptr*,env*);
ErrorStatus N_ANS_atom(ANS_cptr*,env*);
ErrorStatus N_null(ANS_cptr*,env*);
ErrorStatus N_and(ANS_cptr*,env*);
ErrorStatus N_or(ANS_cptr*,env*);
ErrorStatus N_not(ANS_cptr*,env*);
ErrorStatus N_cond(ANS_cptr*,env*);
ErrorStatus N_define(ANS_cptr*,env*);
ErrorStatus N_defmacro(ANS_cptr*,env*);
ErrorStatus N_exit(ANS_cptr*,env*);
ErrorStatus N_defmacro(ANS_cptr*,env*);
//ErrorStatus N_undef(ANS_cptr*,env*);
ErrorStatus N_append(ANS_cptr*,env*);
ErrorStatus N_extend(ANS_cptr*,env*);
ErrorStatus N_add(ANS_cptr*,env*);
ErrorStatus N_sub(ANS_cptr*,env*);
ErrorStatus N_mul(ANS_cptr*,env*);
ErrorStatus N_div(ANS_cptr*,env*);
ErrorStatus N_mod(ANS_cptr*,env*);
ErrorStatus N_print(ANS_cptr*,env*);
#endif
