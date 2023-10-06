#include<fakeLisp/codegen.h>

static inline FklSymbolDef* get_def_by_id_in_scope(FklSid_t id,uint32_t scopeId,FklCodegenEnvScope* scope)
{
	FklSidScope key={id,scopeId};
	return fklGetHashItem(&key,&scope->defs);
}

FklSymbolDef* fklFindSymbolDefByIdAndScope(FklSid_t id,uint32_t scope,FklCodegenEnv* env)
{
	FklCodegenEnvScope* scopes=env->scopes;
	for(;scope;scope=scopes[scope-1].p)
	{
		FklSymbolDef* r=get_def_by_id_in_scope(id,scope,&scopes[scope-1]);
		if(r)
			return r;
	}
	return NULL;
}

inline void fklPrintCodegenError(FklNastNode* obj
		,FklBuiltInErrorType type
		,FklSid_t sid
		,FklSymbolTable* symbolTable
		,size_t line
		,FklSymbolTable* publicSymbolTable)
{
	if(type==FKL_ERR_DUMMY)
		return;
	fprintf(stderr,"error in compiling: ");
	switch(type)
	{
		case FKL_ERR_CIR_REF:
			fprintf(stderr,"Circular reference occur in expanding macro");
			break;
		case FKL_ERR_SYMUNDEFINE:
			fprintf(stderr,"Symbol ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			fprintf(stderr," is undefined");
			break;
		case FKL_ERR_SYNTAXERROR:
			fprintf(stderr,"Invalid syntax ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_INVALIDEXPR:
			fprintf(stderr,"Invalid expression");
			if(obj!=NULL)
			{
				fputc(' ',stderr);
				fklPrintNastNode(obj,stderr,publicSymbolTable);
			}
			break;
		case FKL_ERR_CIRCULARLOAD:
			fprintf(stderr,"Circular load file ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_INVALIDPATTERN:
			fprintf(stderr,"Invalid string match pattern ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_INVALID_MACRO_PATTERN:
			fprintf(stderr,"Invalid macro pattern ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_MACROEXPANDFAILED:
			fprintf(stderr,"Failed to expand macro in ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_LIBUNDEFINED:
			fprintf(stderr,"Library ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			fprintf(stderr," undefined");
			break;
		case FKL_ERR_FILEFAILURE:
			fprintf(stderr,"Failed for file:");
			fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_IMPORTFAILED:
			fprintf(stderr,"Failed for importing module:");
			fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_UNEXPECTED_EOF:
			fprintf(stderr,"Unexpected eof");
			break;
		case FKL_ERR_IMPORT_MISSING:
			fprintf(stderr,"Missing import for ");
			fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP:
			fputs("Exporting production groups with reference to other group",stderr);
			break;
		case FKL_ERR_IMPORT_READER_MACRO_ERROR:
			fputs("Failed to import reader macro",stderr);
			break;
		default:
			fprintf(stderr,"Unknown compiling error.");
			break;
	}
	if(obj)
	{
		if(sid)
		{
			fprintf(stderr," at line %lu of file ",obj->curline);
			fklPrintString(fklGetSymbolWithId(sid,symbolTable)->symbol,stderr);
			fputc('\n',stderr);
		}
		else
			fprintf(stderr," at line %lu\n",obj->curline);
	}
	else
	{
		fprintf(stderr," at line %lu of file ",line);
		fklPrintString(fklGetSymbolWithId(sid,symbolTable)->symbol,stderr);
		fputc('\n',stderr);
	}
	if(obj)
		fklDestroyNastNode(obj);
}

#define INIT_SYMBOL_DEF(ID,SCOPE,IDX) {{ID,SCOPE},IDX,IDX,0}

uint32_t fklAddCodegenBuiltinRefBySid(FklSid_t id,FklCodegenEnv* env)
{
	FklHashTable* ht=&env->refs;
	uint32_t idx=ht->num;
	FklSymbolDef def=INIT_SYMBOL_DEF(id,env->pscope,idx);
	FklSymbolDef* el=fklGetOrPutHashItem(&def,ht);
	return el->idx;
}

static inline FklSymbolDef* has_outer_def(FklCodegenEnv* cur
		,FklSid_t id
		,uint32_t scope
		,FklCodegenEnv** targetEnv)
{
	FklSymbolDef* def=NULL;
	for(;cur;cur=cur->prev)
	{
		def=fklFindSymbolDefByIdAndScope(id,scope,cur);
		if(def)
		{
			*targetEnv=cur;
			return def;
		}
		scope=cur->pscope;
	}
	return NULL;
}

static void initSymbolDef(FklSymbolDef* def,uint32_t idx)
{
	def->idx=idx;
	def->cidx=idx;
	def->isLocal=0;
}

static inline FklSymbolDef* add_ref_to_all_penv(FklSid_t id
		,FklCodegenEnv* cur
		,FklCodegenEnv* targetEnv)
{
	uint32_t idx=cur->refs.num;
	FklSymbolDef def=INIT_SYMBOL_DEF(id,cur->pscope,idx);
	FklSymbolDef* cel=fklGetOrPutHashItem(&def,&cur->refs);
	for(cur=cur->prev;cur!=targetEnv;cur=cur->prev)
	{
		uint32_t idx=cur->refs.num;
		def.k.scope=cur->pscope;
		initSymbolDef(&def,idx);
		FklSymbolDef* nel=fklGetOrPutHashItem(&def,&cur->refs);
		cel->cidx=nel->idx;
		cel=nel;
	}
	return cel;
}

static inline FklSymbolDef* has_outer_ref(FklCodegenEnv* cur
		,FklSid_t id
		,FklCodegenEnv** targetEnv)
{
	FklSymbolDef* ref=NULL;
	FklSidScope key={id,0};
	for(;cur;cur=cur->prev)
	{
		key.scope=cur->pscope;
		ref=fklGetHashItem(&key,&cur->refs);
		if(ref)
		{
			*targetEnv=cur;
			return ref;
		}
	}
	return NULL;
}

static inline int is_ref_solved(FklSymbolDef* ref,FklCodegenEnv* env)
{
	if(env)
	{
		uint32_t top=env->uref.top;
		FklUnReSymbolRef** refs=(FklUnReSymbolRef**)env->uref.base;
		for(uint32_t i=0;i<top;i++)
		{
			FklUnReSymbolRef* cur=refs[i];
			if(cur->id==ref->k.id&&cur->scope==ref->k.scope)
				return 0;
		}
	}
	return 1;
}

static FklUnReSymbolRef* createUnReSymbolRef(FklSid_t id
		,uint32_t idx
		,uint32_t scope
		,uint32_t prototypeId
		,FklSid_t fid
		,uint64_t line)
{
	FklUnReSymbolRef* r=(FklUnReSymbolRef*)malloc(sizeof(FklUnReSymbolRef));
	FKL_ASSERT(r);
	r->id=id;
	r->idx=idx;
	r->scope=scope,
	r->prototypeId=prototypeId;
	r->fid=fid;
	r->line=line;
	return r;
}

uint32_t fklAddCodegenRefBySid(FklSid_t id,FklCodegenEnv* env,FklSid_t fid,uint64_t line)
{
	FklHashTable* ht=&env->refs;
	FklSidScope key={id,env->pscope};
	FklSymbolDef* el=fklGetHashItem(&key,ht);
	if(el)
		return el->idx;
	else
	{
		uint32_t idx=ht->num;
		FklCodegenEnv* prev=env->prev;
		if(prev)
		{
			FklCodegenEnv* targetEnv=NULL;
			FklSymbolDef* targetDef=has_outer_def(prev,id,env->pscope,&targetEnv);
			if(targetDef)
			{
				FklSymbolDef* cel=add_ref_to_all_penv(id,env,targetEnv);
				cel->isLocal=1;
				cel->cidx=targetDef->idx;
				targetEnv->slotFlags[targetDef->idx]=FKL_CODEGEN_ENV_SLOT_REF;
			}
			else
			{
				FklSymbolDef* targetRef=has_outer_ref(prev
						,id
						,&targetEnv);
				if(targetRef&&is_ref_solved(targetRef,targetEnv->prev))
					add_ref_to_all_penv(id,env,targetEnv->prev);
				else
				{
					FklSymbolDef def=INIT_SYMBOL_DEF(id,env->pscope,idx);
					FklSymbolDef* cel=fklGetOrPutHashItem(&def,ht);
					cel->cidx=prev->refs.num;
					FklUnReSymbolRef* unref=createUnReSymbolRef(id,idx,def.k.scope,env->prototypeId,fid,line);
					fklPushPtrStack(unref,&prev->uref);
				}
			}
		}
		else
		{
			FklSymbolDef def=INIT_SYMBOL_DEF(id,0,idx);
			FklSymbolDef* cel=fklGetOrPutHashItem(&def,ht);
			idx=cel->idx;
			FklUnReSymbolRef* unref=createUnReSymbolRef(id,idx,0,env->prototypeId,fid,line);
			fklPushPtrStack(unref,&env->uref);
		}
		return idx;
	}
}

int fklIsSymbolDefined(FklSid_t id,uint32_t scope,FklCodegenEnv* env)
{
	return get_def_by_id_in_scope(id,scope,&env->scopes[scope-1])!=NULL;
}

static inline int has_resolvable_ref(FklSid_t id,uint32_t scope,FklCodegenEnv* env)
{
	FklUnReSymbolRef** urefs=(FklUnReSymbolRef**)env->uref.base;
	uint32_t top=env->uref.top;
	for(uint32_t i=0;i<top;i++)
	{
		FklUnReSymbolRef* cur=urefs[i];
		if(cur->id==id&&cur->scope==scope)
			return 1;
	}
	return 0;
}

static inline uint32_t get_next_empty(uint32_t empty,uint8_t* flags,uint32_t lcount)
{
	for(;empty<lcount&&flags[empty];empty++);
	return empty;
}

uint32_t fklAddCodegenDefBySid(FklSid_t id,uint32_t scopeId,FklCodegenEnv* env)
{
	FklCodegenEnvScope* scope=&env->scopes[scopeId-1];
	FklHashTable* ht=&scope->defs;
	FklSidScope key={id,scopeId};
	FklSymbolDef* el=fklGetHashItem(&key,ht);
	if(!el)
	{
		uint32_t idx=scope->empty;
		el=fklPutHashItem(&key,ht);
		if(idx<env->lcount&&has_resolvable_ref(id,scopeId,env))
			idx=env->lcount;
		else
			scope->empty=get_next_empty(scope->empty+1,env->slotFlags,env->lcount);
		el->idx=idx;
		uint32_t end=(idx+1)-scope->start;
		if(scope->end<end)
			scope->end=end;
		if(idx>=env->lcount)
		{
			env->lcount=idx+1;
			uint8_t* slotFlags=(uint8_t*)fklRealloc(env->slotFlags,sizeof(uint8_t)*env->lcount);
			FKL_ASSERT(slotFlags);
			env->slotFlags=slotFlags;
		}
		env->slotFlags[idx]=FKL_CODEGEN_ENV_SLOT_OCC;
	}
	return el->idx;
}

static inline void replace_sid(FklSid_t* id,const FklSymbolTable* origin_st,FklSymbolTable* target_st)
{
	FklSid_t sid=*id;
	*id=fklAddSymbol(fklGetSymbolWithId(sid,origin_st)->symbol,target_st)->id;
}

static inline void recompute_sid_for_bytecodelnt(FklByteCodelnt* bcl
		,const FklSymbolTable* origin_st
		,FklSymbolTable* target_st)
{
	FklByteCode* bc=bcl->bc;
	FklInstruction* start=bc->code;
	const FklInstruction* end=start+bc->len;
	for(;start<end;start++)
		if(start->op==FKL_OP_PUSH_SYM)
			replace_sid(&start->sid,origin_st,target_st);

	FklLineNumberTableItem* lnt_start=bcl->l;
	FklLineNumberTableItem* lnt_end=lnt_start+bcl->ls;
	for(;lnt_start<lnt_end;lnt_start++)
		replace_sid(&lnt_start->fid,origin_st,target_st);
}

static inline void recompute_sid_for_nast(FklNastNode* node
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	FklPtrStack pending;
	fklInitPtrStack(&pending,16,16);
	fklPushPtrStack(node,&pending);
	while(!fklIsPtrStackEmpty(&pending))
	{
		FklNastNode* top=fklPopPtrStack(&pending);
		switch(top->type)
		{
			case FKL_NAST_SYM:
			case FKL_NAST_SLOT:
				replace_sid(&top->sym,origin_table,target_table);
				break;
			case FKL_NAST_BOX:
				fklPushPtrStack(top->box,&pending);
				break;
			case FKL_NAST_PAIR:
				fklPushPtrStack(top->pair->car,&pending);
				fklPushPtrStack(top->pair->cdr,&pending);
				break;
			case FKL_NAST_VECTOR:
				for(size_t i=0;i<top->vec->size;i++)
					fklPushPtrStack(top->vec->base[i],&pending);
				break;
			case FKL_NAST_HASHTABLE:
				for(size_t i=0;i<top->hash->num;i++)
				{
					fklPushPtrStack(top->hash->items[i].car,&pending);
					fklPushPtrStack(top->hash->items[i].cdr,&pending);
				}
				break;
			default:
				break;
		}
	}

	fklUninitPtrStack(&pending);
}

static inline void recompute_sid_for_prototypes(FklFuncPrototypes* pts
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	uint32_t end=pts->count+1;

	uint32_t builtin_symbol_num=fklGetBuiltinSymbolNum();
	for(uint32_t i=1;i<end;i++)
	{
		FklFuncPrototype* cur=&pts->pts[i];
		replace_sid(&cur->fid,origin_table,target_table);
		if(cur->sid)
			replace_sid(&cur->sid,origin_table,target_table);

		for(uint32_t i=cur->is_top?builtin_symbol_num:0;i<cur->rcount;i++)
			replace_sid(&cur->refs[i].k.id,origin_table,target_table);
	}
}

static inline void recompute_sid_for_export_sid_index(FklHashTable* t
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	for(FklHashTableItem* sid_idx_list=t->first;sid_idx_list;sid_idx_list=sid_idx_list->next)
	{
		FklCodegenExportSidIndexHashItem* sid_idx=(FklCodegenExportSidIndexHashItem*)sid_idx_list->data;
		replace_sid(&sid_idx->sid,origin_table,target_table);
	}
}

static inline void recompute_sid_for_compiler_macros(FklCodegenMacro* m
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	for(;m;m=m->next)
	{
		recompute_sid_for_nast(m->origin_exp,origin_table,target_table);
		recompute_sid_for_bytecodelnt(m->bcl,origin_table,target_table);
	}
}

static inline void recompute_sid_for_replacements(FklHashTable* t
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	for(FklHashTableItem* rep_list=t->first;rep_list;rep_list=rep_list->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)rep_list->data;
		replace_sid(&rep->id,origin_table,target_table);
		recompute_sid_for_nast(rep->node,origin_table,target_table);
	}
}

static inline void recompute_sid_for_script_lib(FklCodegenLib* lib
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	recompute_sid_for_bytecodelnt(lib->bcl,origin_table,target_table);
	recompute_sid_for_export_sid_index(&lib->exports,origin_table,target_table);
	recompute_sid_for_compiler_macros(lib->head,origin_table,target_table);
	recompute_sid_for_replacements(lib->replacements,origin_table,target_table);
}

static inline void recompute_sid_for_dll_lib(FklCodegenLib* lib
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	recompute_sid_for_export_sid_index(&lib->exports,origin_table,target_table);
}

static inline void recompute_sid_for_lib_stack(FklPtrStack* loadedLibStack
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	for(size_t i=0;i<loadedLibStack->top;i++)
	{
		FklCodegenLib* lib=loadedLibStack->base[i];
		switch(lib->type)
		{
			case FKL_CODEGEN_LIB_SCRIPT:
				recompute_sid_for_script_lib(lib,origin_table,target_table);
				break;
			case FKL_CODEGEN_LIB_DLL:
				recompute_sid_for_dll_lib(lib,origin_table,target_table);
				break;
		}
	}
}

static inline void recompute_sid_for_double_st_script_lib(FklCodegenLib* lib
		,const FklSymbolTable* origin_table
		,FklSymbolTable* gst
		,FklSymbolTable* pst)
{
	recompute_sid_for_bytecodelnt(lib->bcl,origin_table,gst);
	recompute_sid_for_export_sid_index(&lib->exports,origin_table,pst);
	recompute_sid_for_compiler_macros(lib->head,origin_table,pst);
	recompute_sid_for_replacements(lib->replacements,origin_table,pst);
}

static inline void recompute_sid_for_double_st_lib_stack(FklPtrStack* loadedLibStack
		,const FklSymbolTable* origin_table
		,FklSymbolTable* gst
		,FklSymbolTable* pst)
{
	for(size_t i=0;i<loadedLibStack->top;i++)
	{
		FklCodegenLib* lib=loadedLibStack->base[i];
		switch(lib->type)
		{
			case FKL_CODEGEN_LIB_SCRIPT:
				recompute_sid_for_double_st_script_lib(lib,origin_table,gst,pst);
				break;
			case FKL_CODEGEN_LIB_DLL:
				recompute_sid_for_dll_lib(lib,origin_table,pst);
				break;
		}
	}
}


static inline void recompute_sid_for_main_file(FklCodegenInfo* codegen
		,FklByteCodelnt* bcl
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	recompute_sid_for_bytecodelnt(bcl,origin_table,target_table);
	recompute_sid_for_export_sid_index(&codegen->exports,origin_table,target_table);
	recompute_sid_for_compiler_macros(codegen->export_macro,origin_table,target_table);
	recompute_sid_for_replacements(codegen->export_replacement,origin_table,target_table);
}

inline void fklRecomputeSidForSingleTableInfo(FklCodegenInfo* codegen
		,FklByteCodelnt* bcl
		,const FklSymbolTable* origin_table
		,FklSymbolTable* target_table)
{
	recompute_sid_for_prototypes(codegen->pts,origin_table,target_table);
	recompute_sid_for_prototypes(codegen->macro_pts,origin_table,target_table);
	recompute_sid_for_main_file(codegen,bcl,origin_table,target_table);
	recompute_sid_for_lib_stack(codegen->libStack,origin_table,target_table);
	recompute_sid_for_lib_stack(codegen->macroLibStack,origin_table,target_table);
}

static void _codegen_replacement_uninitItem(void* item)
{
	FklCodegenReplacement* replace=(FklCodegenReplacement*)item;
	fklDestroyNastNode(replace->node);
}

static int _codegen_replacement_keyEqual(const void* pkey0,const void* pkey1)
{
	FklSid_t k0=*(const FklSid_t*)pkey0;
	FklSid_t k1=*(const FklSid_t*)pkey1;
	return k0==k1;
}

static void _codegen_replacement_setVal(void* d0,const void* d1)
{
	*(FklCodegenReplacement*)d0=*(const FklCodegenReplacement*)d1;
	fklMakeNastNodeRef(((FklCodegenReplacement*)d0)->node);
}

static void _codegen_replacement_setKey(void* k0,const void* k1)
{
	*(FklSid_t*)k0=*(const FklSid_t*)k1;
}

static FklHashTableMetaTable CodegenReplacementHashMetaTable=
{
	.size=sizeof(FklCodegenReplacement),
	.__setKey=_codegen_replacement_setKey,
	.__setVal=_codegen_replacement_setVal,
	.__hashFunc=fklSidHashFunc,
	.__uninitItem=_codegen_replacement_uninitItem,
	.__keyEqual=_codegen_replacement_keyEqual,
	.__getKey=fklHashDefaultGetKey,
};

FklHashTable* fklCreateCodegenReplacementTable(void)
{
	return fklCreateHashTable(&CodegenReplacementHashMetaTable);
}

static void export_sid_index_item_set_val(void* d0,const void* d1)
{
	*(FklCodegenExportSidIndexHashItem*)d0=*(const FklCodegenExportSidIndexHashItem*)d1;
}

static const FklHashTableMetaTable export_sid_index_meta_table=
{
	.size=sizeof(FklCodegenExportSidIndexHashItem),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=fklSetSidKey,
	.__setVal=export_sid_index_item_set_val,
	.__hashFunc=fklSidHashFunc,
	.__keyEqual=fklSidKeyEqual,
	.__uninitItem=fklDoNothingUninitHashItem,
};

void fklInitExportSidIdxTable(FklHashTable* ht)
{
	fklInitHashTable(ht,&export_sid_index_meta_table);
}

#include<string.h>
#include<fakeLisp/parser.h>

static inline char* load_script_lib_path(const char* main_dir,FILE* fp)
{
	FklStringBuffer buf;
	fklInitStringBuffer(&buf);
	fklStringBufferConcatWithCstr(&buf,main_dir);
	int ch=fgetc(fp);
	for(;;)
	{
		while(ch)
		{
			fklStringBufferPutc(&buf,ch);
			ch=fgetc(fp);
		}
		ch=fgetc(fp);
		if(!ch)
			break;
		fklStringBufferPutc(&buf,FKL_PATH_SEPARATOR);
	}

	fklStringBufferPutc(&buf,FKL_PRE_COMPILE_FKL_SUFFIX);

	char* path=fklCopyCstr(buf.buf);
	fklUninitStringBuffer(&buf);
	return path;
}

static inline void load_export_sid_idx_table(FklHashTable* t,FILE* fp)
{
	fklInitExportSidIdxTable(t);
	fread(&t->num,sizeof(t->num),1,fp);
	uint32_t num=t->num;
	t->num=0;
	for(uint32_t i=0;i<num;i++)
	{
		FklCodegenExportSidIndexHashItem sid_idx;
		fread(&sid_idx.sid,sizeof(sid_idx.sid),1,fp);
		fread(&sid_idx.idx,sizeof(sid_idx.idx),1,fp);
		fklGetOrPutHashItem(&sid_idx,t);
	}
}

static inline FklNastNode* load_nast_node_with_null_chr(FklSymbolTable* st,FILE* fp)
{
	FklStringBuffer buf;
	fklInitStringBuffer(&buf);
	char ch=0;
	while((ch=fgetc(fp)))
		fklStringBufferPutc(&buf,ch);
	FklNastNode* node=fklCreateNastNodeFromCstr(buf.buf,st);
	fklUninitStringBuffer(&buf);
	return node;
}

static inline FklCodegenMacro* load_compiler_macros(FklSymbolTable* st,FILE* fp)
{
	uint64_t count=0;
	fread(&count,sizeof(count),1,fp);
	FklCodegenMacro* r=NULL;
	FklCodegenMacro** pr=&r;
	for(uint64_t i=0;i<count;i++)
	{
		FklNastNode* node=load_nast_node_with_null_chr(st,fp);
		uint32_t prototype_id=0;
		fread(&prototype_id,sizeof(prototype_id),1,fp);
		FklByteCodelnt* bcl=fklLoadByteCodelnt(fp);
		FklNastNode* pattern=fklCreatePatternFromNast(node,NULL);
		FklCodegenMacro* cur=fklCreateCodegenMacro(pattern
				,node
				,bcl
				,NULL
				,prototype_id
				,1);
		*pr=cur;
		pr=&cur->next;
	}
	return r;
}

static inline FklHashTable* load_replacements(FklSymbolTable* st,FILE* fp)
{
	FklHashTable* ht=fklCreateCodegenReplacementTable();
	fread(&ht->num,sizeof(ht->num),1,fp);
	uint32_t num=ht->num;
	ht->num=0;
	for(uint32_t i=0;i<num;i++)
	{
		FklSid_t id=0;
		fread(&id,sizeof(id),1,fp);
		FklNastNode* node=load_nast_node_with_null_chr(st,fp);
		FklCodegenReplacement* rep=fklPutHashItem(&id,ht);
		rep->node=node;
	}
	return ht;
}

static inline void load_script_lib_from_pre_compile(FklCodegenLib* lib,FklSymbolTable* st,const char* main_dir,FILE* fp)
{
	lib->rp=load_script_lib_path(main_dir,fp);
	fread(&lib->prototypeId,sizeof(lib->prototypeId),1,fp);
	lib->bcl=fklLoadByteCodelnt(fp);
	load_export_sid_idx_table(&lib->exports,fp);
	lib->head=load_compiler_macros(st,fp);
	lib->replacements=load_replacements(st,fp);
	lib->named_prod_groups.t=NULL;
}

static inline char* load_dll_lib_path(const char* main_dir,FILE* fp)
{
	FklStringBuffer buf;
	fklInitStringBuffer(&buf);
	fklStringBufferConcatWithCstr(&buf,main_dir);
	int ch=fgetc(fp);
	for(;;)
	{
		while(ch)
		{
			fklStringBufferPutc(&buf,ch);
			ch=fgetc(fp);
		}
		ch=fgetc(fp);
		if(!ch)
			break;
		fklStringBufferPutc(&buf,FKL_PATH_SEPARATOR);
	}

	fklStringBufferConcatWithCstr(&buf,FKL_DLL_FILE_TYPE);

	char* path=fklCopyCstr(buf.buf);
	fklUninitStringBuffer(&buf);
	char* rp=fklRealpath(path);
	free(path);
	return rp;
}

static inline int load_dll_lib_from_pre_compile(FklCodegenLib* lib,FklSymbolTable* st,const char* main_dir,FILE* fp)
{
	lib->rp=load_dll_lib_path(main_dir,fp);
	if(!lib->rp||!fklIsAccessableRegFile(lib->rp))
	{
		free(lib->rp);
		return 1;
	}
	lib->dll=fklLoadDll(lib->rp);
	load_export_sid_idx_table(&lib->exports,fp);
	return 0;
}

static inline FklCodegenLib* load_lib_from_pre_compile(FklSymbolTable* st,const char* main_dir,FILE* fp)
{
	FklCodegenLib* lib=(FklCodegenLib*)calloc(1,sizeof(FklCodegenLib));
	FKL_ASSERT(lib);
	uint8_t type=0;
	fread(&type,sizeof(type),1,fp);
	lib->type=type;
	switch(lib->type)
	{
		case FKL_CODEGEN_LIB_SCRIPT:
			load_script_lib_from_pre_compile(lib,st,main_dir,fp);
			break;
		case FKL_CODEGEN_LIB_DLL:
			if(load_dll_lib_from_pre_compile(lib,st,main_dir,fp))
			{
				free(lib);
				return NULL;
			}
			break;
	}
	return lib;
}

static inline int load_imported_lib_stack(FklPtrStack* libStack,FklSymbolTable* st,const char* main_dir,FILE* fp)
{
	uint64_t num=0;
	fread(&num,sizeof(num),1,fp);
	fklInitPtrStack(libStack,num+1,16);
	for(size_t i=0;i<num;i++)
	{
		FklCodegenLib* lib=load_lib_from_pre_compile(st,main_dir,fp);
		if(!lib)
			return 1;
		fklPushPtrStack(lib,libStack);
	}
	FklCodegenLib* main_lib=(FklCodegenLib*)malloc(sizeof(FklCodegenLib));
	FKL_ASSERT(main_lib);
	main_lib->named_prod_groups.t=NULL;
	main_lib->type=FKL_CODEGEN_LIB_SCRIPT;
	main_lib->rp=load_script_lib_path(main_dir,fp);
	main_lib->bcl=fklLoadByteCodelnt(fp);
	main_lib->prototypeId=1;
	load_export_sid_idx_table(&main_lib->exports,fp);
	main_lib->head=load_compiler_macros(st,fp);
	main_lib->replacements=load_replacements(st,fp);
	fklPushPtrStack(main_lib,libStack);
	return 0;
}

static inline int load_macro_lib_stack(FklPtrStack* libStack
		,FklSymbolTable* st
		,const char* main_dir
		,FILE* fp)
{
	uint64_t num=0;
	fread(&num,sizeof(num),1,fp);
	fklInitPtrStack(libStack,num,16);
	for(size_t i=0;i<num;i++)
	{
		FklCodegenLib* lib=load_lib_from_pre_compile(st,main_dir,fp);
		if(!lib)
			return 1;
		fklPushPtrStack(lib,libStack);
	}
	return 0;
}

static inline void increase_bcl_lib_prototype_id(FklByteCodelnt* bcl,uint32_t lib_count,uint32_t pts_count)
{
	FklInstruction* ins=bcl->bc->code;
	const FklInstruction* end=ins+bcl->bc->len;
	for(;ins<end;ins++)
	{
		switch(ins->op)
		{
			case FKL_OP_LOAD_DLL:
			case FKL_OP_LOAD_LIB:
			case FKL_OP_EXPORT:
				ins->imm_u32+=lib_count;
				break;
			case FKL_OP_PUSH_PROC:
				ins->imm+=pts_count;
				break;
			default:
				break;
		}
	}
}

static inline void increase_compiler_macros_lib_prototype_id(FklCodegenMacro* head
		,uint32_t macro_lib_count
		,uint32_t macro_pts_count)
{
	for(;head;head=head->next)
	{
		head->prototype_id+=macro_pts_count;
		increase_bcl_lib_prototype_id(head->bcl,macro_lib_count,macro_pts_count);
	}
}

static inline void increase_lib_id_and_prototype_id(FklCodegenLib* lib
		,uint32_t lib_count
		,uint32_t macro_lib_count
		,uint32_t pts_count
		,uint32_t macro_pts_count)
{
	switch(lib->type)
	{
		case FKL_CODEGEN_LIB_SCRIPT:
			lib->prototypeId+=pts_count;
			increase_bcl_lib_prototype_id(lib->bcl,lib_count,pts_count);
			increase_compiler_macros_lib_prototype_id(lib->head,macro_lib_count,macro_pts_count);
			break;
		case FKL_CODEGEN_LIB_DLL:
			break;
	}
}

static inline void increase_prototype_and_lib_id(uint32_t pts_count
		,uint32_t macro_pts_count
		,uint32_t lib_count
		,uint32_t macro_lib_count
		,FklPtrStack* libStack
		,FklPtrStack* macroLibStack)
{
	uint32_t top=libStack->top;
	FklCodegenLib** base=(FklCodegenLib**)libStack->base;
	for(uint32_t i=0;i<top;i++)
	{
		FklCodegenLib* lib=base[i];
		increase_lib_id_and_prototype_id(lib
				,lib_count
				,macro_lib_count
				,pts_count
				,macro_pts_count);
	}

	top=macroLibStack->top;
	base=(FklCodegenLib**)macroLibStack->base;
	for(uint32_t i=0;i<top;i++)
	{
		FklCodegenLib* lib=base[i];
		increase_lib_id_and_prototype_id(lib
				,macro_lib_count
				,macro_lib_count
				,pts_count
				,macro_pts_count);
	}
}

static inline void merge_prototypes(FklFuncPrototypes* o,const FklFuncPrototypes* pts)
{
	uint32_t o_count=o->count;
	o->count+=pts->count;
	FklFuncPrototype* pa=(FklFuncPrototype*)fklRealloc(o->pts,sizeof(FklFuncPrototype)*(o->count+1));
	FKL_ASSERT(pa);
	o->pts=pa;
	uint32_t i=o_count+1;
	memcpy(&pa[i],&pts->pts[1],sizeof(FklFuncPrototype)*pts->count);
	uint32_t end=o->count+1;
	for(;i<end;i++)
	{
		FklFuncPrototype* cur=&pa[i];
		cur->refs=fklCopyMemory(cur->refs,sizeof(FklSymbolDef)*cur->rcount);
	}
}

inline int fklLoadPreCompile(FklFuncPrototypes* info_pts
		,FklFuncPrototypes* info_macro_pts
		,FklPtrStack* info_scriptLibStack
		,FklPtrStack* info_macroScriptLibStack
		,FklSymbolTable* gst
		,FklSymbolTable* pst
		,const char* rp
		,FILE* fp)
{
	int need_open=fp==NULL;
	if(need_open)
	{
		fp=fopen(rp,"rb");
		if(fp==NULL)
			return 1;
	}
	int err=0;
	FklSymbolTable ost;
	char* main_dir=fklGetDir(rp);
	main_dir=fklStrCat(main_dir,FKL_PATH_SEPARATOR_STR);

	FklPtrStack scriptLibStack=FKL_STACK_INIT;
	FklPtrStack macroScriptLibStack=FKL_STACK_INIT;

	fklInitSymbolTable(&ost);
	fklLoadSymbolTable(fp,&ost);

	uint32_t builtin_symbol_num=fklGetBuiltinSymbolNum();
	FklFuncPrototypes* pts=NULL;
	FklFuncPrototypes* macro_pts=NULL;

	pts=fklLoadFuncPrototypes(builtin_symbol_num,fp);

	if(load_imported_lib_stack(&scriptLibStack,&ost,main_dir,fp))
		goto error;

	macro_pts=fklLoadFuncPrototypes(builtin_symbol_num,fp);
	if(load_macro_lib_stack(&macroScriptLibStack,&ost,main_dir,fp))
	{
		while(!fklIsPtrStackEmpty(&macroScriptLibStack))
			fklDestroyCodegenLib(fklPopPtrStack(&macroScriptLibStack));
		fklUninitPtrStack(&macroScriptLibStack);
error:
		while(!fklIsPtrStackEmpty(&scriptLibStack))
			fklDestroyCodegenLib(fklPopPtrStack(&scriptLibStack));
		err=1;
		goto exit;
	}

	recompute_sid_for_prototypes(pts,&ost,gst);
	recompute_sid_for_prototypes(macro_pts,&ost,pst);

	recompute_sid_for_double_st_lib_stack(&scriptLibStack,&ost,gst,pst);
	recompute_sid_for_lib_stack(&macroScriptLibStack,&ost,pst);

	increase_prototype_and_lib_id(info_pts->count
			,info_macro_pts->count
			,info_scriptLibStack->top
			,info_macroScriptLibStack->top
			,&scriptLibStack
			,&macroScriptLibStack);

	for(uint32_t i=0;i<scriptLibStack.top;i++)
		fklPushPtrStack(scriptLibStack.base[i],info_scriptLibStack);

	for(uint32_t i=0;i<macroScriptLibStack.top;i++)
		fklPushPtrStack(macroScriptLibStack.base[i],info_macroScriptLibStack);

	merge_prototypes(info_pts,pts);
	merge_prototypes(info_macro_pts,macro_pts);

	fklUninitPtrStack(&macroScriptLibStack);
exit:
	if(need_open)
		fclose(fp);
	free(main_dir);
	fklDestroyFuncPrototypes(pts);
	if(macro_pts)
		fklDestroyFuncPrototypes(macro_pts);
	fklUninitSymbolTable(&ost);
	fklUninitPtrStack(&scriptLibStack);
	return err;
}

