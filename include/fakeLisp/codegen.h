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
	FklPtrStack uref;

	uint32_t* scopes;
	uint32_t sc;
	uint32_t pscope;

	uint32_t prototypeId;

	struct FklCodegenEnv* prev;
	FklHashTable* defs;
	FklHashTable* refs;
	struct FklCodegenMacroScope* macros;
	size_t refcount;
}FklCodegenEnv;

void fklUpdatePrototype(FklFuncPrototypes* cp,FklCodegenEnv* env,FklSymbolTable* globalSymTable,FklSymbolTable* publicSymbolTable);

typedef struct FklCodegenMacro
{
	FklNastNode* pattern;
	FklFuncPrototypes* ptpool;
	FklByteCodelnt* bcl;
	struct FklCodegenMacro* next;
	uint8_t own;
}FklCodegenMacro;

typedef struct FklCodegenMacroScope
{
	uint32_t refcount;
	struct FklCodegenMacroScope* prev;
	FklHashTable* replacements;
	FklCodegenMacro* head;
	FklStringMatchPattern* patterns;
	FklStringMatchPattern** phead;
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
	uint32_t* exportIndex;
	FklSid_t* exports;
	FklSid_t* exportsP;
	FklCodegenMacro* head;
	FklHashTable* replacements;
	FklStringMatchPattern* patterns;
	uint32_t prototypeId;
}FklCodegenLib;

typedef struct FklCodegen
{
	char* filename;
	char* realpath;
	char* curDir;
	uint64_t curline;
	uint32_t builtinSymbolNum;
	uint8_t* builtinSymModiMark;
	FklCodegenEnv* globalEnv;
	FklSymbolTable* globalSymTable;
	FklSymbolTable* publicSymbolTable;
	FklSid_t fid;
	size_t exportNum;
	FklSid_t* exports;
	FklSid_t* exportsP;
	FklCodegenMacro* exportM;
	FklStringMatchPattern* exportRM;
	FklHashTable* exportR;
	FklPtrStack* loadedLibStack;
	FklPtrStack* macroLibStack;
	FklStringMatchPattern* head;
	FklStringMatchPattern** phead;
	struct FklCodegen* prev;
	unsigned int destroyAbleMark:1;
	unsigned int libMark:1;
	unsigned int macroMark:1;
	unsigned long refcount:61;
	FklFuncPrototypes* ptpool;
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

typedef FklByteCodelnt* (*FklByteCodeProcesser)(FklCodegen*
		,FklCodegenEnv*
		,uint32_t scope
		,FklCodegenMacroScope*
		,FklCodegenQuestContext* context
		,FklSid_t
		,uint64_t);

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
	uint32_t scope;
	FklCodegenMacroScope* macroScope;
    uint64_t curline;
	FklCodegen* codegen;
	struct FklCodegenQuest* prev;
	FklCodegenNextExpression* nextExpression;
}FklCodegenQuest;

void fklInitGlobalCodegener(FklCodegen* codegen
		,const char* rp
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* publicSymbolTable
		,int destroyAbleMark);
void fklInitCodegener(FklCodegen* codegen
		,const char* filename
		,FklCodegenEnv* env
		,FklCodegen* prev
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* publicSymbolTable
		,int destroyAbleMark
		,int libMark
		,int macroMark);

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

FklSymbolDef* fklFindSymbolDefByIdAndScope(FklSid_t id,uint32_t scope,FklCodegenEnv* env);

void fklPrintUndefinedRef(const FklCodegenEnv* env
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* publicSymbolTable);

uint32_t fklAddCodegenBuiltinRefBySid(FklSid_t id,FklCodegenEnv* env);
uint32_t fklAddCodegenRefBySid(FklSid_t id,FklCodegenEnv* env,FklSid_t fid,uint64_t line);

uint32_t fklAddCodegenDefBySid(FklSid_t id,uint32_t scope,FklCodegenEnv* env);

int fklIsSymbolDefined(FklSid_t sid,uint32_t scope,FklCodegenEnv*);

int fklIsReplacementDefined(FklSid_t sid,FklCodegenEnv*);

FklNastNode* fklGetReplacement(FklSid_t sid,FklHashTable*);

void fklAddReplacementBySid(FklSid_t sid,FklNastNode*,FklHashTable*);

FklCodegenEnv* fklCreateCodegenEnv(FklCodegenEnv* prev,uint32_t pscope,FklCodegenMacroScope*);
void fklDestroyCodegenEnv(FklCodegenEnv* env);

void fklInitCodegenScriptLib(FklCodegenLib* lib
		,char* rp
		,FklByteCodelnt* bcl
		,size_t exportNum
		,FklSid_t* exports
		,FklSid_t* exportsP
		,FklCodegenMacro* head
		,FklHashTable* replacements
		,FklStringMatchPattern* patterns
		,FklCodegenEnv* env);

typedef void (*FklCodegenDllLibInitExportFunc)(size_t*,FklSid_t**,FklSymbolTable* table);

FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(FklDllHandle dll);

void fklInitCodegenDllLib(FklCodegenLib* lib
		,char* rp
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklSymbolTable* ptable
		,FklCodegenDllLibInitExportFunc init);

void fklDestroyCodegenLibMacroScope(FklCodegenLib* lib);
void fklUninitCodegenLib(FklCodegenLib*);
void fklUninitCodegenLibInfo(FklCodegenLib*);

FklCodegenLib* fklCreateCodegenScriptLib(char* rp
		,FklByteCodelnt* bcl
		,size_t exportNum
		,FklSid_t* exports
		,FklSid_t* exportsP
		,FklCodegenMacro* head
		,FklHashTable* replacements
		,FklStringMatchPattern* patterns
		,FklCodegenEnv* env);

FklCodegenLib* fklCreateCodegenDllLib(char* rp
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklSymbolTable* ptable
		,FklCodegenDllLibInitExportFunc init);

void fklDestroyCodegenLib(FklCodegenLib*);

FklCodegenMacro* fklCreateCodegenMacro(FklNastNode* pattern
		,FklByteCodelnt* bcl
		,FklCodegenMacro* next
		,FklFuncPrototypes* ptpool
		,int own);
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
		,FklFuncPrototypes* ptpool
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklCodegen* codegen);

struct FklVMlib;
struct FklVMvalue;
typedef struct FklVMvarRef FklVMvarRef;

void fklInitVMlibWithCodegenLibRefs(FklCodegenLib* clib
		,struct FklVMlib* vlib
		,FklVM* exe
		,FklVMvarRef** refs
		,uint32_t count
		,int needCopy
		,FklFuncPrototypes*);

struct FklVMgc;
void fklInitVMlibWithCodgenLib(FklCodegenLib* clib
		,struct FklVMlib* vlib
		,struct FklVMgc* vm
		,int needCopy
		,FklFuncPrototypes*);

void fklInitVMlibWithCodgenLibAndDestroy(FklCodegenLib* clib
		,struct FklVMlib* vlib
		,struct FklVMgc* vm
		,FklFuncPrototypes*);

void fklInitFrameToReplFrame(FklVM*,FklCodegen* codegen,const FklSid_t* headSymbol);

#ifdef __cplusplus
}
#endif
#endif
