#ifndef EXECUTE_H
#define EXECUTE_H
#include"fakedef.h"

typedef struct
{
	struct StackValue* car;
	struct StackValue* cdr;
}fakepair;

typedef struct StackValue
{
	valueType type;
	union
	{
		char* str;
		struct ExCode* prc;
		fakepair par;
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
	stackvalue** values;
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
	stackvalue** values;
}fakestack;

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
}fakeVM;

int RunExecute(fakeVM*);
fakeVM* newExecutor(byteCode*,byteCode*);
int B_dummy(fakeVM*,excode*);
int B_push_nil(fakeVM*,excode*);
int B_push_pair(fakeVM*,excode*);
int B_push_int(fakeVM*,excode*);
int B_push_chr(fakeVM*,excode*);
int B_push_dbl(fakeVM*,excode*);
int B_push_str(fakeVM*,excode*);
int B_push_sym(fakeVM*,excode*);
int B_push_var(fakeVM*,excode*);
int B_push_car(fakeVM*,excode*);
int B_push_cdr(fakeVM*,excode*);
int B_push_proc(fakeVM*,excode*);
int B_pop(fakeVM*,excode*);
int B_pop_var(fakeVM*,excode*);
int B_pop_rest_var(fakeVM*,excode*);
int B_pop_car(fakeVM*,excode*);
int B_pop_cdr(fakeVM*,excode*);
int B_add(fakeVM*,excode*);
int B_sub(fakeVM*,excode*);
int B_mul(fakeVM*,excode*);
int B_div(fakeVM*,excode*);
int B_mod(fakeVM*,excode*);
int B_atom(fakeVM*,excode*);
int B_null(fakeVM*,excode*);
int B_init_proc(fakeVM*,excode*);
int B_end_proc(fakeVM*,excode*);
int B_set_bp(fakeVM*,excode*);
int B_res_bp(fakeVM*,excode*);
int B_open(fakeVM*,excode*);
int B_close(fakeVM*,excode*);
int B_eq(fakeVM*,excode*);
int B_ne(fakeVM*,excode*);
int B_gt(fakeVM*,excode*);
int B_lt(fakeVM*,excode*);
int B_le(fakeVM*,excode*);
int B_not(fakeVM*,excode*);
int B_jump_if_ture(fakeVM*,excode*);
int B_jump_if_false(fakeVM*,excode*);
int B_jump(fakeVM*,excode*);
int B_in(fakeVM*,excode*);
int B_out(fakeVM*,excode*);
int B_go(fakeVM*,excode*);
int B_run(fakeVM*,excode*);
int B_send(fakeVM*,excode*);
int B_accept(fakeVM*,excode*);
static fakestack* newStack(uint32_t);
static filestack* newFileStack();
static excode* newExcode(byteCode*);
static stackvalue* copyValue(stackvalue*);
static stackvalue* newStackValue(valueType);
static stackvalue* getTopValue(fakestack*);
static stackvalue* getValue(fakestack*,int32_t);
static stackvalue* getCar(stackvalue*);
static stackvalue* getCdr(stackvalue*);
static char* copyStr(const char*);
static void freeExcode(excode*);
static void freeStackValue(stackvalue*);
static fakepair* copyPair(const fakepair*);
static fakepair* newFakePair(stackvalue*,stackvalue*);
#endif
