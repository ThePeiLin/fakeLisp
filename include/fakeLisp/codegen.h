#ifndef FKL_CODEGEN_H
#define FKL_CODEGEN_H

#include"utils.h"
#include"symbol.h"
#include"pattern.h"
#include"grammer.h"
#include"base.h"
#include"nast.h"
#include"bytecode.h"
#include"builtin.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklCodegenEnvScope
{
	uint32_t p;
	uint32_t start;
	uint32_t empty;
	uint32_t end;
	FklHashTable* defs;
}FklCodegenEnvScope;

#define FKL_CODEGEN_ENV_SLOT_OCC (1)
#define FKL_CODEGEN_ENV_SLOT_REF (2)
typedef struct FklCodegenEnv
{
	FklPtrStack uref;

	uint8_t* slotFlags;
	FklCodegenEnvScope* scopes;
	uint32_t lcount;
	uint32_t sc;
	uint32_t pscope;

	uint32_t prototypeId;

	struct FklCodegenEnv* prev;
	FklHashTable* refs;
	struct FklCodegenMacroScope* macros;
	size_t refcount;
}FklCodegenEnv;

void fklUpdatePrototype(FklFuncPrototypes* cp
		,FklCodegenEnv* env
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* pst);

typedef struct FklCodegenMacro
{
	FklNastNode* pattern;
	FklFuncPrototypes* pts;
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
	};
	char* rp;
	size_t exportNum;
	uint32_t* exportIndex;
	FklSid_t* exports;
	FklSid_t* exports_in_pst;

	FklCodegenMacro* head;
	FklHashTable* replacements;

	FklHashTable named_prod_groups;
	FklSymbolTable* terminal_table;

	uint32_t prototypeId;
}FklCodegenLib;

typedef struct
{
	FklSid_t id;
	int is_ref_outer;
	FklHashTable prods;
	FklGrammerIgnore* ignore;
}FklGrammerProductionGroup;

typedef enum
{
	FKL_CODEGEN_PATTERN_BEGIN=0,
	FKL_CODEGEN_PATTERN_LOCAL,
	FKL_CODEGEN_PATTERN_LET0,
	FKL_CODEGEN_PATTERN_LET1,
	FKL_CODEGEN_PATTERN_LET80,
	FKL_CODEGEN_PATTERN_LET81,
	FKL_CODEGEN_PATTERN_LET82,
	FKL_CODEGEN_PATTERN_NAMED_LET0,
	FKL_CODEGEN_PATTERN_NAMED_LET1,
	FKL_CODEGEN_PATTERN_LETREC0,
	FKL_CODEGEN_PATTERN_LETREC1,

	FKL_CODEGEN_PATTERN_DO0,
	FKL_CODEGEN_PATTERN_DO0N,
	FKL_CODEGEN_PATTERN_DO1,
	FKL_CODEGEN_PATTERN_DO1N,
	FKL_CODEGEN_PATTERN_DO11,
	FKL_CODEGEN_PATTERN_DO11N,

	FKL_CODEGEN_PATTERN_DEFUN,
	FKL_CODEGEN_PATTERN_DEFINE,
	FKL_CODEGEN_PATTERN_SETQ,
	FKL_CODEGEN_PATTERN_CHECK,
	FKL_CODEGEN_PATTERN_QUOTE,
	FKL_CODEGEN_PATTERN_UNQUOTE,
	FKL_CODEGEN_PATTERN_QSQUOTE,
	FKL_CODEGEN_PATTERN_LAMBDA,
	FKL_CODEGEN_PATTERN_AND,
	FKL_CODEGEN_PATTERN_OR,
	FKL_CODEGEN_PATTERN_COND,
	FKL_CODEGEN_PATTERN_IF0,
	FKL_CODEGEN_PATTERN_IF1,
	FKL_CODEGEN_PATTERN_WHEN,
	FKL_CODEGEN_PATTERN_UNLESS,
	FKL_CODEGEN_PATTERN_LOAD,
	FKL_CODEGEN_PATTERN_IMPORT_PREFIX,
	FKL_CODEGEN_PATTERN_IMPORT_ONLY,
	FKL_CODEGEN_PATTERN_IMPORT_ALIAS,
	FKL_CODEGEN_PATTERN_IMPORT_EXCEPT,
	FKL_CODEGEN_PATTERN_IMPORT_COMMON,
	FKL_CODEGEN_PATTERN_IMPORT,
	FKL_CODEGEN_PATTERN_MACROEXPAND,
	FKL_CODEGEN_PATTERN_DEFMACRO,
	FKL_CODEGEN_PATTERN_IMPORT_NONE,
	FKL_CODEGEN_PATTERN_EXPORT_SINGLE,
	FKL_CODEGEN_PATTERN_EXPORT,
	FKL_CODEGEN_PATTERN_DEF_READER_MACROS,
	FKL_CODEGEN_PATTERN_NUM,
}FklCodegenPatternEnum;

typedef enum
{
	FKL_CODEGEN_SUB_PATTERN_UNQUOTE=0,
	FKL_CODEGEN_SUB_PATTERN_UNQTESP,
	FKL_CODEGEN_SUB_PATTERN_NUM,
}FklCodegenSubPatternEnum;

#define FKL_BUILTIN_REPLACEMENT_NUM (5)
#define FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM (17)
#define FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM (11)

typedef struct
{
	FklSymbolTable public_symbol_table;

	FklSid_t builtInPatternVar_rest;
	FklSid_t builtInPatternVar_name;
	FklSid_t builtInPatternVar_value;
	FklSid_t builtInPatternVar_cond;
	FklSid_t builtInPatternVar_args;
	FklSid_t builtInPatternVar_arg0;
	FklSid_t builtInPatternVar_arg1;
	FklSid_t builtInPatternVar_custom;
	FklSid_t builtInPatternVar_builtin;
	FklSid_t builtInPatternVar_simple;

	FklSid_t builtin_replacement_id[FKL_BUILTIN_REPLACEMENT_NUM];

	FklNastNode* builtin_pattern_node[FKL_CODEGEN_PATTERN_NUM];
	FklNastNode* builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_NUM];

	FklSid_t builtin_prod_action_id[FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM];
	FklSid_t simple_prod_action_id[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM];

}FklCodegenOuterCtx;

typedef struct FklCodegen
{
	FklCodegenOuterCtx* outer_ctx;

	struct FklCodegen* prev;

	char* filename;
	char* realpath;
	char* curDir;
	uint64_t curline;
	FklSid_t fid;

	struct
	{
		FklGrammer* self_g;
		FklHashTable self_builtin_prods;
		FklGrammerIgnore* self_builtin_ignores;
		FklHashTable self_unnamed_prods;
		FklGrammerIgnore* self_unnamed_ignores;
		FklHashTable self_named_prod_groups;
	};

	FklGrammer** g;
	FklHashTable* builtin_prods;
	FklGrammerIgnore** builtin_ignores;
	FklHashTable* unnamed_prods;
	FklGrammerIgnore** unnamed_ignores;
	FklHashTable* named_prod_groups;

	uint32_t builtinSymbolNum;
	uint8_t* builtinSymModiMark;
	FklCodegenEnv* globalEnv;
	FklSymbolTable* globalSymTable;

	size_t exportNum;
	FklSid_t* exports;
	FklSid_t* exports_in_pst;

	FklCodegenMacro* export_macro;
	FklHashTable* export_replacement;

	FklHashTable* export_named_prod_groups;

	FklPtrStack* loadedLibStack;
	FklPtrStack* macroLibStack;

	FklFuncPrototypes* pts;

	unsigned int destroyAbleMark:1;
	unsigned int libMark:1;
	unsigned int macroMark:1;
	unsigned long refcount:61;
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

typedef struct FklCodegenErrorState
{
	FklSid_t fid;
	FklBuiltInErrorType type;
	FklNastNode* place;
	size_t line;
}FklCodegenErrorState;

typedef FklByteCodelnt* (*FklByteCodeProcesser)(FklCodegen*
		,FklCodegenEnv*
		,uint32_t scope
		,FklCodegenMacroScope*
		,FklCodegenQuestContext* context
		,FklSid_t
		,uint64_t
		,FklCodegenErrorState* errorState
		,FklCodegenOuterCtx* outer_ctx);

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
		,int destroyAbleMark
		,FklCodegenOuterCtx* outer_ctx);

void fklInitCodegener(FklCodegen* codegen
		,const char* filename
		,FklCodegenEnv* env
		,FklCodegen* prev
		,FklSymbolTable* globalSymTable
		,int destroyAbleMark
		,int libMark
		,int macroMark
		,FklCodegenOuterCtx* outer_ctx);

void fklUninitCodegener(FklCodegen* codegen);
void fklDestroyCodegener(FklCodegen* codegen);
FklCodegen* fklCreateCodegener(void);

void fklInitCodegenOuterCtx(FklCodegenOuterCtx* ctx);

void fklUninitCodegenOuterCtx(FklCodegenOuterCtx* ctx);
FklByteCode* fklCodegenNode(const FklNastNode*,FklCodegen* codegen);
FklByteCodelnt* fklGenExpressionCode(FklNastNode* exp
		,FklCodegenEnv* globalEnv
		,FklCodegen* codegen);
FklByteCodelnt* fklGenExpressionCodeWithQuest(FklCodegenQuest*
		,FklCodegen* codegen);
FklByteCodelnt* fklGenExpressionCodeWithFp(FILE*
		,FklCodegen* codegen);

FklSymbolDef* fklFindSymbolDefByIdAndScope(FklSid_t id,uint32_t scope,FklCodegenEnv* env);

void fklPrintUndefinedRef(const FklCodegenEnv* env,FklSymbolTable* globalSymTable,FklSymbolTable* pst);

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
		,FklCodegen* codegen
		,FklByteCodelnt* bcl
		,FklCodegenEnv* env);

typedef void (*FklCodegenDllLibInitExportFunc)(size_t*,FklSid_t**,FklSymbolTable* table);

FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(FklDllHandle dll);

void fklInitCodegenDllLib(FklCodegenLib* lib
		,char* rp
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init
		,FklSymbolTable* pst);

void fklDestroyCodegenLibMacroScope(FklCodegenLib* lib);
void fklUninitCodegenLib(FklCodegenLib*);
void fklUninitCodegenLibInfo(FklCodegenLib*);

FklCodegenLib* fklCreateCodegenScriptLib(FklCodegen* codegen
		,FklByteCodelnt* bcl
		,FklCodegenEnv* env);

FklCodegenLib* fklCreateCodegenDllLib(char* rp
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init
		,FklSymbolTable* pst);

void fklDestroyCodegenLib(FklCodegenLib*);

FklCodegenMacro* fklCreateCodegenMacro(FklNastNode* pattern
		,FklByteCodelnt* bcl
		,FklCodegenMacro* next
		,FklFuncPrototypes* pts
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
		,FklFuncPrototypes* pts
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklPtrStack* macroLibStack
		,FklNastNode** pr
		,uint64_t curline
		,FklSymbolTable* pst);

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
		,struct FklVM* vm
		,int needCopy
		,FklFuncPrototypes*);

void fklInitVMlibWithCodgenLibAndDestroy(FklCodegenLib* clib
		,struct FklVMlib* vlib
		,struct FklVM* vm
		,FklFuncPrototypes*);

void fklInitFrameToReplFrame(FklVM*,FklCodegen* codegen);

#ifdef __cplusplus
}
#endif
#endif
