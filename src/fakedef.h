#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<pthread.h>
#define THRESHOLD_SIZE 64
#define NUMOFBUILTINSYMBOL 5
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
	INVALIDSYMBOL,
	LIBUNDEFINED,
	UNEXPECTEOF
}ErrorType;

typedef void (*GenDestructor)(void*);
typedef struct
{
	void** data;
	uint32_t num;
	long int top;
}ComStack;

typedef struct QueueNode
{
	void* data;
	struct QueueNode* next;
}QueueNode;

typedef struct
{
	QueueNode* head;
	QueueNode* tail;
}ComQueue;

typedef struct
{
	void (*destructor)(void*);
	void* block;
}FakeMem;

typedef struct
{
	ComStack* s;
}FakeMemMenager;

typedef struct
{
	uint32_t size;
	char* code;
}ByteCode;

typedef struct ByteCode_Lable
{
	char* label;
	int32_t place;
}ByteCodeLabel;

typedef struct Byte_Code_with_Line_Number_Node
{
	int32_t ls;
	struct Line_Number_Table_Node** l;
	ByteCode* bc;
}ByteCodelnt;

typedef struct
{
	uint32_t refcount;
	uint32_t size;
	uint8_t* str;
}ByteString;

typedef struct
{
	struct AST_pair* outer;
	uint32_t curline;
	ValueType type;
	union
	{
		struct AST_atom* atom;
		struct AST_pair* pair;
		void* all;
	}u;
}AST_cptr;

typedef struct AST_pair
{
	struct AST_pair* prev;
	AST_cptr car,cdr;
}AST_pair;

typedef struct Channel
{
	uint32_t max;
	uint32_t refcount;
	pthread_mutex_t lock;
	ComQueue* messages;
	ComQueue* sendq;
	ComQueue* recvq;
}Chanl;

typedef struct
{
	pthread_cond_t cond;
	struct VM_Value* m;
}SendT;

typedef struct
{
	pthread_cond_t cond;
	struct FakeVM* v;
}RecvT;

typedef struct VM_File
{
	uint32_t refcount;
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
	ErrorType status;
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
	ByteCodelnt* proc;
	struct Pre_Macro* next;
}PreMacro;

typedef struct Symbol_Table_Node
{
	char* symbol;
	int32_t id;
}SymTabNode;

typedef struct Symbol_Table
{
	int32_t num;
	SymTabNode** list;
	SymTabNode** idl;
}SymbolTable;

typedef struct Comp_Def
{
	uint32_t id;
	struct Comp_Def* next;
}CompDef;

typedef struct Comp_Env
{
	struct Comp_Env* prev;
	char* prefix;
	const char** exp;
	uint32_t n;
	CompDef* head;
	PreMacro* macro;
	ByteCodelnt* proc;
	struct Key_Word* keyWords;
}CompEnv;

typedef struct Interpreter
{
	char* filename;
	char* curDir;
	FILE* file;
	int curline;
	CompEnv* glob;
	struct Symbol_Table* table;
	struct Line_Number_Table* lnt;
	struct Interpreter* prev;
}Intpr;

typedef struct Key_Word
{
	char* word;
	struct Key_Word* next;
}KeyWord;

typedef struct
{
	uint32_t refcount;
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
	uint32_t id;
	VMvalue* value;
}VMenvNode;

typedef struct VM_Env
{
	struct VM_Env* prev;
	uint32_t refcount;
	uint32_t num;
	VMenvNode** list;
}VMenv;

typedef struct VM_Code
{
	uint32_t offset;
	int32_t refcount;
	VMenv* prevEnv;
	uint32_t size;
	char* code;
}VMcode;

typedef struct VM_Str
{
	uint32_t refcount;
	char* str;
}VMstr;

typedef struct VM_Process
{
	struct VM_Process* prev;
	VMenv* localenv;
	uint32_t cp;
	VMcode* code;
}VMprocess;

typedef struct
{
	uint32_t tp;
	uint32_t bp;
	uint32_t size;
	VMvalue** values;
	uint32_t tpsi;
	uint32_t tptp;
	uint32_t* tpst;
}VMstack;

typedef struct FakeVM
{
	unsigned int mark :1;
	int32_t VMid;
	int argc;
	char** argv;
	pthread_t tid;
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
	uint32_t num;
	uint32_t threshold;
	VMvalue* head;
}VMheap;

typedef struct
{
	uint32_t num;
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
	uint32_t refcount;
	DllHandle handle;
}VMDll;

typedef struct VM_Dlproc
{
	uint32_t refcount;
	DllFunc func;
	VMDll* dll;
}VMDlproc;

typedef struct String_Match_Pattern
{
	uint32_t num;
	char** parts;
	ByteCodelnt* proc;
	struct String_Match_Pattern* prev;
	struct String_Match_Pattern* next;
}StringMatchPattern;

typedef struct VM_proc_status
{
	uint32_t cp;
	VMcode* proc;
	VMenv* env;
}VMprocStatus;

typedef struct VM_Continuation
{
	uint32_t refcount;
	uint32_t num;
	VMstack* stack;
	VMprocStatus* status;
}VMcontinuation;

typedef struct Line_Number_Table
{
	uint32_t num;
	struct Line_Number_Table_Node** list;
}LineNumberTable;

typedef struct Line_Number_Table_Node
{
	uint32_t fid;
	uint32_t scp;
	uint32_t cpc;
	uint32_t line;
}LineNumTabNode;
#endif
