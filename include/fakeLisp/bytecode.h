#ifndef FKL_BYTECODE_H
#define FKL_BYTECODE_H
#include"symbol.h"
#include"basicADT.h"
#include<stdint.h>
#include<stdio.h>
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

typedef struct LineNumberTable
{
	uint32_t num;
	struct FklLineNumberTableNode** list;
}LineNumberTable;

typedef struct FklLineNumberTableNode
{
	FklSid_t fid;
	uint32_t scp;
	uint32_t cpc;
	uint32_t line;
}LineNumTabNode;
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
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt*,int32_t);
void fklCodefklLntCat(FklByteCodelnt*,FklByteCodelnt*);
void fklCodelntCopyCat(FklByteCodelnt*,const FklByteCodelnt*);
void reCodefklLntCat(FklByteCodelnt*,FklByteCodelnt*);

FklByteCodeLabel* fklNewByteCodeLable(int32_t,const char*);
FklByteCodeLabel* fklFindByteCodeLabel(const char*,FklPtrStack*);
void fklFreeByteCodeLabel(FklByteCodeLabel*);

LineNumberTable* fklNewLineNumTable();
LineNumTabNode* fklNewLineNumTabNode(FklSid_t fid,int32_t scp,int32_t cpc,int32_t line);
LineNumTabNode* fklFindLineNumTabNode(uint32_t cp,LineNumberTable*);
void fklFreeLineNumTabNode(LineNumTabNode*);
void fklFreeLineNumberTable(LineNumberTable*);
void fklLntCat(LineNumberTable* t,int32_t bs,LineNumTabNode** l2,int32_t s2);
void fklWriteLineNumberTable(LineNumberTable*,FILE*);
void fklDBG_printByteCode(uint8_t* code,uint32_t s,uint32_t c,FILE*);
#endif
