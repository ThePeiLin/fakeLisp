#ifndef FKL_BYTECODE_H
#define FKL_BYTECODE_H

#include"symbol.h"
#include"base.h"
#include"opcode.h"
#include<stdint.h>
#include<stdio.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct
{
	FklOpcode op:8;
	union
	{
		uint8_t au;
		int8_t ai;
	};
	union
	{
		uint16_t bu;
		int16_t bi;
	};
}FklInstruction;

typedef struct
{
	FklHashTable ht;
	int64_t* base;
	uint32_t count;
	uint32_t size;
}FklConstI64Table;

typedef struct
{
	FklHashTable ht;
	double* base;
	uint32_t count;
	uint32_t size;
}FklConstF64Table;

typedef struct
{
	FklHashTable ht;
	FklString** base;
	uint32_t count;
	uint32_t size;
}FklConstStrTable;

typedef struct
{
	FklHashTable ht;
	FklBytevector** base;
	uint32_t count;
	uint32_t size;
}FklConstBvecTable;

typedef struct
{
	FklHashTable ht;
	FklBigInt** base;
	uint32_t count;
	uint32_t size;
}FklConstBiTable;

typedef struct
{
	FklConstI64Table ki64t;
	FklConstF64Table kf64t;
	FklConstStrTable kstrt;
	FklConstBvecTable kbvect;
	FklConstBiTable kbit;
}FklConstTable;

#define FKL_INSTRUCTION_STATIC_INIT {.op=FKL_OP_DUMMY,.au=0,.bu=0}

typedef struct
{
	uint64_t len;
	FklInstruction* code;
}FklByteCode;

typedef struct
{
	FklSid_t fid;
	uint64_t scp;
	uint32_t line;
	uint32_t scope;
}FklLineNumberTableItem;

typedef struct
{
	FklLineNumberTableItem* l;
	FklByteCode* bc;
	uint32_t ls;
}FklByteCodelnt;

void fklInitConstTable(FklConstTable* kt);
FklConstTable* fklCreateConstTable(void);

void fklUninitConstTable(FklConstTable* kt);
void fklDestroyConstTable(FklConstTable* kt);

uint32_t fklAddI64Const(FklConstTable* kt,int64_t k);
uint32_t fklAddF64Const(FklConstTable* kt,double k);
uint32_t fklAddStrConst(FklConstTable* kt,const FklString* k);
uint32_t fklAddBvecConst(FklConstTable* kt,const FklBytevector* k);
uint32_t fklAddBigIntConst(FklConstTable* kt,const FklBigInt* k);

int64_t fklGetI64ConstWithIdx(const FklConstTable* kt,uint32_t i);
double fklGetF64ConstWithIdx(const FklConstTable* kt,uint32_t i);
const FklString* fklGetStrConstWithIdx(const FklConstTable* kt,uint32_t i);
const FklBytevector* fklGetBvecConstWithIdx(const FklConstTable* kt,uint32_t i);
const FklBigInt* fklGetBiConstWithIdx(const FklConstTable* kt,uint32_t i);

void fklPrintConstTable(const FklConstTable* kt,FILE* fp);

FklByteCode* fklCreateByteCode(size_t);

void fklCodeConcat(FklByteCode*,const FklByteCode*);
void fklCodeReverseConcat(const FklByteCode*,FklByteCode*);

FklByteCode* fklCopyByteCode(const FklByteCode*);
FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt*);

void fklDestroyByteCode(FklByteCode*);
void fklPrintByteCode(const FklByteCode*,FILE*,FklSymbolTable*,FklConstTable*);

FklByteCodelnt* fklCreateByteCodelnt(FklByteCode* bc);

FklByteCodelnt* fklCreateSingleInsBclnt(FklInstruction ins
		,FklSid_t fid
		,uint32_t line
		,uint32_t scope);

void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp,const FklSymbolTable*,const FklConstTable*);
void fklDestroyByteCodelnt(FklByteCodelnt*);
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt*,uint64_t);
void fklCodeLntConcat(FklByteCodelnt*,FklByteCodelnt*);
void fklCodeLntReverseConcat(FklByteCodelnt*,FklByteCodelnt*);

void fklByteCodeLntPushBackIns(FklByteCodelnt* bcl
		,const FklInstruction* ins
		,FklSid_t fid
		,uint32_t line
		,uint32_t scope);

void fklByteCodeLntInsertFrontIns(const FklInstruction* ins
		,FklByteCodelnt* bcl
		,FklSid_t fid
		,uint32_t line
		,uint32_t scope);

void fklByteCodePushBack(FklByteCode* bc,FklInstruction ins);
void fklByteCodeInsertFront(FklInstruction,FklByteCode* bc);

void fklByteCodeLntInsertInsAt(FklByteCodelnt* bcl,FklInstruction ins,uint64_t idx);
FklInstruction fklByteCodeLntRemoveInsAt(FklByteCodelnt* bcl,uint64_t idx);

void fklInitLineNumTabNode(FklLineNumberTableItem*
		,FklSid_t fid
		,uint64_t scp
		,uint32_t line
		,uint32_t scope);

const FklLineNumberTableItem* fklFindLineNumTabNode(uint64_t cp,size_t ls,const FklLineNumberTableItem* l);

typedef struct
{
	int64_t ix;
	uint64_t ux;
	uint64_t uy;
}FklInstructionArg;

int8_t fklGetInsOpArg(const FklInstruction* ins,FklInstructionArg* arg);

int fklIsJmpIns(const FklInstruction* ins);
int fklIsPutLocIns(const FklInstruction* ins);
int fklIsPushProcIns(const FklInstruction* ins);
int fklIsPutVarRefIns(const FklInstruction* ins);

void fklScanAndSetTailCall(FklByteCode* bc);


void fklWriteConstTable(const FklConstTable* kt,FILE* fp);
void fklLoadConstTable(FILE* fp,FklConstTable* kt);

void fklWriteLineNumberTable(const FklLineNumberTableItem*,uint32_t num,FILE*);
void fklLoadLineNumberTable(FILE* fp,FklLineNumberTableItem** plist,uint32_t* pnum);

void fklWriteByteCode(const FklByteCode* bc,FILE* fp);
FklByteCode* fklLoadByteCode(FILE* fp);

void fklWriteByteCodelnt(const FklByteCodelnt* bcl,FILE* fp);
FklByteCodelnt* fklLoadByteCodelnt(FILE* fp);

#ifdef __cplusplus
}
#endif

#endif
