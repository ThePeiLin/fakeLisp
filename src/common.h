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

int isHexNum(const char*);
int isOctNum(const char*);
int isDouble(const char*);
int isNum(const char*);
char* getStringFromList(const char*);
char* getStringAfterBackslash(const char*);
int power(int,int);
char* castEscapeCharater(const char*,char,size_t*);
void printRawString(const char*,FILE*);
void printRawChar(char,FILE*);
char* doubleToString(double);
double stringToDouble(const char*);
char* intToString(long);
int64_t stringToInt(const char*);
int32_t countChar(const char*,char,int32_t);
int stringToChar(const char*);
uint8_t* castStrByteStr(const char*);
uint8_t castCharInt(char);
void exError(const AST_cptr*,int,Intpr*);
void printCptr(const AST_cptr*,FILE*);
PreDef* addDefine(const char*,const AST_cptr*,PreEnv*);
PreDef* findDefine(const char*,const PreEnv*);
PreDef* newDefines(const char*);
AST_pair* newPair(int,AST_pair*);
AST_cptr* newCptr(int,AST_pair*);
AST_atom* newAtom(int type,const char*,AST_pair*);
CompDef* addCompDef(const char*,CompEnv*);
CompEnv* newCompEnv(CompEnv*);
void destroyCompEnv(CompEnv*);
CompDef* findCompDef(const char*,CompEnv*);
Intpr* newIntpr(const char*,FILE*,CompEnv*,LineNumberTable*,VMDefTypes* deftypes);
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
void replaceCptr(AST_cptr*,const AST_cptr*);
AST_cptr* nextCptr(const AST_cptr*);
AST_cptr* prevCptr(const AST_cptr*);
AST_cptr* getLastCptr(const AST_cptr*);
AST_cptr* getFirstCptr(const AST_cptr*);
ByteCode* newByteCode(unsigned int);
void codeCat(ByteCode*,const ByteCode*);
void reCodeCat(const ByteCode*,ByteCode*);
void initCompEnv(CompEnv*);
ByteCode* copyByteCode(const ByteCode*);
void freeByteCode(ByteCode*);
void printByteCode(const ByteCode*,FILE*);
void printByteStr(size_t size,const uint8_t* str,FILE*,int);
void printAsByteStr(const uint8_t*,int32_t,FILE*);
int hasLoadSameFile(const char*,const Intpr*);
Intpr* getFirstIntpr(Intpr*);
AST_cptr* getASTPairCar(const AST_cptr*);
AST_cptr* getASTPairCdr(const AST_cptr*);
AST_cptr* baseCreateTree(const char*,Intpr*);
void changeWorkPath(const char*);
char* getDir(const char*);
char* getStringFromFile(FILE*);
int eqByteString(const ByteString*,const ByteString*);
char** split(char*,char*,int*);
char* relpath(char*,char*);
char* getLastWorkDir(Intpr*);

SymbolTable* newSymbolTable();
SymTabNode* newSymTabNode(const char*);
SymTabNode* addSymbol(const char*,SymbolTable*);
SymTabNode* addSymbolToGlob(const char*);
SymTabNode* findSymbol(const char*,SymbolTable*);
SymTabNode* findSymbolInGlob(const char*);
SymTabNode* getGlobSymbolWithId(Sid_t id);
void printSymbolTable(SymbolTable*,FILE*);
void printGlobSymbolTable(FILE*);
void freeSymTabNode(SymTabNode*);
void freeSymbolTable(SymbolTable*);
void freeGlobSymbolTable();

void writeSymbolTable(SymbolTable*,FILE*);
void writeGlobSymbolTable(FILE*);
void writeLineNumberTable(LineNumberTable*,FILE*);

LineNumberTable* newLineNumTable();
LineNumTabNode* newLineNumTabNode(int32_t fid,int32_t scp,int32_t cpc,int32_t line);
LineNumTabNode* findLineNumTabNode(uint32_t cp,LineNumberTable*);
void freeLineNumTabNode(LineNumTabNode*);
void freeLineNumberTable(LineNumberTable*);

ByteCodelnt* newByteCodelnt(ByteCode* bc);
void printByteCodelnt(ByteCodelnt* obj,FILE* fp);
void freeByteCodelnt(ByteCodelnt*);
void increaseScpOfByteCodelnt(ByteCodelnt*,int32_t);
void codelntCat(ByteCodelnt*,ByteCodelnt*);
void codelntCopyCat(ByteCodelnt*,const ByteCodelnt*);
void lntCat(LineNumberTable* t,int32_t bs,LineNumTabNode** l2,int32_t s2);
void reCodelntCat(ByteCodelnt*,ByteCodelnt*);

ByteCodeLabel* newByteCodeLable(int32_t,const char*);
ByteCodeLabel* findByteCodeLabel(const char*,ComStack*);
void freeByteCodeLabel(ByteCodeLabel*);

ComStack* newComStack(uint32_t size);
void pushComStack(void* data,ComStack*);
void* popComStack(ComStack*);
void* topComStack(ComStack*);
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

void freeAllMacro(PreMacro* head);
void freeAllKeyWord(KeyWord*);

QueueNode* newQueueNode(void*);
void freeQueueNode(QueueNode*);

ComQueue* newComQueue();
void freeComQueue(ComQueue*);
int32_t lengthComQueue(ComQueue*);
void* firstComQueue(ComQueue*);
void* popComQueue(ComQueue*);
void pushComQueue(void*,ComQueue*);
ComQueue* copyComQueue(ComQueue*);
char* strCat(char*,const char*);
uint8_t* createByteArry(int32_t);

VMDefTypes* newVMDefTypes(void);
#endif
