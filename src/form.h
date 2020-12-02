#ifndef FORM_H
#define FORM_H
#include"fakedef.h"
static AST_cptr** dealArg(AST_cptr*,int);
static int coutArg(AST_cptr*);
static void deleteArg(AST_cptr**,int);
static int isFalse(const AST_cptr*);
ErrorStatus N_quote(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_car(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_cdr(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_cons(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_eq(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_lambda(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_print(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_list(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_let(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_atom(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_null(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_and(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_or(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_not(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_cond(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_define(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_defmacro(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_exit(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_defmacro(AST_cptr*,PreEnv*,intpr*);
//ErrorStatus N_undef(AST_cptr*,PreEnv*);
ErrorStatus N_append(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_extend(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_add(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_sub(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_mul(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_div(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_mod(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_print(AST_cptr*,PreEnv*,intpr*);
ErrorStatus N_import(AST_cptr*,PreEnv*,intpr*);
#endif
