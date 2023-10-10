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
	FklOpcode op;
	uint32_t imm;
	union
	{
		FklString* str;
		FklBytevector* bvec;
		FklBigInt* bi;
		FklSid_t sid;

		uint32_t imm_u32;
		uint64_t imm_u64;
		char chr;
		int8_t imm_i8;
		int16_t imm_i16;
		int32_t imm_i32;
		int64_t imm_i64;
		double f64;
	};
}FklInstruction;

typedef struct
{
	uint64_t len;
	FklInstruction* code;
}FklByteCode;

typedef struct
{
	FklSid_t fid;
	uint64_t scp;
	uint64_t cpc;
	uint32_t line;
}FklLineNumberTableItem;

typedef struct
{
	FklLineNumberTableItem* l;
	FklByteCode* bc;
	uint32_t ls;
}FklByteCodelnt;

FklByteCode* fklCreateByteCode(size_t);

void fklCodeConcat(FklByteCode*,const FklByteCode*);
void fklCodeReverseConcat(const FklByteCode*,FklByteCode*);

FklByteCode* fklCopyByteCode(const FklByteCode*);
FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt*);

void fklDestroyByteCode(FklByteCode*);
void fklPrintByteCode(const FklByteCode*,FILE*,FklSymbolTable*);

FklByteCodelnt* fklCreateByteCodelnt(FklByteCode* bc);

FklByteCodelnt* fklCreateSingleInsBclnt(FklInstruction ins
		,FklSid_t fid
		,uint64_t line);

void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp,const FklSymbolTable*);
void fklDestroyByteCodelnt(FklByteCodelnt*);
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt*,uint64_t);
void fklCodeLntConcat(FklByteCodelnt*,FklByteCodelnt*);
void fklCodeLntReverseConcat(FklByteCodelnt*,FklByteCodelnt*);

void fklBytecodeLntPushFrontIns(FklByteCodelnt* bcl,const FklInstruction* ins,FklSid_t fid,uint64_t line);

void fklBytecodeLntPushBackIns(const FklInstruction* ins,FklByteCodelnt* bcl,FklSid_t fid,uint64_t line);

void fklByteCodeInsertFront(FklInstruction,FklByteCode* bc);

void fklInitLineNumTabNode(FklLineNumberTableItem*
		,FklSid_t fid
		,uint64_t scp
		,uint64_t cpc
		,uint64_t line);
FklLineNumberTableItem* fklCreateLineNumTabNode(FklSid_t fid,uint64_t scp,uint64_t cpc,uint64_t line);
FklLineNumberTableItem* fklCreateLineNumTabNodeWithFilename(const char* filename
		,uint64_t scp
		,uint64_t cpc
		,uint32_t line
		,FklSymbolTable* table);
const FklLineNumberTableItem* fklFindLineNumTabNode(uint64_t cp,size_t ls,const FklLineNumberTableItem* l);
void fklWriteLineNumberTable(const FklLineNumberTableItem*,uint32_t num,FILE*);
void fklDBG_printByteCode(FklInstruction* code,uint64_t s,uint64_t c,FILE*);

int16_t fklGetI16FromByteCode(uint8_t* code);
int32_t fklGetI32FromByteCode(uint8_t* code);
uint32_t fklGetU32FromByteCode(uint8_t* code);
int64_t fklGetI64FromByteCode(uint8_t* code);
uint64_t fklGetU64FromByteCode(uint8_t* code);
double fklGetF64FromByteCode(uint8_t* code);
FklSid_t fklGetSidFromByteCode(uint8_t* code);

void fklSetI8ToByteCode(uint8_t* code,int8_t i);
void fklSetI16ToByteCode(uint8_t* code,int16_t i);
void fklSetI32ToByteCode(uint8_t* code,int32_t i);
void fklSetU32ToByteCode(uint8_t* code,uint32_t i);
void fklSetI64ToByteCode(uint8_t* code,int64_t i);
void fklSetU64ToByteCode(uint8_t* code,uint64_t i);
void fklSetF64ToByteCode(uint8_t* code,double i);
void fklSetSidToByteCode(uint8_t* code,FklSid_t i);
void fklScanAndSetTailCall(FklByteCode* bc);

FklInstruction fklCreatePushI8Ins(int8_t);
FklInstruction fklCreatePushI16Ins(int16_t);
FklInstruction fklCreatePushI32Ins(int32_t);
FklInstruction fklCreatePushI64Ins(int64_t);
FklInstruction fklCreatePushBigIntIns(const FklBigInt*);
FklInstruction fklCreatePushSidIns(FklSid_t);
FklInstruction fklCreatePushCharIns(char);
FklInstruction fklCreatePushF64Ins(double a);
FklInstruction fklCreatePushNilIns(void);
FklInstruction fklCreatePushStrIns(const FklString* str);
FklInstruction fklCreatePushBvecIns(const FklBytevector* bvec);


void fklLoadLineNumberTable(FILE* fp,FklLineNumberTableItem** plist,uint32_t* pnum);

void fklWriteByteCode(const FklByteCode* bc,FILE* fp);
FklByteCode* fklLoadByteCode(FILE* fp);

void fklWriteByteCodelnt(const FklByteCodelnt* bcl,FILE* fp);
FklByteCodelnt* fklLoadByteCodelnt(FILE* fp);

#ifdef __cplusplus
}
#endif

#endif
