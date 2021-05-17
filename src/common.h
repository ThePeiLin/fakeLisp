#ifndef TOOL_H
#define TOOL_H
#include"fakedef.h"
#include<stdint.h>

#define INCREASE_ALL_SCP(l,ls,s) {int32_t i=0;\
	for(;i<(ls);i++)\
	(l)[i]->scp+=(s);\
}

#define MIN(a,b) (((a)<(b))?(a):(b))

int isHexNum(const char*);
int isOctNum(const char*);
int isDouble(const char*);
int isNum(const char*);
char* getStringFromList(const char*);
char* getStringAfterBackslash(const char*);
int power(int,int);
char* castEscapeCharater(const char*,char,int32_t*);
void printRawString(const char*,FILE*);
void printRawChar(char,FILE*);
char* doubleToString(double);
double stringToDouble(const char*);
char* intToString(long);
int32_t stringToInt(const char*);
int32_t countChar(const char*,char,int32_t);
int stringToChar(const char*);
uint8_t* castStrByteStr(const char*);
uint8_t castCharInt(char);
void errors(const char*,const char*,int);
void exError(const AST_cptr*,int,Intpr*);
void printCptr(const AST_cptr*,FILE*);
PreDef* addDefine(const char*,const AST_cptr*,PreEnv*);
PreDef* findDefine(const char*,const PreEnv*);
PreDef* newDefines(const char*);
AST_pair* newPair(int,AST_pair*);
AST_cptr* newCptr(int,AST_pair*);
AST_atom* newAtom(int type,const char*,AST_pair*);
CompDef* addCompDef(const char*,CompEnv*,SymbolTable*);
RawProc* newRawProc(int32_t);
RawProc* addRawProc(ByteCode*,Intpr*);
ByteCode* castRawproc(ByteCode*,RawProc*);
CompEnv* newCompEnv(CompEnv*);
void destroyCompEnv(CompEnv*);
CompDef* findCompDef(const char*,CompEnv*,SymbolTable*);
Intpr* newIntpr(const char*,FILE*,CompEnv*,SymbolTable*,LineNumberTable*);
Intpr* newTmpIntpr(const char*,FILE*);
int isscript(const char*);
int iscode(const char*);
int isAllSpace(const char*);
char* copyStr(const char*);
void freeIntpr(Intpr*);
void freeAtom(AST_atom*);
int destoryCptr(AST_cptr*);
PreEnv* newEnv(PreEnv*);
void destroyEnv(PreEnv*);
int AST_cptrcmp(const AST_cptr*,const AST_cptr*);
int deleteCptr(AST_cptr*);
int copyCptr(AST_cptr*,const AST_cptr*);
void* copyMemory(void*,size_t);
void replace(AST_cptr*,const AST_cptr*);
AST_cptr* nextCptr(const AST_cptr*);
AST_cptr* prevCptr(const AST_cptr*);
AST_cptr* getLastCptr(const AST_cptr*);
AST_cptr* getFirstCptr(const AST_cptr*);
ByteCode* newByteCode(unsigned int);
void codeCat(ByteCode*,const ByteCode*);
void reCodeCat(const ByteCode*,ByteCode*);
void initCompEnv(CompEnv*,SymbolTable*);
ByteCode* copyByteCode(const ByteCode*);
void freeByteCode(ByteCode*);
void printByteCode(const ByteCode*,FILE*);
void printByteStr(const ByteString*,FILE*,int);
void printAsByteStr(const uint8_t*,int32_t,FILE*);
int hasLoadSameFile(const char*,const Intpr*);
RawProc* getHeadRawProc(const Intpr*);
Intpr* getFirstIntpr(Intpr*);
AST_cptr* getANSPairCar(const AST_cptr*);
AST_cptr* getANSPairCdr(const AST_cptr*);
AST_cptr* baseCreateTree(const char*,Intpr*);
void changeWorkPath(const char*);
char* getDir(const char*);
char* getStringFromFile(FILE*);
void freeAllRawProc(RawProc*);
int bytsStrEq(ByteString*,ByteString*);
char** split(char*,char*,int*);
char* relpath(char*,char*);
char* getLastWorkDir(Intpr*);
void freeRawProc(ByteCode*,int32_t);

SymbolTable* newSymbolTable();
SymTabNode* newSymTabNode(const char*);
SymTabNode* addSymTabNode(SymTabNode*,SymbolTable*);
SymTabNode* findSymbol(const char*,SymbolTable*);
void printSymbolTable(SymbolTable*,FILE*);
void freeSymTabNode(SymTabNode*);
void freeSymbolTable(SymbolTable*);

void writeSymbolTable(SymbolTable*,FILE*);
void writeLineNumberTable(LineNumberTable*,FILE*);

LineNumberTable* newLineNumTable();
LineNumTabNode* newLineNumTabNode(int32_t fid,int32_t scp,int32_t cpc,int32_t line);
LineNumTabNode* findLineNumTabNode(int32_t id,int32_t cp,LineNumberTable*);
LineNumTabId* addLineNumTabId(LineNumTabNode** list,int32_t size,int32_t id,LineNumberTable* table);
LineNumTabId* newLineNumTabId(int32_t id);
void freeLineNumTabNode(LineNumTabNode*);
void freeLineNumberTable(LineNumberTable*);

ByteCodelnt* newByteCodelnt(ByteCode* bc);
void printByteCodelnt(ByteCodelnt* obj,SymbolTable* table,FILE* fp);
void freeByteCodelnt(ByteCodelnt*);
void increaseScpOfByteCodelnt(ByteCodelnt*,int32_t);
void codelntCat(ByteCodelnt*,ByteCodelnt*);
void reCodelntCat(ByteCodelnt*,ByteCodelnt*);

ByteCodeLabel* newByteCodeLable(int32_t,const char*);
ByteCodeLabel* findByteCodeLable(const char*,ByteCodeLabel*);
void addByteCodeLabel(ByteCodeLabel* obj,ByteCodeLabel** head);
void freeByteCodeLabel(ByteCodeLabel*);
void destroyByteCodeLabelChain(ByteCodeLabel* head);

ComStack* newComStack(uint32_t size);
void pushComStack(void* data,ComStack*);
void* popComStack(ComStack*);
void freeComStack(ComStack*);
void recycleComStack(ComStack*);
int isComStackEmpty(ComStack*);

FakeMem* newFakeMem(void* mem,void (*destructor)(void*));
void freeFakeMem(FakeMem*);

FakeMemMenager* newFakeMemMenager(size_t size);
void* reallocFakeMem(void* o_block,void* n_block,FakeMemMenager*);
void freeFakeMemMenager(FakeMemMenager*);
void pushFakeMem(void* block,void (*destructor)(void*),FakeMemMenager* memMenager);
void deleteFakeMem(void* block,FakeMemMenager* memMenager);

void mergeSort(void* base
		,size_t num
		,size_t size
		,int (*cmpf)(const void*,const void*));
#endif
