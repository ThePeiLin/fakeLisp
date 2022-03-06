#ifndef TOOL_H
#define TOOL_H
#include"fakedef.h"
#include<stdint.h>

int fklIsHexNum(const char*);
int fklIsOctNum(const char*);
int fklIsDouble(const char*);
int fklIsNum(const char*);
char* fklGetStringFromList(const char*);
char* fklGetStringAfterBackslash(const char*);
int fklPower(int,int);
char* fklCastEscapeCharater(const char*,char,size_t*);
void fklPrintRawString(const char*,FILE*);
void fklPrintRawChar(char,FILE*);
char* fklDoubleToString(double);
double fklStringToDouble(const char*);
char* fklIntToString(long);
int64_t fklStringToInt(const char*);
int32_t fklCountChar(const char*,char,int32_t);
int fklStringToChar(const char*);
uint8_t* fklCastStrByteStr(const char*);
uint8_t fklCastCharInt(char);
FklAstPair* fklNewPair(int,FklAstPair*);
FklAstCptr* fklNewCptr(int,FklAstPair*);
FklAstAtom* fklNewAtom(int type,const char*,FklAstPair*);
int fklIsscript(const char*);
int fklIscode(const char*);
int fklIsAllSpace(const char*);
char* fklCopyStr(const char*);
int fklFklAstCptrcmp(const FklAstCptr*,const FklAstCptr*);
int fklDeleteCptr(FklAstCptr*);
int fklCopyCptr(FklAstCptr*,const FklAstCptr*);
void* fklCopyMemory(void*,size_t);
void fklReplaceCptr(FklAstCptr*,const FklAstCptr*);
FklAstCptr* fklNextCptr(const FklAstCptr*);
FklAstCptr* fklPrevCptr(const FklAstCptr*);
FklAstCptr* fklGetLastCptr(const FklAstCptr*);
FklAstCptr* fklGetFirstCptr(const FklAstCptr*);
FklByteCode* fklNewByteCode(unsigned int);
void fklCodeCat(FklByteCode*,const FklByteCode*);
void fklReCodeCat(const FklByteCode*,FklByteCode*);
FklByteCode* fklCopyByteCode(const FklByteCode*);
FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt*);
int fklIsSymbolShouldBeExport(const char* str,const char** pStr,uint32_t n);
void fklFreeByteCode(FklByteCode*);
void fklPrintByteCode(const FklByteCode*,FILE*);
void fklPrintByteStr(size_t size,const uint8_t* str,FILE*,int);
void fklPrintAsByteStr(const uint8_t*,int32_t,FILE*);
FklAstCptr* fklGetASTPairCar(const FklAstCptr*);
FklAstCptr* fklGetASTPairCdr(const FklAstCptr*);
FklAstCptr* fklGetCptrCar(const FklAstCptr*);
FklAstCptr* fklGetCptrCdr(const FklAstCptr*);
void fklChangeWorkPath(const char*);
char* fklGetDir(const char*);
char* fklGetStringFromFile(FILE*);
int fklEqByteString(const FklByteString*,const FklByteString*);
char** fklSplit(char*,char*,int*);
char* fklRelpath(char*,char*);

FklSymbolTable* fklNewSymbolTable();
FklSymTabNode* fklNewSymTabNode(const char*);
FklSymTabNode* fklAddSymbol(const char*,FklSymbolTable*);
FklSymTabNode* fklAddSymbolToGlob(const char*);
FklSymTabNode* fklFindSymbol(const char*,FklSymbolTable*);
FklSymTabNode* fklFindSymbolInGlob(const char*);
FklSymTabNode* fklGetGlobSymbolWithId(FklSid_t id);
void fklPrintSymbolTable(FklSymbolTable*,FILE*);
void fklPrintGlobSymbolTable(FILE*);
void fklFreeSymTabNode(FklSymTabNode*);
void fklFreeSymbolTable(FklSymbolTable*);
void fklFreeGlobSymbolTable();

void fklWriteSymbolTable(FklSymbolTable*,FILE*);
void fklWriteGlobSymbolTable(FILE*);
void fklWriteLineNumberTable(LineNumberTable*,FILE*);

LineNumberTable* fklNewLineNumTable();
LineNumTabNode* fklNewLineNumTabNode(FklSid_t fid,int32_t scp,int32_t cpc,int32_t line);
LineNumTabNode* fklFindLineNumTabNode(uint32_t cp,LineNumberTable*);
void fklFreeLineNumTabNode(LineNumTabNode*);
void fklFreeLineNumberTable(LineNumberTable*);

void fklFreeDefTypeTable(FklVMDefTypes* defs);
FklByteCodelnt* fklNewByteCodelnt(FklByteCode* bc);
void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp);
void fklFreeByteCodelnt(FklByteCodelnt*);
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt*,int32_t);
void fklCodefklLntCat(FklByteCodelnt*,FklByteCodelnt*);
void fklCodelntCopyCat(FklByteCodelnt*,const FklByteCodelnt*);
void fklLntCat(LineNumberTable* t,int32_t bs,LineNumTabNode** l2,int32_t s2);
void reCodefklLntCat(FklByteCodelnt*,FklByteCodelnt*);

FklByteCodeLabel* fklNewByteCodeLable(int32_t,const char*);
FklByteCodeLabel* fklFindByteCodeLabel(const char*,FklPtrStack*);
void fklFreeByteCodeLabel(FklByteCodeLabel*);

FklPtrStack* fklNewPtrStack(uint32_t size);
void fklPushPtrStack(void* data,FklPtrStack*);
void* fklPopPtrStack(FklPtrStack*);
void* fklTopPtrStack(FklPtrStack*);
void fklFreePtrStack(FklPtrStack*);
void fklRecyclePtrStack(FklPtrStack*);
int fklIsPtrStackEmpty(FklPtrStack*);

FklMem* fklNewMem(void* mem,void (*destructor)(void*));
void fklFreeMem(FklMem*);

FklMemMenager* fklNewMemMenager(size_t size);
void* fklReallocMem(void* o_block,void* n_block,FklMemMenager*);
void fklFreeMemMenager(FklMemMenager*);
void fklPushMem(void* block,void (*destructor)(void*),FklMemMenager* memMenager);
void fklDeleteMem(void* block,FklMemMenager* memMenager);

void mergeSort(void* base
		,size_t num
		,size_t size
		,int (*cmpf)(const void*,const void*));

FklQueueNode* fklNewQueueNode(void*);
void fklFreeQueueNode(FklQueueNode*);

FklPtrQueue* fklNewPtrQueue();
void fklFreePtrQueue(FklPtrQueue*);
int32_t fklLengthPtrQueue(FklPtrQueue*);
void* fklFirstPtrQueue(FklPtrQueue*);
void* fklPopPtrQueue(FklPtrQueue*);
void fklPushPtrQueue(void*,FklPtrQueue*);
FklPtrQueue* fklCopyPtrQueue(FklPtrQueue*);
char* fklStrCat(char*,const char*);
uint8_t* fklCreateByteArry(int32_t);

FklVMDefTypes* fklNewVMDefTypes(void);
#endif
