#ifndef FKL_BYTECODE_H
#define FKL_BYTECODE_H

#include"symbol.h"
#include"base.h"
#include<stdint.h>
#include<stdio.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct
{
	uint64_t size;
	uint8_t* code;
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
	size_t ls;
	FklLineNumberTableItem* l;
	FklByteCode* bc;
}FklByteCodelnt;

FklByteCode* fklCreateByteCode(size_t);
FklByteCode* fklCreateByteCodeAndInit(size_t,const uint8_t*);
void fklCodeConcat(FklByteCode*,const FklByteCode*);
void fklCodeReverseConcat(const FklByteCode*,FklByteCode*);
FklByteCode* fklCopyByteCode(const FklByteCode*);
FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt*);
void fklDestroyByteCode(FklByteCode*);
void fklPrintByteCode(const FklByteCode*,FILE*,FklSymbolTable*);

FklByteCodelnt* fklCreateByteCodelnt(FklByteCode* bc);
void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp,FklSymbolTable*);
void fklDestroyByteCodelnt(FklByteCodelnt*);
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt*,uint64_t);
void fklCodeLntConcat(FklByteCodelnt*,FklByteCodelnt*);
void fklCodeLntReverseConcat(FklByteCodelnt*,FklByteCodelnt*);

void fklBclBcAppendToBcl(FklByteCodelnt* bcl
		,const FklByteCode* bc
		,FklSid_t fid
		,uint64_t line);

void fklBcBclAppendToBcl(const FklByteCode* bc
		,FklByteCodelnt* bcl
		,FklSid_t fid
		,uint64_t line);

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
FklLineNumberTableItem* fklFindLineNumTabNode(uint64_t cp,size_t ls,FklLineNumberTableItem* l);
void fklWriteLineNumberTable(FklLineNumberTableItem*,size_t num,FILE*);
void fklDBG_printByteCode(uint8_t* code,uint64_t s,uint64_t c,FILE*);

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

FklByteCode* fklCreatePushI8ByteCode(int8_t);
FklByteCode* fklCreatePushI16ByteCode(int16_t);
FklByteCode* fklCreatePushI32ByteCode(int32_t);
FklByteCode* fklCreatePushI64ByteCode(int64_t);
FklByteCode* fklCreatePushBigIntByteCode(const FklBigInt*);
FklByteCode* fklCreatePushSidByteCode(FklSid_t);
FklByteCode* fklCreatePushCharByteCode(char);
FklByteCode* fklCreatePushF64ByteCode(double a);
FklByteCode* fklCreatePushNilByteCode(void);
FklByteCode* fklCreatePushStrByteCode(const FklString* str);
FklByteCode* fklCreatePushBvecByteCode(const FklBytevector* bvec);

void fklLoadLineNumberTable(FILE* fp,FklLineNumberTableItem** plist,size_t* pnum);

#ifdef __cplusplus
}
#endif

#endif
