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
void fklExError(const AST_cptr*,int,Intpr*);
void fklPrintCptr(const AST_cptr*,FILE*);
PreDef* fklAddDefine(const char*,const AST_cptr*,PreEnv*);
PreDef* fklFindDefine(const char*,const PreEnv*);
PreDef* fklNewDefines(const char*);
AST_pair* fklNewPair(int,AST_pair*);
AST_cptr* fklNewCptr(int,AST_pair*);
AST_atom* fklNewAtom(int type,const char*,AST_pair*);
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
void fklFreeAtom(AST_atom*);
int fklDestoryCptr(AST_cptr*);
PreEnv* fklNewEnv(PreEnv*);
void fklDestroyEnv(PreEnv*);
int fklAST_cptrcmp(const AST_cptr*,const AST_cptr*);
int fklDeleteCptr(AST_cptr*);
int fklCopyCptr(AST_cptr*,const AST_cptr*);
void* fklCopyMemory(void*,size_t);
void fklReplaceCptr(AST_cptr*,const AST_cptr*);
AST_cptr* fklNextCptr(const AST_cptr*);
AST_cptr* fklPrevCptr(const AST_cptr*);
AST_cptr* fklGetLastCptr(const AST_cptr*);
AST_cptr* fklGetFirstCptr(const AST_cptr*);
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
AST_cptr* fklGetASTPairCar(const AST_cptr*);
AST_cptr* fklGetASTPairCdr(const AST_cptr*);
AST_cptr* fklGetCptrCar(const AST_cptr*);
AST_cptr* fklGetCptrCdr(const AST_cptr*);
AST_cptr* fklBaseCreateTree(const char*,Intpr*);
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

FakeMem* fklNewFakeMem(void* mem,void (*destructor)(void*));
void fklFreeFakeMem(FakeMem*);

FakeMemMenager* fklNewFakeMemMenager(size_t size);
void* fklReallocFakeMem(void* o_block,void* n_block,FakeMemMenager*);
void fklFreeFakeMemMenager(FakeMemMenager*);
void fklPushFakeMem(void* block,void (*destructor)(void*),FakeMemMenager* memMenager);
void fklDeleteFakeMem(void* block,FakeMemMenager* memMenager);

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
