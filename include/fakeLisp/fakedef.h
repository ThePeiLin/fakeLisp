#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<pthread.h>
#include<ffi.h>
#include<setjmp.h>
#define FKL_THRESHOLD_SIZE 64
#define NUMOFBUILTINSYMBOL 54
#define MAX_STRING_SIZE 64
#define NUMOFBUILTINERRORTYPE 28
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
}VMptrTag;

typedef enum
{
	NATIVE_TYPE_TAG=0,
	ARRAY_TYPE_TAG,
	PTR_TYPE_TAG,
	STRUCT_TYPE_TAG,
	UNION_TYPE_TAG,
	FUNC_TYPE_TAG,
}DefTypeTag;

//typedef enum
//{
//	BYTE_1,
//	BYTE_2,
//	BYTE_4,
//	BYTE_8,
//}TypeSizeTag;

typedef uint32_t Sid_t;
typedef uint32_t TypeId_t;

typedef union VMTypeUnion
{
	void* all;
	struct VMNativeType* nt;
	struct VMArrayType* at;
	struct VMPtrType* pt;
	struct VMStructType* st;
	struct VMUnionType* ut;
	struct VMFuncType* ft;
}VMTypeUnion;

typedef struct VMDefTypesNode
{
	Sid_t name;
	TypeId_t type;
}VMDefTypesNode;

typedef struct VMDefTypes
{
	size_t num;
	VMDefTypesNode** u;
}VMDefTypes;

typedef enum{NIL=0,IN32,CHR,DBL,IN64,SYM,STR,BYTS,PRC,CONT,CHAN,FP,DLL,DLPROC,FLPROC,ERR,MEM,CHF,PAIR,ATM} ValueType;
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
	CANTDEREFERENCE,
	CANTGETELEM,
	INVALIDMEMBER,
	NOMEMBERTYPE,
	NONSCALARTYPE,
	INVALIDASSIGN,
	INVALIDACCESS,
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
}FklMem;

typedef struct
{
	ComStack* s;
}FklMemMenager;

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
	struct FklAstPair* outer;
	uint32_t curline;
	ValueType type;
	union
	{
		struct FklAstAtom* atom;
		struct FklAstPair* pair;
		void* all;
	}u;
}FklAstCptr;

typedef struct FklAstPair
{
	struct FklAstPair* prev;
	FklAstCptr car,cdr;
}FklAstPair;

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
	struct FklVM* v;
}RecvT;

typedef struct FklAstAtom
{
	FklAstPair* prev;
	ValueType type;
	union
	{
		char* str;
		char chr;
		int32_t in32;
		int64_t in64;
		double dbl;
		ByteString byts;
	} value;
}FklAstAtom;

typedef struct
{
	ErrorType state;
	FklAstCptr* place;
}ErrorState;

typedef struct PreDef
{
	char* symbol;
	FklAstCptr obj;//node or symbol or val
	struct PreDef* next;
}PreDef;

typedef struct PreEnv
{
	struct PreEnv* prev;
	PreDef* symbols;
	struct PreEnv* next;
}PreEnv;

typedef struct PreMacro
{
	FklAstCptr* pattern;
	ByteCodelnt* proc;
	VMDefTypes* deftypes;
	struct PreMacro* next;
	struct CompEnv* macroEnv;
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

typedef struct CompDef
{
	Sid_t id;
	struct CompDef* next;
}CompDef;

typedef struct CompEnv
{
	struct CompEnv* prev;
	char* prefix;
	const char** exp;
	uint32_t n;
	CompDef* head;
	PreMacro* macro;
	ByteCodelnt* proc;
	uint32_t refcount;
	struct KeyWord* keyWords;
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

typedef struct KeyWord
{
	char* word;
	struct KeyWord* next;
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


typedef struct VMMem
{
	TypeId_t type;
	uint8_t* mem;
}VMMem;

typedef struct VMvalue
{
	unsigned int mark :1;
	unsigned int type :6;
	union
	{
		struct VMpair* pair;
		double* dbl;
		int64_t* in64;
		char* str;
		struct VMMem* chf;
		struct VMByts* byts;
		struct VMproc* prc;
		DllHandle dll;
		struct VMDlproc* dlproc;
		struct VMFlproc* flproc;
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

typedef struct FklVM
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
}FklVM;

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
	FklVM** VMs;
}FklVMlist;

typedef void (*DllFunc)(FklVM*,pthread_rwlock_t*);

typedef struct VMDlproc
{
	DllFunc func;
	VMvalue* dll;
}VMDlproc;

typedef struct VMFlproc
{
	void* func;
	ffi_cif cif;
	ffi_type** atypes;
	TypeId_t type;
}VMFlproc;

typedef struct StringMatchPattern
{
	uint32_t num;
	char** parts;
	ValueType type;
	union{
		void (*fProc)(FklVM*);
		ByteCodelnt* bProc;
	}u;
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
	VMrunnable* state;
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
	uint32_t align;
	uint32_t size;
}VMNativeType;

typedef struct VMArrayType
{
	size_t num;
	size_t totalSize;
	uint32_t align;
	TypeId_t etype;
}VMArrayType;

typedef struct VMPtrType
{
	TypeId_t ptype;
}VMPtrType;

typedef struct VMStructType
{
	int64_t type;
	uint32_t num;
	size_t totalSize;
	uint32_t align;
	struct VMStructMember
	{
		Sid_t memberSymbol;
		TypeId_t type;
	}layout[];
}VMStructType;

typedef struct VMUnionType
{
	int64_t type;
	uint32_t num;
	size_t maxSize;
	uint32_t align;
	struct VMStructMember layout[];
}VMUnionType;

typedef struct VMFuncType
{
	TypeId_t rtype;
	uint32_t anum;
	TypeId_t atypes[];
}VMFuncType;
#endif
