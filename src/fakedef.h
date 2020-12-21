#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<pthread.h>
#define THRESHOLD_SIZE 64
#define SYMUNDEFINE 1
#define SYNTAXERROR 2
#define ILLEGALEXPR 3
#define CIRCULARLOAD 4
#define NUMOFBUILTINSYMBOL 54
#define MAX_STRING_SIZE 64

typedef enum{NIL,IN32,CHR,DBL,SYM,STR,BYTA,PRC,PAIR,ATM} ValueType;

typedef struct
{
	int32_t size;
	char* code;
}ByteCode;

typedef struct
{
	int32_t refcount;
	int32_t size;
	uint8_t* arry;
}ByteArry;

typedef struct
{
	struct AST_Pair* outer;
	int curline;
	ValueType type;
	void* value;
}AST_cptr;

typedef struct AST_Pair
{
	struct AST_Pair* prev;
	AST_cptr car,cdr;
}AST_pair;

typedef struct AST_atom
{
	AST_pair* prev;
	ValueType type;
	union
	{
		char* str;
		char chr;
		int32_t num;
		double dbl;
		ByteArry byta;
	} value;
}AST_atom;

typedef struct
{
	int status;
	AST_cptr* place;
}ErrorStatus;

typedef struct Pre_Def
{
	char* symName;
	AST_cptr obj;//node or symbol or val
	struct Pre_Def* next;
}PreDef;

typedef struct Pre_Env
{
	struct Pre_Env* prev;
	PreDef* symbols;
	struct Pre_Env* next;
}PreEnv;

//typedef struct Pre_Macro
//{
//	AST_cptr* format;
//	AST_cptr* express;
//	struct Pre_Macro* next;
//}PreMacro;

typedef struct Pre_Macro
{
	AST_cptr* pattern;
	int32_t bound;
	ByteCode* proc;
	struct Raw_Proc* procs;
	struct DLL_s* modules;
	struct Pre_Macro* next;
}PreMacro;

typedef struct Pre_MacroSym
{
	char* symName;
	int (*Func)(const AST_cptr*,const AST_cptr*,const char*,PreEnv*);
	struct Pre_MacroSym* next;
}PreMasym;

typedef struct Comp_Def
{
	int32_t count;
	char* symName;
	struct Comp_Def* next;
}CompDef;

typedef struct Comp_Env
{
	struct Comp_Env* prev;
	CompDef* symbols;
}CompEnv;

typedef struct Raw_Proc
{
	int32_t count;
	ByteCode* proc;
//	CompEnv* curEnv;
	struct Raw_Proc* next;
}RawProc;

typedef struct Mod_List
{
	char* name;
	int32_t count;
	struct Mod_List* next;
}Modlist;

typedef struct Interpreter
{
	char* filename;
	char* curDir;
	FILE* file;
	int curline;
	CompEnv* glob;
	RawProc* procs;
	struct DLL_s* modules;
	Modlist* head;
	Modlist* tail;
	struct Interpreter* prev;
}intpr;

typedef struct Pre_Func//function and form
{
	char* functionName;
	ErrorStatus (*function)(AST_cptr*,PreEnv*,struct Interpreter*);
	struct Pre_Func* next;
}PreFunc;

typedef struct Key_Word
{
	char* word;
	struct Key_Word* next;
}KeyWord;

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
	unsigned int mark :1;
	unsigned int access :1;
	unsigned int type :6;
	union
	{
		char* chr;
		struct VM_Str* str;
		VMpair* pair;
		ByteArry* byta;
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
	threadMessage* queueHead;
	threadMessage* queueTail;
	filestack* files;
	struct DLL_s* modules;
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

#ifdef _WIN32
#include<windows.h>
typedef HMODULE DllHandle;
#else
typedef void* DllHandle;
#endif

typedef struct DLL_s
{
	int32_t count;
	DllHandle handle;
	struct DLL_s* next;
}Dlls;

typedef int (*ModFunc)(fakeVM*,pthread_rwlock_t*);

typedef struct String_Match_Pattern
{
	int32_t num;
	char** parts;
	struct String_Match_Pattern* prev;
	struct String_Match_Pattern* next;
}StringMatchPattern;

typedef struct Reader_Macro
{
	StringMatchPattern* pattern;
	AST_cptr* expression;
	struct Reader_Macro* next;
}ReaderMacro;

#endif
