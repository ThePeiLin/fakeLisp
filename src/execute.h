#ifndef EXECUTE_H
#define EXECUTE_H
#include"fakedef.h"

typedef struct
{
	int32_t car;
	int32_t cdr;
}subpair;

typedef struct
{
	valueType type;
	union
	{
		void* obj;
		int32_t num;
		char chr;
		double dbl;
		subpair par;
	}value;
}substackvalue;

typedef struct
{
	int32_t tp;
	uint32_t size;
	substackvalue* values;
}fakesubstack;

typedef struct
{
	valueType type;
	union
	{
		void* obj;
		int32_t num;
		char chr;
		double dbl;
	}value;
}stackvalue;

typedef struct VarStack
{
	struct VarStack* prev;
	struct VarStack* next;
	int beBound;
	int32_t bound;
	uint32_t size;
	stackvalue* values;
}varstack;

typedef struct ExCode
{
	struct ExCode* prev;
	varstack* localenv;
	int32_t cp;
	int32_t refcount;
	uint32_t size;
	char* code;
}excode;

typedef struct
{
	int32_t tp;
	int32_t bp;
	uint32_t size;
	stackvalue* values;
}fakestack;

typedef struct
{
	int count;
	char* name;
}subSymbol;

typedef struct SymbolList
{
	int size;
	subSymbol* symbols;
}symlist;

typedef struct
{
	uint32_t size;
	FILE** files;
}filestack;

typedef struct
{
	byteCode* procs;
	excode* mainproc;
	fakestack* stack;
	filestack* files;
}executor;

int execute(executor*);
int B_dummy(executor*,excode*);
int B_push_nil(executor*,excode*);
int B_push_pair(executor*,excode*);
int B_push_int(executor*,excode*);
int B_push_chr(executor*,excode*);
int B_push_str(executor*,excode*);
int B_push_sym(executor*,excode*);
int B_push_var(executor*,excode*);
int B_push_car(executor*,excode*);
int B_push_cdr(executor*,excode*);
int B_push_proc(executor*,excode*);
int B_pop(executor*,excode*);
int B_pop_var(executor*,excode*);
int B_pop_rest_var(executor*,excode*);
int B_pop_car(executor*,excode*);
int B_pop_cdr(executor*,excode*);
int B_add(executor*,excode*);
int B_sub(executor*,excode*);
int B_mul(executor*,excode*);
int B_div(executor*,excode*);
int B_mod(executor*,excode*);
int B_atom(executor*,excode*);
int B_null(executor*,excode*);
int B_init_proc(executor*,excode*);
int B_end_proc(executor*,excode*);
int B_set_bp(executor*,excode*);
int B_res_bp(executor*,excode*);
int B_open(executor*,excode*);
int B_close(executor*,excode*);
int B_eq(executor*,excode*);
int B_ne(executor*,excode*);
int B_gt(executor*,excode*);
int B_lt(executor*,excode*);
int B_le(executor*,excode*);
int B_not(executor*,excode*);
int B_jump_if_ture(executor*,excode*);
int B_jump_if_false(executor*,excode*);
int B_jump(executor*,excode*);
int B_in(executor*,excode*);
int B_out(executor*,excode*);
int B_go(executor*,excode*);
int B_run(executor*,excode*);
int B_send(executor*,excode*);
int B_accept(executor*,excode*);
static fakesubstack* newSubStack(uint32_t);
static excode* newExcode();
static stackvalue* copyValue(stackvalue*);
static substackvalue* copySubValue(substackvalue*);
static void freeSubStackValue(substackvalue*);
static void freeStackValue(stackvalue*);
static void freeSubStack(fakesubstack*);
static fakesubstack* copyPair(fakesubstack*,int32_t,int32_t);
#endif
