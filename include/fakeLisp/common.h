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
void fklExError(const FklAstCptr*,int,Intpr*);
void fklPrintCptr(const FklAstCptr*,FILE*);
PreDef* fklAddDefine(const char*,const FklAstCptr*,PreEnv*);
PreDef* fklFindDefine(const char*,const PreEnv*);
PreDef* fklNewDefines(const char*);
FklAstPair* fklNewPair(int,FklAstPair*);
FklAstCptr* fklNewCptr(int,FklAstPair*);
FklAstAtom* fklNewAtom(int type,const char*,FklAstPair*);
CompDef* fklAddCompDef(const char*,CompEnv*);
CompEnv* fklNewCompEnv(CompEnv*);
void fklFreeAllMacroThenDestroyCompEnv(CompEnv* env);
void fklDestroyCompEnv(CompEnv*);
CompDef* fklFindCompDef(const char*,CompEnv*);
Intpr* fklNewIntpr(const char*,FILE*,CompEnv*,LineNumberTable*,VMDefTypes* deftypes);
Intpr* fklNewTmpIntpr(const char*,FILE*);
int fklIsscript(const char*);
int fklIscode(const char*);
int fklIsAllSpace(const char*);
char* fklCopyStr(const char*);
void fklFreeIntpr(Intpr*);
void fklFreeAtom(FklAstAtom*);
int fklDestoryCptr(FklAstCptr*);
PreEnv* fklNewEnv(PreEnv*);
void fklDestroyEnv(PreEnv*);
int fklFklAstCptrcmp(const FklAstCptr*,const FklAstCptr*);
int fklDeleteCptr(FklAstCptr*);
int fklCopyCptr(FklAstCptr*,const FklAstCptr*);
void* fklCopyMemory(void*,size_t);
void fklReplaceCptr(FklAstCptr*,const FklAstCptr*);
FklAstCptr* fklNextCptr(const FklAstCptr*);
FklAstCptr* fklPrevCptr(const FklAstCptr*);
FklAstCptr* fklGetLastCptr(const FklAstCptr*);
FklAstCptr* fklGetFirstCptr(const FklAstCptr*);
ByteCode* fklNewByteCode(unsigned int);
void fklCodeCat(ByteCode*,const ByteCode*);
void fklReCodeCat(const ByteCode*,ByteCode*);
void fklInitCompEnv(CompEnv*);
ByteCode* fklCopyByteCode(const ByteCode*);
ByteCodelnt* fklCopyByteCodelnt(const ByteCodelnt*);
int fklIsSymbolShouldBeExport(const char* str,const char** pStr,uint32_t n);
void fklFreeByteCode(ByteCode*);
void fklPrintByteCode(const ByteCode*,FILE*);
void fklPrintByteStr(size_t size,const uint8_t* str,FILE*,int);
void fklPrintAsByteStr(const uint8_t*,int32_t,FILE*);
int fklHasLoadSameFile(const char*,const Intpr*);
Intpr* fklGetFirstIntpr(Intpr*);
FklAstCptr* fklGetASTPairCar(const FklAstCptr*);
FklAstCptr* fklGetASTPairCdr(const FklAstCptr*);
FklAstCptr* fklGetCptrCar(const FklAstCptr*);
FklAstCptr* fklGetCptrCdr(const FklAstCptr*);
FklAstCptr* fklBaseCreateTree(const char*,Intpr*);
void fklChangeWorkPath(const char*);
char* fklGetDir(const char*);
char* fklGetStringFromFile(FILE*);
int fklEqByteString(const ByteString*,const ByteString*);
char** fklSplit(char*,char*,int*);
char* fklRelpath(char*,char*);
char* fklGetLastWorkDir(Intpr*);

SymbolTable* fklNewSymbolTable();
SymTabNode* fklNewSymTabNode(const char*);
SymTabNode* fklAddSymbol(const char*,SymbolTable*);
SymTabNode* fklAddSymbolToGlob(const char*);
SymTabNode* fklFindSymbol(const char*,SymbolTable*);
SymTabNode* fklFindSymbolInGlob(const char*);
SymTabNode* fklGetGlobSymbolWithId(Sid_t id);
void fklPrintSymbolTable(SymbolTable*,FILE*);
void fklPrintGlobSymbolTable(FILE*);
void fklFreeSymTabNode(SymTabNode*);
void fklFreeSymbolTable(SymbolTable*);
void fklFreeGlobSymbolTable();

void fklWriteSymbolTable(SymbolTable*,FILE*);
void fklWriteGlobSymbolTable(FILE*);
void fklWriteLineNumberTable(LineNumberTable*,FILE*);

LineNumberTable* fklNewLineNumTable();
LineNumTabNode* fklNewLineNumTabNode(int32_t fid,int32_t scp,int32_t cpc,int32_t line);
LineNumTabNode* fklFindLineNumTabNode(uint32_t cp,LineNumberTable*);
void fklFreeLineNumTabNode(LineNumTabNode*);
void fklFreeLineNumberTable(LineNumberTable*);

void fklFreeDefTypeTable(VMDefTypes* defs);
ByteCodelnt* fklNewByteCodelnt(ByteCode* bc);
void fklPrintByteCodelnt(ByteCodelnt* obj,FILE* fp);
void fklFreeByteCodelnt(ByteCodelnt*);
void fklIncreaseScpOfByteCodelnt(ByteCodelnt*,int32_t);
void fklCodefklLntCat(ByteCodelnt*,ByteCodelnt*);
void fklCodelntCopyCat(ByteCodelnt*,const ByteCodelnt*);
void fklLntCat(LineNumberTable* t,int32_t bs,LineNumTabNode** l2,int32_t s2);
void reCodefklLntCat(ByteCodelnt*,ByteCodelnt*);

ByteCodeLabel* fklNewByteCodeLable(int32_t,const char*);
ByteCodeLabel* fklFindByteCodeLabel(const char*,ComStack*);
void fklFreeByteCodeLabel(ByteCodeLabel*);

ComStack* fklNewComStack(uint32_t size);
void fklPushComStack(void* data,ComStack*);
void* fklPopComStack(ComStack*);
void* fklTopComStack(ComStack*);
void fklFreeComStack(ComStack*);
void fklRecycleComStack(ComStack*);
int fklIsComStackEmpty(ComStack*);

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

void fklFreeAllMacro(PreMacro* head);
void fklFreeAllKeyWord(KeyWord*);

QueueNode* fklNewQueueNode(void*);
void fklFreeQueueNode(QueueNode*);

ComQueue* fklNewComQueue();
void fklFreeComQueue(ComQueue*);
int32_t fklLengthComQueue(ComQueue*);
void* fklFirstComQueue(ComQueue*);
void* fklPopComQueue(ComQueue*);
void fklPushComQueue(void*,ComQueue*);
ComQueue* fklCopyComQueue(ComQueue*);
char* fklStrCat(char*,const char*);
uint8_t* fklCreateByteArry(int32_t);

VMDefTypes* fklNewVMDefTypes(void);
#endif
