#ifndef FKL_BYTECODE_H
#define FKL_BYTECODE_H
#include"symbol.h"
#include"basicADT.h"
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

//typedef struct FklByteCodeLabel
//{
//	char* label;
//	int32_t place;
//}FklByteCodeLabel;

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
FklByteCode* fklNewByteCode(unsigned int);
void fklCodeCat(FklByteCode*,const FklByteCode*);
void fklReCodeCat(const FklByteCode*,FklByteCode*);
FklByteCode* fklCopyByteCode(const FklByteCode*);
FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt*);
void fklFreeByteCode(FklByteCode*);
void fklPrintByteCode(const FklByteCode*,FILE*);

FklByteCodelnt* fklNewByteCodelnt(FklByteCode* bc);
void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp);
void fklFreeByteCodelnt(FklByteCodelnt*);
void fklFreeByteCodeAndLnt(FklByteCodelnt*);
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt*,uint64_t);
void fklCodeLntCat(FklByteCodelnt*,FklByteCodelnt*);
void fklCodelntCopyCat(FklByteCodelnt*,const FklByteCodelnt*);
void fklReCodeLntCat(FklByteCodelnt*,FklByteCodelnt*);

//FklByteCodeLabel* fklNewByteCodeLable(int32_t,const char*);
//FklByteCodeLabel* fklFindByteCodeLabel(const char*,FklPtrStack*);
//void fklFreeByteCodeLabel(FklByteCodeLabel*);

FklLineNumberTable* fklNewLineNumTable();
FklLineNumTabNode* fklNewLineNumTabNode(FklSid_t fid,uint64_t scp,uint64_t cpc,uint32_t line);
FklLineNumTabNode* fklNewLineNumTabNodeWithFilename(const char* filename
		,uint64_t scp
		,uint64_t cpc
		,uint32_t line);
FklLineNumTabNode* fklFindLineNumTabNode(uint64_t cp,FklLineNumberTable*);
void fklFreeLineNumTabNode(FklLineNumTabNode*);
void fklFreeLineNumberTable(FklLineNumberTable*);
void fklLntCat(FklLineNumberTable* t,uint32_t bs,FklLineNumTabNode** l2,uint32_t s2);
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
void fklScanAndSetTailInvoke(FklByteCode* bc);

FklByteCode* fklNewPushI32ByteCode(int32_t);
FklByteCode* fklNewPushI64ByteCode(int64_t);
FklByteCode* fklNewPushBigIntByteCode(const FklBigInt*);
FklByteCode* fklNewPushSidByteCode(FklSid_t);
FklByteCode* fklNewPushCharByteCode(char);
FklByteCode* fklNewPushF64ByteCode(double a);
FklByteCode* fklNewPushNilByteCode(void);
FklByteCode* fklNewPushStrByteCode(const FklString* str);
#ifdef __cplusplus
}
#endif

#endif
