#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<pthread.h>
#define THRESHOLD_SIZE 64
#define NUMOFBUILTINSYMBOL 47
#define MAX_STRING_SIZE 64

typedef enum{NIL=0,IN32,CHR,DBL,SYM,STR,BYTS,PRC,CONT,CHAN,FP,DLL,DLPROC,PAIR,ATM} ValueType;

typedef enum
{
	SYMUNDEFINE=1,
	SYNTAXERROR,
	INVALIDEXPR,
	CIRCULARLOAD,
	INVALIDPATTERN,
	WRONGARG,
	STACKERROR,
	TOOMANYARG,
	TOOFEWARG,
	CANTCREATETHREAD,
	THREADERROR,
	MACROEXPANDFAILED,
	INVOKEERROR,
	LOADDLLFAILD,
	INVALIDSYMBOL
}ErrorType;

typedef struct Com_Stack
{
	void** data;
	uint32_t size;
	uint32_t top;
}ComStack;

typedef struct
{
	int32_t size;
	char* code;
}ByteCode;

typedef struct ByteCode_Lable
{
	char* label;
	int32_t place;
	struct ByteCode_Lable* next;
}ByteCodeLabel;

typedef struct Byte_Code_with_Line_Number_Node
{
	int32_t ls;
	struct Line_Number_Table_Node** l;
	ByteCode* bc;
}ByteCodelnt;

typedef struct
{
	int32_t refcount;
	int32_t size;
	uint8_t* str;
}ByteString;

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

typedef struct Channel
{
	int32_t max;
	int32_t size;
	int32_t refcount;
	pthread_mutex_t lock;
	struct Thread_Message* head;
	struct Thread_Message* tail;
}Chanl;

typedef struct VM_File
{
	int32_t refcount;
	FILE* fp;
}VMfp;

typedef struct AST_atom
{
	AST_pair* prev;
	ValueType type;
	union
	{
		char* str;
		char chr;
		int32_t in32;
		double dbl;
		ByteString byts;
	} value;
}AST_atom;

typedef struct
{
	int status;
	AST_cptr* place;
}ErrorStatus;

typedef struct Pre_Def
{
	char* symbol;
	AST_cptr obj;//node or symbol or val
	struct Pre_Def* next;
}PreDef;

typedef struct Pre_Env
{
	struct Pre_Env* prev;
	PreDef* symbols;
	struct Pre_Env* next;
}PreEnv;

typedef struct Pre_Macro
{
	AST_cptr* pattern;
	struct Line_Number_Table* lnt;
	ByteCode* proc;
	struct Raw_Proc* procs;
	struct Pre_Macro* next;
}PreMacro;

typedef struct Symbol_Table_Node
{
	char* symbol;
	int32_t id;
}SymTabNode;

typedef struct Symbol_Table
{
	int32_t size;
	SymTabNode** list;
	SymTabNode** idl;
}SymbolTable;

typedef struct Comp_Def
{
	int32_t id;
	struct Comp_Def* next;
}CompDef;

typedef struct Comp_Env
{
	struct Comp_Env* prev;
	CompDef* head;
}CompEnv;

typedef struct Raw_Proc
{
	int32_t count;
	ByteCode* proc;
	struct Raw_Proc* prev;
	struct Raw_Proc* next;
}RawProc;

typedef struct Interpreter
{
	char* filename;
	char* curDir;
	FILE* file;
	int curline;
	CompEnv* glob;
	RawProc* procs;
	struct Symbol_Table* table;
	struct Line_Number_Table* lnt;
	struct Interpreter* prev;
}Intpr;

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
		ByteString* byts;
		int32_t* in32;
		double* dbl;
		struct VM_Code* prc;
		struct VM_Continuation* cont;
		struct Channel* chan;
		struct VM_File* fp;
		struct VM_Dll* dll;
		struct VM_Dlproc* dlproc;
		void* all;
	}u;
	struct VM_Value* prev;
	struct VM_Value* next;
}VMvalue;

typedef struct VM_Env_node
{
	int32_t id;
	VMvalue* value;
}VMenvNode;

typedef struct VM_Env
{
	struct VM_Env* prev;
	int32_t refcount;
	int32_t size;
	VMenvNode** list;
}VMenv;

typedef struct VM_Code
{
	int32_t id;
	int32_t refcount;
	VMenv* prevEnv;
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
	int32_t tpsi;
	int32_t tptp;
	int32_t* tpst;
}VMstack;

typedef struct Thread_Message
{
	VMvalue* message;
	struct Thread_Message* next;
}ThreadMessage;

typedef struct
{
	unsigned int mark :1;
	int32_t VMid;
	int argc;
	char** argv;
	pthread_t tid;
	ByteCode* procs;
	VMprocess* curproc;
	VMprocess* mainproc;
	VMstack* stack;
	struct Channel* chan;
	struct Symbol_Table* table;
	struct VM_Heap* heap;
	struct Line_Number_Table* lnt;
	void (*callback)(void*);
}FakeVM;

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
	FakeVM** VMs;
}FakeVMlist;

#ifdef _WIN32
#include<windows.h>
typedef HMODULE DllHandle;
#else
typedef void* DllHandle;
#endif

typedef int (*DllFunc)(FakeVM*,pthread_rwlock_t*);

typedef struct VM_Dll
{
	int32_t refcount;
	DllHandle handle;
}VMDll;

typedef struct VM_Dlproc
{
	int32_t refcount;
	DllFunc func;
	VMDll* dll;
}VMDlproc;

typedef struct String_Match_Pattern
{
	int32_t num;
	char** parts;
	ByteCode* proc;
	struct Raw_Proc* procs;
	struct Line_Number_Table* lnt;
	struct String_Match_Pattern* prev;
	struct String_Match_Pattern* next;
}StringMatchPattern;

typedef struct VM_proc_status
{
	int32_t cp;
	VMcode* proc;
	VMenv* env;
}VMprocStatus;

typedef struct VM_Continuation
{
	int32_t refcount;
	int32_t size;
	VMstack* stack;
	VMprocStatus* status;
}VMcontinuation;

typedef struct Line_Number_Table
{
	int32_t size;
	struct Line_Number_Table_Id** list;
}LineNumberTable;

typedef struct Line_Number_Table_Id
{
	int32_t id;
	int32_t size;
	struct Line_Number_Table_Node** list;
}LineNumTabId;

typedef struct Line_Number_Table_Node
{
	int32_t fid;
	int32_t scp;
	int32_t cpc;
	int32_t line;
}LineNumTabNode;
#endif
