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
	varstack* localenv;
	int32_t refcount;
	uint32_t size;
	char* code;
}excode;

typedef struct FakeProcess
{
	struct FakeProcess* prev;
	varstack* localenv;
	int32_t cp;
	excode* code;
}fakeprocess;

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

typedef struct ThreadMessage
{
	stackvalue* message;
	struct ThreadMessage* next;
}threadMessage;

typedef struct
{
	fakeprocess* curproc;
	byteCode* procs;
	fakeprocess* mainproc;
	fakestack* stack;
	threadMessage* queue;
	filestack* files;
}fakeVM;

typedef struct
{
	int32_t size;
	fakeVM** VMs;
}fakeVMStack;

void runFakeVM(fakeVM*);
void printStackValue(stackvalue*,FILE*);
fakeVM* newFakeVM(byteCode*,byteCode*);
void initGlobEnv(varstack*);
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
int B_rand(fakeVM*);
int B_atom(fakeVM*);
int B_null(fakeVM*);
int B_init_proc(fakeVM*);
int B_end_proc(fakeVM*);
int B_set_bp(fakeVM*);
int B_invoke(fakeVM*);
int B_res_bp(fakeVM*);
int B_open(fakeVM*);
int B_close(fakeVM*);
int B_cast_to_chr(fakeVM*);
int B_cast_to_int(fakeVM*);
int B_cast_to_dbl(fakeVM*);
int B_cast_to_str(fakeVM*);
int B_cast_to_sym(fakeVM*);
int B_is_chr(fakeVM*);
int B_is_int(fakeVM*);
int B_is_dbl(fakeVM*);
int B_is_str(fakeVM*);
int B_is_sym(fakeVM*);
int B_is_prc(fakeVM*);
int B_nth(fakeVM*);
int B_length(fakeVM*);
int B_append(fakeVM*);
int B_str_cat(fakeVM*);
int B_eq(fakeVM*);
int B_gt(fakeVM*);
int B_ge(fakeVM*);
int B_lt(fakeVM*);
int B_le(fakeVM*);
int B_not(fakeVM*);
int B_jump_if_ture(fakeVM*);
int B_jump_if_false(fakeVM*);
int B_jump(fakeVM*);
int B_getc(fakeVM*);
int B_getch(fakeVM*);
int B_ungetc(fakeVM*);
int B_read(fakeVM*);
int B_write(fakeVM*);
int B_tell(fakeVM*);
int B_seek(fakeVM*);
int B_rewind(fakeVM*);
int B_exit(fakeVM*);
int B_go(fakeVM*);
int B_wait(fakeVM*);
int B_send(fakeVM*);
int B_accept(fakeVM*);
static fakestack* newStack(uint32_t);
static filestack* newFileStack();
excode* newExcode(byteCode*);
static stackvalue* copyValue(stackvalue*);
static stackvalue* newStackValue(valueType);
stackvalue* getTopValue(fakestack*);
stackvalue* getValue(fakestack*,int32_t);
static stackvalue* getCar(stackvalue*);
static stackvalue* getCdr(stackvalue*);
static void freeExcode(excode*);
static int stackvaluecmp(stackvalue*,stackvalue*);
void freeStackValue(stackvalue*);
varstack* newVarStack(int32_t,int,varstack*);
static void freeVarStack(varstack*);
static void stackRecycle(fakeVM*);
static excode* newBuiltInProc(byteCode*);
static fakeprocess* newFakeProcess(excode*,fakeprocess*);
void printAllStack(fakestack*,FILE*);
static int createNewThread(fakeVM*);
static fakeVMStack* newThreadStack(int32_t);
static threadMessage* newMessage(stackvalue*);
static int sendMessage(threadMessage*,fakeVM*);
static stackvalue* acceptMassage(fakeVM*);
static stackvalue* castCptrStackValue(const cptr*);
static fakeprocess* hasSameProc(excode*,fakeprocess*);
static int isTheLastExpress(const fakeprocess*,const fakeprocess*);
#ifndef _WIN32
static int getch();
#endif
void printEnv(varstack*,FILE*);
#endif
