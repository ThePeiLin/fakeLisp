#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<pthread.h>
#include<ffi.h>
#include<setjmp.h>

typedef struct FklVMvalue* FklVMptr;
typedef struct FklVMStructMember FklVMStructMember;
typedef enum
{
	PTR_TAG=0,
	NIL_TAG,
	FKL_I32_TAG,
	SYM_TAG,
	CHR_TAG,
	REF_TAG,
}FklVMptrTag;

typedef enum
{
	NATIVE_TYPE_TAG=0,
	ARRAY_TYPE_TAG,
	PTR_TYPE_TAG,
	STRUCT_TYPE_TAG,
	UNION_TYPE_TAG,
	FUNC_TYPE_TAG,
}FklDefTypeTag;

//typedef enum
//{
//	BYTE_1,
//	BYTE_2,
//	BYTE_4,
//	BYTE_8,
//}TypeSizeTag;

typedef uint32_t FklSid_t;
typedef uint32_t FklTypeId_t;

typedef union FklVMTypeUnion
{
	void* all;
	struct FklVMNativeType* nt;
	struct FklVMArrayType* at;
	struct FklVMPtrType* pt;
	struct FklVMStructType* st;
	struct FklVMUnionType* ut;
	struct FklVMFuncType* ft;
}FklVMTypeUnion;

typedef struct FklVMDefTypesNode
{
	FklSid_t name;
	FklTypeId_t type;
}FklVMDefTypesNode;

typedef struct FklVMDefTypes
{
	size_t num;
	FklVMDefTypesNode** u;
}FklVMDefTypes;

typedef enum{NIL=0,
	FKL_I32,
	CHR,
	DBL,
	IN64,
	SYM,
	STR,
	BYTS,
	PRC,
	CONT,
	CHAN,
	FP,
	DLL,
	DLPROC,
	FLPROC,
	ERR,
	MEM,
	CHF,
	PAIR,
	ATM,
}FklValueType;
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
}FklErrorType;

#ifdef _WIN32
#include<windows.h>
typedef HMODULE DllHandle;
#else
typedef void* DllHandle;
#endif

typedef void (*FklGenDestructor)(void*);
typedef struct
{
	void** data;
	uint32_t num;
	long int top;
}FklComStack;

typedef struct FklQueueNode
{
	void* data;
	struct FklQueueNode* next;
}FklQueueNode;

typedef struct
{
	FklQueueNode* head;
	FklQueueNode* tail;
}FklComQueue;

typedef struct
{
	void (*destructor)(void*);
	void* block;
}FklMem;

typedef struct
{
	FklComStack* s;
}FklMemMenager;

typedef struct
{
	uint32_t size;
	uint8_t* code;
}FklByteCode;

typedef struct FklByteCodeLabel
{
	char* label;
	int32_t place;
}FklByteCodeLabel;

typedef struct
{
	int32_t ls;
	struct FklLineNumberTableNode** l;
	FklByteCode* bc;
}FklByteCodelnt;

typedef struct
{
	uint32_t size;
	uint8_t* str;
}FklByteString;

typedef struct
{
	struct FklAstPair* outer;
	uint32_t curline;
	FklValueType type;
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

typedef struct FklVMChanl
{
	uint32_t max;
	pthread_mutex_t lock;
	FklComQueue* messages;
	FklComQueue* sendq;
	FklComQueue* recvq;
}FklVMChanl;

typedef struct
{
	pthread_cond_t cond;
	struct FklVMvalue* m;
}FklSendT;

typedef struct
{
	pthread_cond_t cond;
	struct FklVM* v;
}FklRecvT;

typedef struct FklAstAtom
{
	FklAstPair* prev;
	FklValueType type;
	union
	{
		char* str;
		char chr;
		int32_t i32;
		int64_t in64;
		double dbl;
		FklByteString byts;
	} value;
}FklAstAtom;

typedef struct
{
	FklErrorType state;
	FklAstCptr* place;
}FklErrorState;

typedef struct FklPreDef
{
	char* symbol;
	FklAstCptr obj;//node or symbol or val
	struct FklPreDef* next;
}FklPreDef;

typedef struct FklPreEnv
{
	struct FklPreEnv* prev;
	FklPreDef* symbols;
	struct FklPreEnv* next;
}FklPreEnv;

typedef struct FklPreMacro
{
	FklAstCptr* pattern;
	FklByteCodelnt* proc;
	FklVMDefTypes* deftypes;
	struct FklPreMacro* next;
	struct FklCompEnv* macroEnv;
}FklPreMacro;

typedef struct FklSymTabNode
{
	char* symbol;
	int32_t id;
}FklSymTabNode;

typedef struct FklSymbolTable
{
	int32_t num;
	FklSymTabNode** list;
	FklSymTabNode** idl;
}FklSymbolTable;

typedef struct FklCompDef
{
	FklSid_t id;
	struct FklCompDef* next;
}FklCompDef;

typedef struct FklCompEnv
{
	struct FklCompEnv* prev;
	char* prefix;
	const char** exp;
	uint32_t n;
	FklCompDef* head;
	FklPreMacro* macro;
	FklByteCodelnt* proc;
	uint32_t refcount;
	struct FklKeyWord* keyWords;
}FklCompEnv;

typedef struct FklInterpreter
{
	char* filename;
	char* curDir;
	FILE* file;
	int curline;
	FklCompEnv* glob;
	struct LineNumberTable* lnt;
	struct FklInterpreter* prev;
	FklVMDefTypes* deftypes;
}FklIntpr;

typedef struct FklKeyWord
{
	char* word;
	struct FklKeyWord* next;
}FklKeyWord;

typedef struct FklVMpair
{
	struct FklVMvalue* car;
	struct FklVMvalue* cdr;
}FklVMpair;

typedef struct FklVMByts
{
	size_t size;
	uint8_t str[];
}FklVMByts;


typedef struct FklVMMem
{
	FklTypeId_t type;
	uint8_t* mem;
}FklVMMem;

typedef struct FklVMvalue
{
	unsigned int mark :1;
	unsigned int type :6;
	union
	{
		struct FklVMpair* pair;
		double* dbl;
		int64_t* in64;
		char* str;
		struct FklVMMem* chf;
		struct FklVMByts* byts;
		struct FklVMproc* prc;
		DllHandle dll;
		struct FklVMDlproc* dlproc;
		struct FklVMFlproc* flproc;
		struct FklVMContinuation* cont;
		FILE* fp;
		struct FklVMChanl* chan;
		struct FklVMerror* err;
	}u;
	struct FklVMvalue* prev;
	struct FklVMvalue* next;
}FklVMvalue;

typedef struct FklVMenvNode
{
	uint32_t id;
	FklVMvalue* value;
}FklVMenvNode;

typedef struct FklVMenv
{
	struct FklVMenv* prev;
	uint32_t refcount;
	uint32_t num;
	FklVMenvNode** list;
}FklVMenv;

typedef struct FklVMproc
{
	uint32_t scp;
	uint32_t cpc;
	FklVMenv* prevEnv;
}FklVMproc;

typedef struct FklVMrunnable
{
	unsigned int mark :1;
	FklVMenv* localenv;
	uint32_t scp;
	uint32_t cp;
	uint32_t cpc;
}FklVMrunnable;

typedef struct
{
	uint32_t tp;
	uint32_t bp;
	uint32_t size;
	FklVMvalue** values;
	uint32_t tpsi;
	uint32_t tptp;
	uint32_t* tpst;
}FklVMstack;

typedef struct FklVM
{
	unsigned int mark :1;
	int32_t VMid;
	int argc;
	char** argv;
	pthread_t tid;
	uint8_t* code;
	uint32_t size;
	FklComStack* rstack;
	FklComStack* tstack;
	FklVMstack* stack;
	struct FklVMvalue* chan;
	struct FklVMHeap* heap;
	struct LineNumberTable* lnt;
	void (*callback)(void*);
	jmp_buf buf;
}FklVM;

typedef struct FklVMHeap
{
	pthread_mutex_t lock;
	uint32_t num;
	uint32_t threshold;
	FklVMvalue* head;
}VMheap;

typedef struct
{
	uint32_t num;
	FklVM** VMs;
}FklVMlist;

typedef void (*FklDllFunc)(FklVM*,pthread_rwlock_t*);

typedef struct FklVMDlproc
{
	FklDllFunc func;
	FklVMvalue* dll;
}FklVMDlproc;

typedef struct FklVMFlproc
{
	void* func;
	ffi_cif cif;
	ffi_type** atypes;
	FklTypeId_t type;
}FklVMFlproc;

typedef struct FklStringMatchPattern
{
	uint32_t num;
	char** parts;
	FklValueType type;
	union{
		void (*fProc)(FklVM*);
		FklByteCodelnt* bProc;
	}u;
	struct FklStringMatchPattern* prev;
	struct FklStringMatchPattern* next;
}FklStringMatchPattern;

typedef struct FklVMerror
{
	FklSid_t type;
	char* who;
	char* message;
}FklVMerror;

typedef struct FklVMContinuation
{
	uint32_t num;
	uint32_t tnum;
	FklVMstack* stack;
	FklVMrunnable* state;
	struct FklVMTryBlock* tb;
}VMcontinuation;

typedef struct LineNumberTable
{
	uint32_t num;
	struct FklLineNumberTableNode** list;
}LineNumberTable;

typedef struct FklLineNumberTableNode
{
	uint32_t fid;
	uint32_t scp;
	uint32_t cpc;
	uint32_t line;
}LineNumTabNode;

typedef struct FklVMTryBlock
{
	FklSid_t sid;
	FklComStack* hstack;
	long int rtp;
	uint32_t tp;
}FklVMTryBlock;

typedef struct FklVMerrorHandler
{
	FklSid_t type;
	FklVMproc proc;
}FklVMerrorHandler;

typedef struct FklVMNativeType
{
	FklSid_t type;
	uint32_t align;
	uint32_t size;
}FklVMNativeType;

typedef struct FklVMArrayType
{
	size_t num;
	size_t totalSize;
	uint32_t align;
	FklTypeId_t etype;
}FklVMArrayType;

typedef struct FklVMPtrType
{
	FklTypeId_t ptype;
}FklVMPtrType;

typedef struct FklVMStructType
{
	int64_t type;
	uint32_t num;
	size_t totalSize;
	uint32_t align;
	struct FklVMStructMember
	{
		FklSid_t memberSymbol;
		FklTypeId_t type;
	}layout[];
}FklVMStructType;

typedef struct FklVMUnionType
{
	int64_t type;
	uint32_t num;
	size_t maxSize;
	uint32_t align;
	struct FklVMStructMember layout[];
}FklVMUnionType;

typedef struct FklVMFuncType
{
	FklTypeId_t rtype;
	uint32_t anum;
	FklTypeId_t atypes[];
}FklVMFuncType;
#endif
