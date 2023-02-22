#ifndef FKL_CODEGEN_H
#define FKL_CODEGEN_H
#include"utils.h"
#include"symbol.h"
#include"pattern.h"
#include"base.h"
#include"parser.h"
#include"bytecode.h"
#include"builtin.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklCodegenEnv
{
	struct FklCodegenEnv* prev;
	FklHashTable* defs;
	FklHashTable* refs;
	struct FklCodegenMacroScope* macros;
	FklStringMatchPattern** phead;
	size_t refcount;
}FklCodegenEnv;

typedef struct FklCodegenMacro
{
	FklNastNode* pattern;
	FklByteCodelnt* bcl;
	struct FklCodegenMacro* next;
}FklCodegenMacro;

typedef struct FklCodegenMacroScope
{
	struct FklCodegenMacroScope* prev;
	FklHashTable* replacements;
	FklCodegenMacro* head;
	FklStringMatchPattern* patterns;
}FklCodegenMacroScope;

typedef enum FklCodegenLibType
{
	FKL_CODEGEN_LIB_SCRIPT=0,
	FKL_CODEGEN_LIB_DLL=1,
}FklCodegenLibType;

typedef struct FklCodegenLib
{
	FklCodegenLibType type;
	union
	{
		FklByteCodelnt* bcl;
		FklDllHandle dll;
	}u;
	char* rp;
	size_t exportNum;
	FklSid_t* exports;
	FklCodegenMacro* head;
	FklHashTable* replacements;
	FklStringMatchPattern* patterns;
}FklCodegenLib;

typedef struct FklCodegen
{
	char* filename;
	char* realpath;
	char* curDir;
	uint64_t curline;
	FklCodegenEnv* globalEnv;
	FklSymbolTable* globalSymTable;
	FklSymbolTable* publicSymbolTable;
	FklSid_t fid;
	size_t exportNum;
	FklSid_t* exports;
	FklPtrStack* loadedLibStack;
	FklPtrStack* macroLibStack;
	FklStringMatchPattern* head;
	FklStringMatchPattern** phead;
	struct FklCodegen* prev;
	unsigned int destroyAbleMark:1;
	unsigned long refcount:63;
	FklPrototypePool* cpool;
}FklCodegen;

typedef struct
{
	void (*__put_bcl)(void*,FklByteCodelnt* bcl);
	void (*__finalizer)(void*);
	FklPtrStack* (*__get_bcl_stack)(void*);
}FklCodegenQuestContextMethodTable;

typedef struct FklCodegenQuestContext
{
	void* data;
	const FklCodegenQuestContextMethodTable* t;
}FklCodegenQuestContext;

typedef FklByteCodelnt* (*FklByteCodeProcesser)(FklCodegen*,FklCodegenEnv*,FklCodegenQuestContext* context,FklSid_t,uint64_t);

typedef struct FklCodegenErrorState
{
	FklSid_t fid;
	FklBuiltInErrorType type;
	FklNastNode* place;
	size_t line;
}FklCodegenErrorState;

typedef struct
{
	FklNastNode* (*getNextExpression)(void*,FklCodegenErrorState*);
	void (*finalizer)(void*);
}FklNextExpressionMethodTable;

typedef struct
{
	const FklNextExpressionMethodTable* t;
	void* context;
}FklCodegenNextExpression;

typedef struct FklCodegenQuest
{
	FklByteCodeProcesser processer;
	FklCodegenQuestContext* context;
	FklCodegenEnv* env;
    uint64_t curline;
	FklCodegen* codegen;
	struct FklCodegenQuest* prev;
	FklCodegenNextExpression* nextExpression;
}FklCodegenQuest;

void fklInitGlobalCodegener(FklCodegen* codegen
		,const char* rp
		,FklCodegenEnv* globalEnv
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* publicSymbolTable
		,int destroyAbleMark);
void fklInitCodegener(FklCodegen* codegen
		,const char* filename
		,FklCodegenEnv* env
		,FklCodegen* prev
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* publicSymbolTable
		,int destroyAbleMark);

void fklUninitCodegener(FklCodegen* codegen);
void fklDestroyCodegener(FklCodegen* codegen);
FklCodegen* fklCreateCodegener(void);
const FklSid_t* fklInitCodegen(FklSymbolTable*);
void fklUninitCodegen(void);
FklByteCode* fklCodegenNode(const FklNastNode*,FklCodegen* codegen);
FklByteCodelnt* fklGenExpressionCode(FklNastNode* exp
		,FklCodegenEnv* globalEnv
		,FklCodegen* codegen);
FklByteCodelnt* fklGenExpressionCodeWithQuest(FklCodegenQuest*
		,FklCodegen* codegen);
FklByteCodelnt* fklGenExpressionCodeWithFp(FILE*
		,FklCodegen* codegen);

uint32_t fklAddCodegenRefById(FklSid_t id,FklCodegenEnv* env);
uint32_t fklAddCodegenDefBySid(FklSid_t sid,FklCodegenEnv*);
int fklIsSymbolDefined(FklSid_t sid,FklCodegenEnv*);
int fklIsReplacementDefined(FklSid_t sid,FklCodegenEnv*);

FklNastNode* fklGetReplacement(FklSid_t sid,FklCodegenEnv*);
void fklAddReplacementBySid(FklSid_t sid,FklNastNode*,FklCodegenEnv*);

FklCodegenEnv* fklCreateCodegenEnv(FklCodegenEnv* prev);
void fklDestroyCodegenEnv(FklCodegenEnv* env);
//FklCodegenEnv* fklCreateGlobCodegenEnv(FklSymbolTable* publicSymbolTable);

void fklCodegenPrintUndefinedSymbol(FklByteCodelnt* code,FklCodegenLib**,FklSymbolTable* symbolTable,size_t exportNum,FklSid_t* exports);

void fklInitCodegenScriptLib(FklCodegenLib* lib
		,char* rp
		,FklByteCodelnt* bcl
		,size_t exportNum
		,FklSid_t* exports
		,FklCodegenMacro* head
		,FklHashTable* replacements
		,FklStringMatchPattern* patterns);

typedef void (*FklCodegenDllLibInitExportFunc)(size_t*,FklSid_t**,FklSymbolTable* table);

FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(FklDllHandle dll);

void fklInitCodegenDllLib(FklCodegenLib* lib
		,char* rp
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init);

void fklDestroyCodegenLibMacroScope(FklCodegenLib* lib);
void fklUninitCodegenLib(FklCodegenLib*);

FklCodegenLib* fklCreateCodegenScriptLib(char* rp
		,FklByteCodelnt* bcl
		,size_t exportNum
		,FklSid_t* exports
		,FklCodegenMacro* head
		,FklHashTable* replacements
		,FklStringMatchPattern* patterns);

FklCodegenLib* fklCreateCodegenDllLib(char* rp
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init);

void fklDestroyCodegenLib(FklCodegenLib*);

FklCodegenMacro* fklCreateCodegenMacro(FklNastNode* pattern
		,FklByteCodelnt* bcl
		,FklCodegenMacro* next);
void fklDestroyCodegenMacro(FklCodegenMacro* macro);
void fklDestroyCodegenMacroList(FklCodegenMacro* macro);

FklCodegenMacroScope* fklCreateCodegenMacroScope(FklCodegenMacroScope* prev);
void fklDestroyCodegenMacroScope(FklCodegenMacroScope* c);

FklNastNode* fklTryExpandCodegenMacro(FklNastNode* exp
		,FklCodegen*
		,FklCodegenMacroScope* macros
		,FklCodegenErrorState*);

struct FklVM;
struct FklVM* fklInitMacroExpandVM(FklByteCodelnt* bcl
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklCodegen* codegen);

struct FklVMlib;
struct FklVMvalue;
struct FklVMCompoundFrameVarRef;

void fklInitVMlibWithCodegenLibRefs(FklCodegenLib* clib
		,struct FklVMlib* vlib
		,FklVM* exe
		,struct FklVMCompoundFrameVarRef* lr
		,int needCopy);

void fklInitVMlibWithCodgenLib(FklCodegenLib* clib
		,struct FklVMlib* vlib
		,struct FklVMvalue* globEnv
		,struct FklVMgc* vm
		,int needCopy);

void fklInitVMlibWithCodgenLibAndDestroy(FklCodegenLib* clib
		,struct FklVMlib* vlib
		,struct FklVMvalue* globEnv
		,struct FklVMgc* vm);
#ifdef __cplusplus
}
#endif
#endif
