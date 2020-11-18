#ifndef EXECUTE_H
#define EXECUTE_H
#include"fakedef.h"
#include<pthread.h>
#define THRESHOLD_SIZE 64
typedef struct
{
	int32_t refcount;
	struct VM_Value* car;
	struct VM_Value* cdr;
}VMpair;

typedef struct
{
	int32_t count;
	char* sym;
}VMsym;

/*typedef struct
{
	int32_t size;
	VMsym* symbols;
}symlist;*/

typedef struct VM_Value
{
	//int8_t mark;
	//int8_t type;
	//union
	//{
	//	char* str;
	//	struct VM_Code* prc;
	//	VMpair pair;
	//	ByteArry byte;
	//	int32_t num;
	//	char chr;
	//	double dbl;
	//}u;
	unsigned int mark :1;
	unsigned int access :1;
	unsigned int type :6;
	union
	{
		char* chr;
		struct VM_Str* str;
		VMpair* pair;
		ByteArry* byte;
		int32_t* num;
		double* dbl;
		struct VM_Code* prc;
		void* all;
	}u;
	struct VM_Value* prev;
	struct VM_Value* next;
}VMvalue;

typedef struct VM_Env
{
	struct VM_Env* prev;
	int32_t refcount;
	int32_t bound;
	int32_t size;
	VMvalue** values;
}VMenv;

typedef struct VM_Code
{
	int32_t refcount;
	VMenv* localenv;
//	symlist* syms;
	int32_t size;
	char* code;
}VMcode;

typedef struct VM_Str
{
	int32_t refcount;
	char* str;
}VMstr;
typedef struct VM_Process
{
	struct VM_Process* prev;
	VMenv* localenv;
	int32_t cp;
	VMcode* code;
}VMprocess;

typedef struct
{
	int32_t tp;
	int32_t bp;
	int32_t size;
	VMvalue** values;
}VMstack;

typedef struct
{
	pthread_mutex_t lock;
	int32_t size;
	FILE** files;
}filestack;

typedef struct ThreadMessage
{
	VMvalue* message;
	struct ThreadMessage* next;
}threadMessage;

typedef struct
{
	int32_t VMid;
	pthread_t tid;
	unsigned int mark :1;
	ByteCode* procs;
	VMprocess* curproc;
	VMprocess* mainproc;
	VMstack* stack;
	pthread_mutex_t lock;
	threadMessage* queue;
	filestack* files;
	struct VM_Heap* heap;
}fakeVM;

typedef struct VM_Heap
{
	pthread_mutex_t lock;
	int32_t size;
	int32_t threshold;
	VMvalue* head;
}VMheap;

typedef struct
{
	int32_t size;
	fakeVM** VMs;
}fakeVMStack;

void runFakeVM(fakeVM*);
void printVMvalue(VMvalue*,VMpair*,FILE*,int8_t);
void princVMvalue(VMvalue*,VMpair*,FILE*);
void printProc(VMcode*,FILE*);
fakeVM* newFakeVM(ByteCode*,ByteCode*);
fakeVM* newThreadVM(VMcode*,ByteCode*,filestack*,VMheap*);
void initGlobEnv(VMenv*,VMheap*);
int B_dummy(fakeVM*);
int B_push_nil(fakeVM*);
int B_push_pair(fakeVM*);
int B_push_int(fakeVM*);
int B_push_chr(fakeVM*);
int B_push_dbl(fakeVM*);
int B_push_str(fakeVM*);
int B_push_sym(fakeVM*);
int B_push_byte(fakeVM*);
int B_push_var(fakeVM*);
int B_push_car(fakeVM*);
int B_push_cdr(fakeVM*);
int B_push_top(fakeVM*);
int B_push_proc(fakeVM*);
int B_push_list_arg(fakeVM*);
int B_pop(fakeVM*);
int B_pop_var(fakeVM*);
int B_pop_rest_var(fakeVM*);
int B_pop_car(fakeVM*);
int B_pop_cdr(fakeVM*);
int B_pop_ref(fakeVM*);
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
int B_cast_to_byte(fakeVM*);
int B_is_chr(fakeVM*);
int B_is_int(fakeVM*);
int B_is_dbl(fakeVM*);
int B_is_str(fakeVM*);
int B_is_sym(fakeVM*);
int B_is_prc(fakeVM*);
int B_is_byte(fakeVM*);
int B_nth(fakeVM*);
int B_length(fakeVM*);
int B_append(fakeVM*);
int B_str_cat(fakeVM*);
int B_byte_cat(fakeVM*);
int B_eq(fakeVM*);
int B_eqn(fakeVM*);
int B_equal(fakeVM*);
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
int B_readb(fakeVM*);
int B_write(fakeVM*);
int B_writeb(fakeVM*);
int B_princ(fakeVM*);
int B_tell(fakeVM*);
int B_seek(fakeVM*);
int B_rewind(fakeVM*);
int B_exit(fakeVM*);
int B_go(fakeVM*);
int B_send(fakeVM*);
int B_accept(fakeVM*);
static VMstack* newStack(int32_t);
static filestack* newFileStack();
VMcode* newVMcode(ByteCode*);
static VMvalue* copyValue(VMvalue*,VMheap*);
static VMvalue* newVMvalue(ValueType,void*,VMheap*,int);
static VMvalue* newTrueValue(VMheap*);
static VMvalue* newNilValue(VMheap*);
VMvalue* getTopValue(VMstack*);
VMvalue* getValue(VMstack*,int32_t);
static VMvalue* getCar(VMvalue*);
static VMvalue* getCdr(VMvalue*);
static void freeVMcode(VMcode*);
static int VMvaluecmp(VMvalue*,VMvalue*);
static int subVMvaluecmp(VMvalue*,VMvalue*);
static int numcmp(VMvalue*,VMvalue*);
static int byteArryCmp(ByteArry*,ByteArry*);
void freeVMvalue(VMvalue*);
void freeVMstr(VMstr*);
void freeRef(VMvalue*);
void freeVMstack(VMstack*);
void freeMessage(threadMessage*);
VMenv* newVMenv(int32_t,VMenv*);
VMpair* newVMpair(VMheap*);
VMstr* newVMstr(const char*);
static void freeVMenv(VMenv*);
static void stackRecycle(fakeVM*);
static VMcode* newBuiltInProc(ByteCode*);
static VMprocess* newFakeProcess(VMcode*,VMprocess*);
void printAllStack(VMstack*,FILE*);
static int createNewThread(fakeVM*);
static fakeVMStack* newThreadStack(int32_t);
static threadMessage* newThreadMessage(VMvalue*,VMheap*);
static threadMessage* newMessage(VMvalue*);
static int sendMessage(threadMessage*,fakeVM*);
static VMvalue* acceptMessage(fakeVM*);
static VMvalue* castCptrVMvalue(const ANS_cptr*,VMheap*);
static VMprocess* hasSameProc(VMcode*,VMprocess*);
static int isTheLastExpress(const VMprocess*,const VMprocess*);
static uint8_t* createByteArry(int32_t);
#ifndef _WIN32
static int getch();
#endif
void printEnv(VMenv*,FILE*);
VMheap* newVMheap();
ByteArry* newByteArry(size_t,uint8_t*);
ByteArry* copyByteArry(const ByteArry*);
ByteArry* newEmptyByteArry();
uint8_t* copyArry(size_t,uint8_t*);
VMcode* copyVMcode(VMcode*,VMheap*);
VMenv* copyVMenv(VMenv*,VMheap*);
void writeRef(VMvalue*,VMvalue*);
void copyRef(VMvalue*,VMvalue*);
void GC_mark(fakeVM*);
void GC_markValue(VMvalue*);
void GC_markValueInStack(VMstack*);
void GC_markValueInEnv(VMenv*);
void GC_markValueInCallChain(VMprocess*);
void GC_markMessage(threadMessage*);
void GC_sweep(VMheap*);
void GC_compact(VMheap*);
#endif
