#ifndef FORM_H
#define FORM_H
#include"fakedef.h"
static ANS_cptr** dealArg(ANS_cptr*,int);
static int coutArg(ANS_cptr*);
static void deleteArg(ANS_cptr**,int);
static int isFalse(const ANS_cptr*);
ErrorStatus N_quote(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_car(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_cdr(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_cons(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_eq(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_lambda(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_print(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_list(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_let(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_atom(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_null(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_and(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_or(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_not(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_cond(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_define(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_defmacro(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_exit(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_defmacro(ANS_cptr*,PreEnv*,intpr*);
//ErrorStatus N_undef(ANS_cptr*,PreEnv*);
ErrorStatus N_append(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_extend(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_add(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_sub(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_mul(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_div(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_mod(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_print(ANS_cptr*,PreEnv*,intpr*);
ErrorStatus N_import(ANS_cptr*,PreEnv*,intpr*);
#endif
