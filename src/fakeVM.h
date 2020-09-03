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
	int inProc;
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
	excode* curproc;
	byteCode* procs;
	excode* mainproc;
	fakestack* stack;
	filestack* files;
}fakeVM;

int RunFakeVM(fakeVM*);
fakeVM* newFakeVM(byteCode*,byteCode*);
int B_dummy(fakeVM*);
int B_push_nil(fakeVM*);
int B_push_pair(fakeVM*);
int B_push_int(fakeVM*);
int B_push_chr(fakeVM*);
int B_push_dbl(fakeVM*);
int B_push_str(fakeVM*);
int B_push_sym(fakeVM*);
int B_push_var(fakeVM*);
int B_push_car(fakeVM*);
int B_push_cdr(fakeVM*);
int B_push_top(fakeVM*);
int B_push_proc(fakeVM*);
int B_pop(fakeVM*);
int B_pop_var(fakeVM*);
int B_pop_rest_var(fakeVM*);
int B_pop_car(fakeVM*);
int B_pop_cdr(fakeVM*);
int B_add(fakeVM*);
int B_sub(fakeVM*);
int B_mul(fakeVM*);
int B_div(fakeVM*);
int B_mod(fakeVM*);
int B_atom(fakeVM*);
int B_null(fakeVM*);
int B_init_proc(fakeVM*);
int B_end_proc(fakeVM*);
int B_set_bp(fakeVM*);
int B_invoke(fakeVM*);
int B_res_bp(fakeVM*);
int B_open(fakeVM*);
int B_close(fakeVM*);
int B_eq(fakeVM*);
int B_ne(fakeVM*);
int B_gt(fakeVM*);
int B_lt(fakeVM*);
int B_le(fakeVM*);
int B_not(fakeVM*);
int B_jump_if_ture(fakeVM*);
int B_jump_if_false(fakeVM*);
int B_jump(fakeVM*);
int B_in(fakeVM*);
int B_out(fakeVM*);
int B_go(fakeVM*);
int B_run(fakeVM*);
int B_send(fakeVM*);
int B_accept(fakeVM*);
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
static varstack* newVarStack(int32_t,int,varstack*);
static void freeVarStack(varstack*);
#endif
