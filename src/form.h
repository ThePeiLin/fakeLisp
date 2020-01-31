#ifndef FORM_H
#define FORM_H
#include"fake.h"
int N_quote(cell*,env*);
int N_car(cell*,env*);
int N_cdr(cell*,env*);
int N_cons(cell*,env*);
int N_eq(cell*,env*);
int N_lambda(cell*,env*);
int N_print(cell*,env*);
int N_list(cell*,env);
int N_let(cell*,env*);
int N_atom(cell*,env*);
int N_null(cell*,env*);
int N_cond(cell*,env*);
int N_define(cell*,env*);
int N_defmacro(cell*,env*);
int N_exit(cell*,env*);
#endif
