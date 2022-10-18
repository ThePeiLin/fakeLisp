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
	uint32_t ls;
	struct FklLineNumberTableNode** l;
	FklByteCode* bc;
}FklByteCodelnt;

typedef struct FklLineNumberTable
{
	uint32_t num;
	struct FklLineNumberTableNode** list;
}FklLineNumberTable;

typedef struct FklLineNumberTableNode
{
	FklSid_t fid;
	uint64_t scp;
	uint64_t cpc;
	uint32_t line;
}FklLineNumTabNode;

FklByteCode* fklCreateByteCode(size_t);
FklByteCode* fklCreateByteCodeAndInit(size_t,const uint8_t*);
void fklCodeCat(FklByteCode*,const FklByteCode*);
void fklReCodeCat(const FklByteCode*,FklByteCode*);
FklByteCode* fklCopyByteCode(const FklByteCode*);
FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt*);
void fklDestroyByteCode(FklByteCode*);
void fklPrintByteCode(const FklByteCode*,FILE*,FklSymbolTable*);

FklByteCodelnt* fklCreateByteCodelnt(FklByteCode* bc);
void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp,FklSymbolTable*);
void fklDestroyByteCodelnt(FklByteCodelnt*);
void fklDestroyByteCodeAndLnt(FklByteCodelnt*);
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt*,uint64_t);
void fklCodeLntCat(FklByteCodelnt*,FklByteCodelnt*);
void fklCodelntCopyCat(FklByteCodelnt*,const FklByteCodelnt*);
void fklReCodeLntCat(FklByteCodelnt*,FklByteCodelnt*);

FklLineNumberTable* fklCreateLineNumTable();
FklLineNumTabNode* fklCreateLineNumTabNode(FklSid_t fid,uint64_t scp,uint64_t cpc,uint64_t line);
FklLineNumTabNode* fklCreateLineNumTabNodeWithFilename(const char* filename
		,uint64_t scp
		,uint64_t cpc
		,uint32_t line);
FklLineNumTabNode* fklFindLineNumTabNode(uint64_t cp,FklLineNumberTable*);
void fklDestroyLineNumTabNode(FklLineNumTabNode*);
void fklDestroyLineNumberTable(FklLineNumberTable*);
void fklLntCat(FklLineNumberTable* t,uint64_t bs,FklLineNumTabNode** l2,uint64_t s2);
void fklWriteLineNumberTable(FklLineNumberTable*,FILE*);
void fklDBG_printByteCode(uint8_t* code,uint64_t s,uint64_t c,FILE*);

int32_t fklGetI32FromByteCode(uint8_t* code);
uint32_t fklGetU32FromByteCode(uint8_t* code);
int64_t fklGetI64FromByteCode(uint8_t* code);
uint64_t fklGetU64FromByteCode(uint8_t* code);
double fklGetF64FromByteCode(uint8_t* code);
FklSid_t fklGetSidFromByteCode(uint8_t* code);

void fklSetI32ToByteCode(uint8_t* code,int32_t i);
void fklSetU32ToByteCode(uint8_t* code,uint32_t i);
void fklSetI64ToByteCode(uint8_t* code,int64_t i);
void fklSetU64ToByteCode(uint8_t* code,uint64_t i);
void fklSetF64ToByteCode(uint8_t* code,double i);
void fklSetSidToByteCode(uint8_t* code,FklSid_t i);
void fklScanAndSetTailCall(FklByteCode* bc);

FklByteCode* fklCreatePushI32ByteCode(int32_t);
FklByteCode* fklCreatePushI64ByteCode(int64_t);
FklByteCode* fklCreatePushBigIntByteCode(const FklBigInt*);
FklByteCode* fklCreatePushSidByteCode(FklSid_t);
FklByteCode* fklCreatePushCharByteCode(char);
FklByteCode* fklCreatePushF64ByteCode(double a);
FklByteCode* fklCreatePushNilByteCode(void);
FklByteCode* fklCreatePushStrByteCode(const FklString* str);
FklByteCode* fklCreatePushBvecByteCode(const FklBytevector* bvec);

void fklDestroyLineNumTabNodeArray(FklLineNumTabNode** a,size_t num);

#ifdef __cplusplus
}
#endif

#endif
