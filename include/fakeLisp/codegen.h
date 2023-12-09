#ifndef FKL_CODEGEN_H
#define FKL_CODEGEN_H

#include"symbol.h"
#include"pattern.h"
#include"grammer.h"
#include"base.h"
#include"nast.h"
#include"bytecode.h"
#include"builtin.h"
#include"vm.h"
#include<uv.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklCodegenEnvScope
{
	uint32_t p;
	uint32_t start;
	uint32_t empty;
	uint32_t end;
	FklHashTable defs;
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
	FklHashTable refs;
	struct FklCodegenMacroScope* macros;
	size_t refcount;
}FklCodegenEnv;

void fklUpdatePrototype(FklFuncPrototypes* cp
		,FklCodegenEnv* env
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* pst);

typedef struct FklCodegenMacro
{
	FklNastNode* origin_exp;
	FklNastNode* pattern;
	uint32_t prototype_id;
	FklByteCodelnt* bcl;
	struct FklCodegenMacro* next;
	uint8_t own;
}FklCodegenMacro;

typedef struct
{
	FklSid_t id;
	FklNastNode* node;
}FklCodegenReplacement;

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

typedef struct
{
	FklSid_t sid;
	uint32_t idx;
}FklCodegenExportSidIndexHashItem;

typedef struct
{
	FklCodegenLibType type;
	union
	{
		FklByteCodelnt* bcl;
		uv_lib_t dll;
	};
	char* rp;
	FklHashTable exports;

	FklCodegenMacro* head;
	FklHashTable* replacements;

	FklHashTable named_prod_groups;
	FklSymbolTable terminal_table;
	FklRegexTable regexes;

	uint32_t prototypeId;
}FklCodegenLib;

typedef enum
{
	FKL_CODEGEN_PROD_BUILTIN=0,
	FKL_CODEGEN_PROD_SIMPLE,
	FKL_CODEGEN_PROD_CUSTOM,
}FklCodegenProdActionType;

typedef struct
{
	FklSid_t group_id;
	FklSid_t sid;
	FklNastNode* vec;
	uint8_t add_extra;
	uint8_t type;
	union
	{
		FklNastNode* forth;
		struct
		{
			uint32_t prototype_id;
			FklByteCodelnt* bcl;
		};
	};
}FklCodegenProdPrinting;

typedef struct
{
	FklSid_t id;
	int is_ref_outer;
	FklHashTable prods;
	FklGrammerIgnore* ignore;
	FklPtrStack ignore_printing;
	FklPtrStack prod_printing;
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
	FKL_CODEGEN_PATTERN_COND_COMPILE,
	FKL_CODEGEN_PATTERN_NUM,
}FklCodegenPatternEnum;

typedef enum
{
	FKL_CODEGEN_SUB_PATTERN_UNQUOTE=0,
	FKL_CODEGEN_SUB_PATTERN_UNQTESP,
	FKL_CODEGEN_SUB_PATTERN_DEFINE,
	FKL_CODEGEN_SUB_PATTERN_DEFMACRO,
	FKL_CODEGEN_SUB_PATTERN_IMPORT,
	FKL_CODEGEN_SUB_PATTERN_AND,
	FKL_CODEGEN_SUB_PATTERN_OR,
	FKL_CODEGEN_SUB_PATTERN_NOT,
	FKL_CODEGEN_SUB_PATTERN_EQ,
	FKL_CODEGEN_SUB_PATTERN_MATCH,
	FKL_CODEGEN_SUB_PATTERN_NUM,
}FklCodegenSubPatternEnum;

#define FKL_BUILTIN_REPLACEMENT_NUM (6)
#define FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM (17)
#define FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM (11)

typedef struct
{
	FklSymbolTable public_symbol_table;

	FklSid_t builtInPatternVar_orig;
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

typedef struct FklCodegenInfo
{
	FklCodegenOuterCtx* outer_ctx;

	struct FklCodegenInfo* prev;

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

	FklHashTable exports;

	FklCodegenMacro* export_macro;
	FklHashTable* export_replacement;

	FklHashTable* export_named_prod_groups;

	FklPtrStack* libStack;

	FklPtrStack* macroLibStack;

	FklFuncPrototypes* pts;
	FklFuncPrototypes* macro_pts;

	unsigned int destroyAbleMark:1;
	unsigned int libMark:1;
	unsigned int macroMark:1;
	uint64_t refcount:61;
}FklCodegenInfo;

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
	FklBuiltinErrorType type;
	FklNastNode* place;
	size_t line;
}FklCodegenErrorState;

typedef FklByteCodelnt* (*FklByteCodeProcesser)(FklCodegenInfo*
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
	uint8_t must_has_retval;
}FklCodegenNextExpression;

typedef struct FklCodegenQuest
{
	FklByteCodeProcesser processer;
	FklCodegenQuestContext* context;
	FklCodegenEnv* env;
	uint32_t scope;
	FklCodegenMacroScope* macroScope;
    uint64_t curline;
	FklCodegenInfo* codegen;
	struct FklCodegenQuest* prev;
	FklCodegenNextExpression* nextExpression;
}FklCodegenQuest;

void fklInitExportSidIdxTable(FklHashTable* ht);
FklHashTable* fklCreateCodegenReplacementTable(void);

void fklInitGlobalCodegenInfo(FklCodegenInfo* codegen
		,const char* rp
		,FklSymbolTable* globalSymTable
		,int destroyAbleMark
		,FklCodegenOuterCtx* outer_ctx);

void fklInitCodegenInfo(FklCodegenInfo* codegen
		,const char* filename
		,FklCodegenEnv* env
		,FklCodegenInfo* prev
		,FklSymbolTable* globalSymTable
		,int destroyAbleMark
		,int libMark
		,int macroMark
		,FklCodegenOuterCtx* outer_ctx);

void fklUninitCodegenInfo(FklCodegenInfo* codegen);
void fklDestroyCodegenInfo(FklCodegenInfo* codegen);

void fklInitCodegenOuterCtx(FklCodegenOuterCtx* ctx);
void fklInitCodegenOuterCtxExceptPattern(FklCodegenOuterCtx* outerCtx);

void fklUninitCodegenOuterCtx(FklCodegenOuterCtx* ctx);
FklByteCode* fklCodegenNode(const FklNastNode*,FklCodegenInfo* codegen);
FklByteCodelnt* fklGenExpressionCode(FklNastNode* exp
		,FklCodegenEnv* globalEnv
		,FklCodegenInfo* codegen);
FklByteCodelnt* fklGenExpressionCodeWithQuest(FklCodegenQuest*
		,FklCodegenInfo* codegen);
FklByteCodelnt* fklGenExpressionCodeWithFp(FILE*
		,FklCodegenInfo* codegen);

FklSymbolDef* fklFindSymbolDefByIdAndScope(FklSid_t id,uint32_t scope,const FklCodegenEnv* env);

void fklPrintCodegenError(FklNastNode* obj
		,FklBuiltinErrorType type
		,const FklCodegenInfo* info
		,const FklSymbolTable* symbolTable
		,size_t line
		,const FklSymbolTable* publicSymbolTable);

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
		,FklCodegenInfo* codegen
		,FklByteCodelnt* bcl
		,FklCodegenEnv* env);

FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(uv_lib_t* dll);

void fklInitCodegenDllLib(FklCodegenLib* lib
		,char* rp
		,uv_lib_t dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init
		,FklSymbolTable* pst);

void fklDestroyCodegenLibMacroScope(FklCodegenLib* lib);
void fklUninitCodegenLib(FklCodegenLib*);
void fklUninitCodegenLibInfo(FklCodegenLib*);

FklCodegenLib* fklCreateCodegenScriptLib(FklCodegenInfo* codegen
		,FklByteCodelnt* bcl
		,FklCodegenEnv* env);

FklCodegenLib* fklCreateCodegenDllLib(char* rp
		,uv_lib_t dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init
		,FklSymbolTable* pst);

void fklDestroyCodegenLibExceptBclAndDll(FklCodegenLib* clib);
void fklDestroyCodegenLib(FklCodegenLib*);

FklCodegenMacro* fklCreateCodegenMacro(FklNastNode* pattern
		,FklNastNode* origin_exp
		,FklByteCodelnt* bcl
		,FklCodegenMacro* next
		,uint32_t prototype_id
		,int own);
void fklDestroyCodegenMacro(FklCodegenMacro* macro);
void fklDestroyCodegenMacroList(FklCodegenMacro* macro);

FklCodegenMacroScope* fklCreateCodegenMacroScope(FklCodegenMacroScope* prev);
void fklDestroyCodegenMacroScope(FklCodegenMacroScope* c);

FklNastNode* fklTryExpandCodegenMacro(FklNastNode* exp
		,FklCodegenInfo*
		,FklCodegenMacroScope* macros
		,FklCodegenErrorState*);

FklVM* fklInitMacroExpandVM(FklByteCodelnt* bcl
		,FklFuncPrototypes* pts
		,uint32_t prototype_id
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklPtrStack* macroLibStack
		,FklNastNode** pr
		,uint64_t curline
		,FklSymbolTable* pst);

void fklInitVMlibWithCodegenLibRefs(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVM* exe
		,FklVMvalue** refs
		,uint32_t count
		,int needCopy
		,FklFuncPrototypes*);

void fklInitVMlibWithCodegenLib(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVM* vm
		,int needCopy
		,FklFuncPrototypes*);

void fklInitVMlibWithCodgenLibAndDestroy(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVM* vm
		,FklFuncPrototypes*);

void fklRecomputeSidForSingleTableInfo(FklCodegenInfo* codegen
		,FklByteCodelnt* bcl
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table);

int fklLoadPreCompile(FklFuncPrototypes* info_pts
		,FklFuncPrototypes* info_macro_pts
		,FklPtrStack* info_scriptLibStack
		,FklPtrStack* info_macroScriptLibStack
		,FklSymbolTable* gst
		,FklCodegenOuterCtx* outer_ctx
		,const char* rp
		,FILE* fp
		,char** errorStr);

void fklInitCodegenProdGroupTable(FklHashTable* ht);

void fklWriteNamedProds(const FklHashTable* named_prod_groups
		,const FklSymbolTable* st
		,FILE* fp);

FklGrammerProduction* fklCreateExtraStartProduction(FklSid_t group,FklSid_t sid);

FklGrammerIgnore* fklNastVectorToIgnore(FklNastNode* ast
		,FklSymbolTable* tt
		,FklRegexTable* rt
		,FklHashTable* builtin_terms);

FklGrammerProduction* fklCodegenProdPrintingToProduction(FklCodegenProdPrinting* p
		,FklSymbolTable* tt
		,FklRegexTable* rt
		,FklHashTable* builtin_terms
		,FklCodegenOuterCtx* outer_ctx
		,FklFuncPrototypes* pts
		,FklPtrStack* macroLibStack);

void fklWriteExportNamedProds(const FklHashTable* export_named_prod_groups
		,const FklHashTable* named_prod_groups
		,const FklSymbolTable* st
		,FILE* fp);

#ifdef __cplusplus
}
#endif
#endif
