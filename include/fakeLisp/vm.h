#ifndef FKL_VM_H
#define FKL_VM_H

#include"basicADT.h"
#include"deftype.h"
#include<stdio.h>
#include<stdint.h>
#include<pthread.h>
#include<ffi.h>
#include<setjmp.h>

typedef struct FklVMvalue* FklVMptr;
typedef enum
{
	FKL_PTR_TAG=0,
	FKL_NIL_TAG,
	FKL_I32_TAG,
	FKL_SYM_TAG,
	FKL_CHR_TAG,
	FKL_REF_TAG,
}FklVMptrTag;

#ifdef _WIN32
#include<windows.h>
typedef HMODULE FklVMdllHandle;
#else
typedef void* FklVMdllHandle;
#endif

typedef struct FklVMchanl
{
	uint32_t max;
	pthread_mutex_t lock;
	FklPtrQueue* messages;
	FklPtrQueue* sendq;
	FklPtrQueue* recvq;
}FklVMchanl;

typedef struct
{
	pthread_cond_t cond;
	struct FklVMvalue* m;
}FklVMsend;

typedef struct
{
	pthread_cond_t cond;
	struct FklVM* v;
}FklVMrecv;

typedef struct FklVMpair
{
	struct FklVMvalue* car;
	struct FklVMvalue* cdr;
}FklVMpair;

typedef struct FklVMbyts
{
	size_t size;
	uint8_t str[];
}FklVMbyts;

typedef struct FklVMmem
{
	FklTypeId_t type;
	uint8_t* mem;
}FklVMmem;

typedef struct FklVMvalue
{
	unsigned int mark :1;
	unsigned int type :6;
	union
	{
		struct FklVMpair* pair;
		double dbl;
		int64_t i64;
		char* str;
		struct FklVMmem* chf;
		struct FklVMbyts* byts;
		struct FklVMproc* prc;
		FklVMdllHandle dll;
		struct FklVMdlproc* dlproc;
		struct FklVMflproc* flproc;
		struct FklVMcontinuation* cont;
		FILE* fp;
		struct FklVMchanl* chan;
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
	FklSid_t sid;
	FklVMenv* prevEnv;
}FklVMproc;

typedef struct FklVMrunnable
{
	unsigned int mark :1;
	FklVMenv* localenv;
	uint32_t scp;
	uint32_t cp;
	uint32_t cpc;
	FklSid_t sid;
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
	FklPtrStack* rstack;
	FklPtrStack* tstack;
	FklVMstack* stack;
	struct FklVMvalue* chan;
	struct FklVMheap* heap;
	struct LineNumberTable* lnt;
	void (*callback)(void*);
	jmp_buf buf;
}FklVM;

typedef struct FklVMheap
{
	pthread_mutex_t lock;
	uint32_t num;
	uint32_t threshold;
	FklVMvalue* head;
}FklVMheap;

typedef struct
{
	uint32_t num;
	FklVM** VMs;
}FklVMlist;

typedef void (*FklDllFunc)(FklVM*,pthread_rwlock_t*);

typedef struct FklVMdlproc
{
	FklDllFunc func;
	FklVMvalue* dll;
	FklSid_t sid;
}Fkldlproc;

typedef struct FklVMflproc
{
	void* func;
	ffi_cif cif;
	ffi_type** atypes;
	FklTypeId_t type;
	FklSid_t sid;
}FklVMflproc;
typedef struct FklVMerror
{
	FklSid_t type;
	char* who;
	char* message;
}FklVMerror;

typedef struct FklVMcontinuation
{
	uint32_t num;
	uint32_t tnum;
	FklVMstack* stack;
	FklVMrunnable* state;
	struct FklVMtryBlock* tb;
}VMcontinuation;

typedef struct FklVMtryBlock
{
	FklSid_t sid;
	FklPtrStack* hstack;
	long int rtp;
	uint32_t tp;
}FklVMtryBlock;

typedef struct FklVMerrorHandler
{
	FklSid_t type;
	FklVMproc proc;
}FklVMerrorHandler;

typedef struct FklVMptrType
{
	FklTypeId_t ptype;
}FklVMptrType;

#endif
