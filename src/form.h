#ifndef FORM_H
#define FORM_H
#include"fake.h"
#include"tool.h"
static cptr** dealArg(cptr*,int);
static int coutArg(cptr*);
static void deleteArg(cptr**,int);
int N_quote(cptr*,env*);
int N_car(cptr*,env*);
int N_cdr(cptr*,env*);
int N_cons(cptr*,env*);
int N_eq(cptr*,env*);
int N_lambda(cptr*,env*);
int N_print(cptr*,env*);
int N_list(cptr*,env*);
int N_let(cptr*,env*);
int N_atom(cptr*,env*);
int N_null(cptr*,env*);
int N_cond(cptr*,env*);
int N_define(cptr*,env*);
int N_defmacro(cptr*,env*);
int N_exit(cptr*,env*);
#endif
