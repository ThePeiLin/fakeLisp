#ifndef TOOL_H
#define TOOL_H
#include"fakedef.h"
#include<stdint.h>

#define INCREASE_ALL_SCP(l,ls,s) {int32_t i=0;\
	for(;i<(ls);i++)\
	(l)[i]->scp+=(s);\
}

#define MIN(a,b) (((a)<(b))?(a):(b))

#define FAKE_ASSERT(exp,str,filepath,cl) \
{ \
	if(!(exp)) \
	{\
		fprintf(stderr,"In file \"%s\" line %d\n",(filepath),(cl));\
		perror((str));\
		exit(1);\
	}\
}

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
void fklExError(const FklAstCptr*,int,FklIntpr*);
void fklPrintCptr(const FklAstCptr*,FILE*);
FklPreDef* fklAddDefine(const char*,const FklAstCptr*,FklPreEnv*);
FklPreDef* fklFindDefine(const char*,const FklPreEnv*);
FklPreDef* fklNewDefines(const char*);
FklAstPair* fklNewPair(int,FklAstPair*);
FklAstCptr* fklNewCptr(int,FklAstPair*);
FklAstAtom* fklNewAtom(int type,const char*,FklAstPair*);
FklCompDef* fklAddCompDef(const char*,FklCompEnv*);
FklCompEnv* fklNewCompEnv(FklCompEnv*);
void fklFreeAllMacroThenDestroyCompEnv(FklCompEnv* env);
void fklDestroyCompEnv(FklCompEnv*);
FklCompDef* fklFindCompDef(const char*,FklCompEnv*);
FklIntpr* fklNewIntpr(const char*,FILE*,FklCompEnv*,LineNumberTable*,FklVMDefTypes* deftypes);
FklIntpr* fklNewTmpIntpr(const char*,FILE*);
int fklIsscript(const char*);
int fklIscode(const char*);
int fklIsAllSpace(const char*);
char* fklCopyStr(const char*);
void fklFreeIntpr(FklIntpr*);
void fklFreeAtom(FklAstAtom*);
int fklDestoryCptr(FklAstCptr*);
FklPreEnv* fklNewEnv(FklPreEnv*);
void fklDestroyEnv(FklPreEnv*);
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
void fklInitCompEnv(FklCompEnv*);
FklByteCode* fklCopyByteCode(const FklByteCode*);
FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt*);
int fklIsSymbolShouldBeExport(const char* str,const char** pStr,uint32_t n);
void fklFreeByteCode(FklByteCode*);
void fklPrintByteCode(const FklByteCode*,FILE*);
void fklPrintByteStr(size_t size,const uint8_t* str,FILE*,int);
void fklPrintAsByteStr(const uint8_t*,int32_t,FILE*);
int fklHasLoadSameFile(const char*,const FklIntpr*);
FklIntpr* fklGetFirstIntpr(FklIntpr*);
FklAstCptr* fklGetASTPairCar(const FklAstCptr*);
FklAstCptr* fklGetASTPairCdr(const FklAstCptr*);
FklAstCptr* fklGetCptrCar(const FklAstCptr*);
FklAstCptr* fklGetCptrCdr(const FklAstCptr*);
FklAstCptr* fklBaseCreateTree(const char*,FklIntpr*);
void fklChangeWorkPath(const char*);
char* fklGetDir(const char*);
char* fklGetStringFromFile(FILE*);
int fklEqByteString(const FklByteString*,const FklByteString*);
char** fklSplit(char*,char*,int*);
char* fklRelpath(char*,char*);
char* fklGetLastWorkDir(FklIntpr*);

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
LineNumTabNode* fklNewLineNumTabNode(int32_t fid,int32_t scp,int32_t cpc,int32_t line);
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
FklByteCodeLabel* fklFindByteCodeLabel(const char*,FklComStack*);
void fklFreeByteCodeLabel(FklByteCodeLabel*);

FklComStack* fklNewComStack(uint32_t size);
void fklPushComStack(void* data,FklComStack*);
void* fklPopComStack(FklComStack*);
void* fklTopComStack(FklComStack*);
void fklFreeComStack(FklComStack*);
void fklRecycleComStack(FklComStack*);
int fklIsComStackEmpty(FklComStack*);

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

void fklFreeAllMacro(FklPreMacro* head);
void fklFreeAllKeyWord(FklKeyWord*);

FklQueueNode* fklNewQueueNode(void*);
void fklFreeQueueNode(FklQueueNode*);

FklComQueue* fklNewComQueue();
void fklFreeComQueue(FklComQueue*);
int32_t fklLengthComQueue(FklComQueue*);
void* fklFirstComQueue(FklComQueue*);
void* fklPopComQueue(FklComQueue*);
void fklPushComQueue(void*,FklComQueue*);
FklComQueue* fklCopyComQueue(FklComQueue*);
char* fklStrCat(char*,const char*);
uint8_t* fklCreateByteArry(int32_t);

FklVMDefTypes* fklNewVMDefTypes(void);
#endif
