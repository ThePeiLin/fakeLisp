#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<pthread.h>
#include<setjmp.h>
#define THRESHOLD_SIZE 64
#define NUMOFBUILTINSYMBOL 51
#define MAX_STRING_SIZE 64
#define NUMOFBUILTINERRORTYPE 21
#define STATIC_SYMBOL_INIT {0,NULL,NULL}
#define UNUSEDBITNUM 3
#define PTR_MASK ((intptr_t)0xFFFFFFFFFFFFFFF8)
#define TAG_MASK ((intptr_t)0x7)

typedef struct VMvalue* VMptr;
typedef struct VMStructMember VMStructMember;
typedef enum
{
	PTR_TAG=0,
	NIL_TAG,
	IN32_TAG,
	SYM_TAG,
	CHR_TAG,
	REF_TAG,
	CHF_TAG,
}VMptrTag;

typedef enum
{
	NATIVE_TYPE_TAG=0,
	ARRAY_TYPE_TAG,
	PTR_TYPE_TAG,
	STRUCT_TYPE_TAG,
	UNION_TYPE_TAG,
	BITS_TYPE_TAG,
	FUNC_TYPE_TAG,
}DefTypeTag;

typedef uint32_t Sid_t;

typedef union VMTypeUnion
{
	void* all;
	struct VMNativeType* nt;
	struct VMArrayType* at;
	struct VMPtrType* pt;
	struct VMStructType* st;
}VMTypeUnion;

typedef struct VMDefTypesNode
{
	Sid_t name;
	VMTypeUnion type;
}VMDefTypesNode;
typedef struct VMDefTypes
{
	size_t num;
	VMDefTypesNode** u;
}VMDefTypes;

typedef enum{NIL=0,IN32,CHR,DBL,SYM,STR,BYTS,PRC,CONT,CHAN,FP,DLL,DLPROC,ERR,PAIR,ATM} ValueType;
typedef enum
{
	SYMUNDEFINE=1,
	SYNTAXERROR,
	INVALIDEXPR,
	INVALIDTYPEDEF,
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
	UNEXPECTEOF,
	DIVZERROERROR,
	FILEFAILURE,
}ErrorType;

#ifdef _WIN32
#include<windows.h>
typedef HMODULE DllHandle;
#else
typedef void* DllHandle;
#endif

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
	uint8_t* code;
}ByteCode;

typedef struct ByteCodeLabel
{
	char* label;
	int32_t place;
}ByteCodeLabel;

typedef struct
{
	int32_t ls;
	struct LineNumberTableNode** l;
	ByteCode* bc;
}ByteCodelnt;

typedef struct
{
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

typedef struct VMChanl
{
	uint32_t max;
	pthread_mutex_t lock;
	ComQueue* messages;
	ComQueue* sendq;
	ComQueue* recvq;
}VMChanl;

typedef struct
{
	pthread_cond_t cond;
	struct VMvalue* m;
}SendT;

typedef struct
{
	pthread_cond_t cond;
	struct FakeVM* v;
}RecvT;

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

typedef struct SymTabNode
{
	char* symbol;
	int32_t id;
}SymTabNode;

typedef struct SymbolTable
{
	int32_t num;
	SymTabNode** list;
	SymTabNode** idl;
}SymbolTable;

typedef struct Comp_Def
{
	Sid_t id;
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
	struct LineNumberTable* lnt;
	struct Interpreter* prev;
	VMDefTypes* deftypes;
}Intpr;

typedef struct Key_Word
{
	char* word;
	struct Key_Word* next;
}KeyWord;

typedef struct VMpair
{
	struct VMvalue* car;
	struct VMvalue* cdr;
}VMpair;

typedef struct VMByts
{
	size_t size;
	uint8_t str[];
}VMByts;

typedef struct VMMemref
{
	struct VMvalue* from;
	uint8_t* obj;
	size_t size;
}VMMemref;

typedef struct VMvalue
{
	unsigned int mark :1;
	unsigned int type :6;
	union
	{
		struct VMpair* pair;
		double* dbl;
		char* str;
		struct VMByts* byts;
		struct VMproc* prc;
		DllHandle dll;
		struct VMDlproc* dlproc;
		struct VMContinuation* cont;
		FILE* fp;
		struct VMChanl* chan;
		struct VMerror* err;
	}u;
	struct VMvalue* prev;
	struct VMvalue* next;
}VMvalue;

typedef struct VMenvNode
{
	uint32_t id;
	VMvalue* value;
}VMenvNode;

typedef struct VMenv
{
	struct VMenv* prev;
	uint32_t refcount;
	uint32_t num;
	VMenvNode** list;
}VMenv;

typedef struct VMproc
{
	uint32_t scp;
	uint32_t cpc;
	VMenv* prevEnv;
}VMproc;

typedef struct VMrunnable
{
	unsigned int mark :1;
	VMenv* localenv;
	uint32_t scp;
	uint32_t cp;
	uint32_t cpc;
}VMrunnable;

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
	uint8_t* code;
	uint32_t size;
	ComStack* rstack;
	ComStack* tstack;
	VMstack* stack;
	struct VMvalue* chan;
	struct VMHeap* heap;
	struct LineNumberTable* lnt;
	void (*callback)(void*);
	jmp_buf buf;
	VMDefTypes* deftypes;
}FakeVM;

typedef struct VMHeap
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

typedef void (*DllFunc)(FakeVM*,pthread_rwlock_t*);

typedef struct VMDlproc
{
	DllFunc func;
	VMvalue* dll;
}VMDlproc;

typedef struct StringMatchPattern
{
	uint32_t num;
	char** parts;
	ByteCodelnt* proc;
	struct StringMatchPattern* prev;
	struct StringMatchPattern* next;
}StringMatchPattern;

typedef struct VMerror
{
	Sid_t type;
	char* who;
	char* message;
}VMerror;

typedef struct VMContinuation
{
	uint32_t num;
	uint32_t tnum;
	VMstack* stack;
	VMrunnable* status;
	struct VMTryBlock* tb;
}VMcontinuation;

typedef struct LineNumberTable
{
	uint32_t num;
	struct LineNumberTableNode** list;
}LineNumberTable;

typedef struct LineNumberTableNode
{
	uint32_t fid;
	uint32_t scp;
	uint32_t cpc;
	uint32_t line;
}LineNumTabNode;

typedef struct VMTryBlock
{
	Sid_t sid;
	ComStack* hstack;
	long int rtp;
	uint32_t tp;
}VMTryBlock;

typedef struct VMerrorHandler
{
	Sid_t type;
	VMproc proc;
}VMerrorHandler;

typedef struct VMNativeType
{
	Sid_t type;
	uint32_t size;
}VMNativeType;

typedef struct VMArrayType
{
	size_t num;
	size_t totalSize;
	VMTypeUnion etype;
}VMArrayType;

typedef struct VMPtrType
{
	VMTypeUnion ptype;
}VMPtrType;

typedef struct VMStructType
{
	Sid_t type;
	uint32_t num;
	size_t totalSize;
	struct VMStructMember
	{
		Sid_t memberSymbol;
		VMTypeUnion type;
	}layout[];
}VMStructType;
#endif
