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

typedef struct FklLineNumberTable
{
	uint32_t num;
	struct FklLineNumberTableNode** list;
}FklLineNumberTable;

typedef struct FklLineNumberTableNode
{
	FklSid_t fid;
	uint32_t scp;
	uint32_t cpc;
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
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt*,int32_t);
void fklCodeLntCat(FklByteCodelnt*,FklByteCodelnt*);
void fklCodelntCopyCat(FklByteCodelnt*,const FklByteCodelnt*);
void fklReCodeLntCat(FklByteCodelnt*,FklByteCodelnt*);

FklByteCodeLabel* fklNewByteCodeLable(int32_t,const char*);
FklByteCodeLabel* fklFindByteCodeLabel(const char*,FklPtrStack*);
void fklFreeByteCodeLabel(FklByteCodeLabel*);

FklLineNumberTable* fklNewLineNumTable();
FklLineNumTabNode* fklNewLineNumTabNode(FklSid_t fid,int32_t scp,int32_t cpc,int32_t line);
FklLineNumTabNode* fklFindLineNumTabNode(uint32_t cp,FklLineNumberTable*);
void fklFreeLineNumTabNode(FklLineNumTabNode*);
void fklFreeLineNumberTable(FklLineNumberTable*);
void fklLntCat(FklLineNumberTable* t,int32_t bs,FklLineNumTabNode** l2,int32_t s2);
void fklWriteLineNumberTable(FklLineNumberTable*,FILE*);
void fklDBG_printByteCode(uint8_t* code,uint32_t s,uint32_t c,FILE*);

#ifdef __cplusplus
}
#endif

#endif
