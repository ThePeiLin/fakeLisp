#include<fakeLisp/grammer.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<ctype.h>
#include<string.h>

// lalr1
// =====

typedef struct
{
	FklSid_t left;
	FklGrammerProduction* prods;
}ProdHashItem;

static void prod_hash_set_key(void* k0,const void* k1)
{
	*(FklSid_t*)k0=*(const FklSid_t*)k1;
}

static uintptr_t prod_hash_func(const void* k)
{
	return *(const FklSid_t*)k;
}

static int prod_hash_equal(const void* k0,const void* k1)
{
	return *(const FklSid_t*)k0==*(const FklSid_t*)k1;
}

static void prod_hash_set_val(void* d0,const void* d1)
{
	*(ProdHashItem*)d0=*(const ProdHashItem*)d1;
}

static inline void destroy_prod(FklGrammerProduction* h)
{

	// if(!h->isBuiltin)
	// 	fklDestroyByteCodelnt(h->action.proc);
	free(h->syms);
	free(h);
}

static inline void destroy_builtin_grammer_sym(FklLalrBuiltinGrammerSym* s)
{
	if(s->t->ctx_destroy)
		s->t->ctx_destroy(s->c);
}

static void prod_hash_uninit(void* d)
{
	ProdHashItem* prod=(ProdHashItem*)d;
	FklGrammerProduction* h=prod->prods;
	while(h)
	{
		FklGrammerProduction* next=h->next;
		FklGrammerSym* syms=h->syms;
		size_t len=h->len;
		for(size_t i=0;i<len;i++)
		{
			FklGrammerSym* s=&syms[i];
			if(s->isbuiltin)
				destroy_builtin_grammer_sym(&s->b);
		}
		destroy_prod(h);
		h=next;
	}
	prod->prods=NULL;
}

static const FklHashTableMetaTable ProdHashMetaTable=
{
	.size=sizeof(ProdHashItem),
	.__setKey=prod_hash_set_key,
	.__setVal=prod_hash_set_val,
	.__hashFunc=prod_hash_func,
	.__uninitItem=prod_hash_uninit,
	.__keyEqual=prod_hash_equal,
	.__getKey=fklHashDefaultGetKey,
};

static inline FklGrammerProduction* create_empty_production(FklSid_t left,size_t len,const char* name,FklBuiltinProdAction func)
{
	FklGrammerProduction* r=(FklGrammerProduction*)calloc(1,sizeof(FklGrammerProduction));
	FKL_ASSERT(r);
	r->left=left;
	r->len=len;
	r->name=name;
	r->func=func;
	r->syms=(FklGrammerSym*)calloc(len,sizeof(FklGrammerSym));
	r->isBuiltin=1;
	FKL_ASSERT(r->syms);
	return r;
}

static int builtin_match_any_func(void* ctx
		,const char* start
		,const char* s
		,size_t restLen
		,size_t* matchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	if(restLen)
	{
		*matchLen=1;
		return 1;
	}
	return 0;
}

#define DEFINE_DEFAULT_C_MATCH_COND(NAME) static void builtin_match_##NAME##_print_c_match_cond(void* ctx,FILE* fp)\
{\
	fputs("builtin_match_"#NAME"(start,*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,outerCtx)",fp);\
}

static void builtin_match_any_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("",fp);
}

static void builtin_match_any_print_c_match_cond(void* c,FILE* fp)
{
	fputs("(restLen&&(matchLen=1))",fp);
}

static const FklLalrBuiltinMatch builtin_match_any=
{
	.name="any",
	.match=builtin_match_any_func,
	.print_src=builtin_match_any_print_src,
	.print_c_match_cond=builtin_match_any_print_c_match_cond,
};

static void default_builtin_match_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("",fp);
}

DEFINE_DEFAULT_C_MATCH_COND(hex2);

static void builtin_match_hex2_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static inline int builtin_match_hex2(const char* start\n"
			"\t\t,const char* s\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* matchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n{\n"
			"\tsize_t i=0;\n"
			"\tfor(;i<2&&s[i]&&isxdigit(s[i]);i++);\n"
			"\t*matchLen=i;\n"
			"\treturn i>0;\n"
			"}\n",fp);
}

static int builtin_match_hex2_func(void* ctx
		,const char* start
		,const char* s
		,size_t restLen
		,size_t* matchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	size_t i=0;
	for(;i<2&&s[i]&&isxdigit(s[i]);i++);
	*matchLen=i;
	return i>0;
}

static const FklLalrBuiltinMatch builtin_match_hex2=
{
	.name="hex2",
	.match=builtin_match_hex2_func,
	.print_src=builtin_match_hex2_print_src,
	.print_c_match_cond=builtin_match_hex2_print_c_match_cond,
};

DEFINE_DEFAULT_C_MATCH_COND(oct3);

static int builtin_match_oct3_func(void* ctx
		,const char* start
		,const char* s
		,size_t restLen
		,size_t* matchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	size_t i=0;
	for(;i<4&&*s&&isdigit(s[i])&&s[i]<'8';i++);
	if(i>2&&*s=='0')
	{
		*matchLen=i;
		return 1;
	}
	return 0;
}

static void builtin_match_oct3_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_oct3(const char* start\n"
			"\t\t,const char* s\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* matchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tsize_t i=0;\n"
			"\tfor(;i<4&&*s&&isdigit(s[i])&&s[i]<'8';i++);\n"
			"\tif(i>2&&*s=='0')\n"
			"\t{\n"
			"\t\t*matchLen=i;\n"
			"\t\treturn 1;\n"
			"\t}\n"
			"\treturn 0;\n"
			"}\n",fp);
}

static const FklLalrBuiltinMatch builtin_match_oct3=
{
	.name="oct3",
	.match=builtin_match_oct3_func,
	.print_src=builtin_match_oct3_print_src,
	.print_c_match_cond=builtin_match_oct3_print_c_match_cond,
};

static int builtin_match_dec3_func(void* ctx
		,const char* start
		,const char* s
		,size_t restLen
		,size_t* matchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	size_t i=0;
	for(;i<3&&s[i]&&isdigit(s[i]);i++);
	if((i>1&&*s!='0')||(*s=='0'&&i==1))
	{
		*matchLen=i;
		return 1;
	}
	return 0;
}

DEFINE_DEFAULT_C_MATCH_COND(dec3);

static void builtin_match_dec3_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_dec3(const char* start\n"
			"\t\t,const char* s\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* matchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tsize_t i=0;\n"
			"\tfor(;i<3&&s[i]&&isdigit(s[i]);i++);\n"
			"\tif((i>1&&*s!='0')||(*s=='0'&&i==1))\n"
			"\t{\n"
			"\t\t*matchLen=i;\n"
			"\t\treturn 1;\n"
			"\t}\n"
			"\treturn 0;\n"
			"}\n",fp);
}

static const FklLalrBuiltinMatch builtin_match_dec3=
{
	.name="dec3",
	.match=builtin_match_dec3_func,
	.print_src=builtin_match_dec3_print_src,
	.print_c_match_cond=builtin_match_dec3_print_c_match_cond,
};

DEFINE_DEFAULT_C_MATCH_COND(space);

static int builtin_match_func_space(void* ctx
		,const char* start
		,const char* s
		,size_t restLen
		,size_t* matchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	size_t i=0;
	for(;s[i]&&isspace(s[i]);i++);
	*matchLen=i;
	return i>0;
}

static void builtin_match_space_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_space(const char* start\n"
			"\t\t,const char* s\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* matchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tsize_t i=0;\n"
			"\tfor(;s[i]&&isspace(s[i]);i++);\n"
			"\t*matchLen=i;\n"
			"\treturn i>0;\n"
			"}\n",fp);
}

static const FklLalrBuiltinMatch builtin_match_space=
{
	.name="space",
	.match=builtin_match_func_space,
	.print_src=builtin_match_space_print_src,
	.print_c_match_cond=builtin_match_space_print_c_match_cond,
};


static int builtin_match_func_eol(void* ctx
		,const char* start
		,const char* s
		,size_t restLen
		,size_t* matchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	size_t i=0;
	for(;s[i]&&s[i]!='\n';i++);
	*matchLen=i+(s[i]=='\n');
	return 1;
}

static void builtin_match_eol_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_eol(const char* start\n"
			"\t\t,const char* s\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* matchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tsize_t i=0;\n"
			"\tfor(;s[i]&&s[i]!='\\n';i++);\n"
			"\t*matchLen=i+(s[i]=='\\n');\n"
			"\treturn 1;\n"
			"}\n",fp);
}

DEFINE_DEFAULT_C_MATCH_COND(eol);

static const FklLalrBuiltinMatch builtin_match_eol=
{
	.name="eol",
	.match=builtin_match_func_eol,
	.print_src=builtin_match_eol_print_src,
	.print_c_match_cond=builtin_match_eol_print_c_match_cond,
};

typedef struct
{
	FklSid_t id;
	const FklLalrBuiltinMatch* t;
}SidBuiltinHashItem;

static void sid_builtin_set_val(void* d0,const void* d1)
{
	*(SidBuiltinHashItem*)d0=*(const SidBuiltinHashItem*)d1;
}

static const FklHashTableMetaTable SidBuiltinHashMetaTable=
{
	.size=sizeof(SidBuiltinHashItem),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=fklSetSidKey,
	.__setVal=sid_builtin_set_val,
	.__hashFunc=fklSidHashFunc,
	.__keyEqual=fklSidKeyEqual,
	.__uninitItem=fklDoNothingUnintHashItem,
};

static inline const FklGrammerSym* sym_at_idx(const FklGrammerProduction* prod,size_t i)
{
	if(i>=prod->len)
		return NULL;
	return &prod->syms[i];
}

static inline void* init_builtin_grammer_sym(const FklLalrBuiltinMatch* m
		,size_t i
		,FklGrammerProduction* prod
		,FklGrammer* g
		,int* failed)
{
	if(m->ctx_global_create)
		return m->ctx_global_create(i,prod,g,failed);
	if(m->ctx_create)
		return m->ctx_create(sym_at_idx(prod,i+1),g->terminals,failed);
	return NULL;
}

static inline FklGrammerProduction* create_grammer_prod_from_cstr(const char* str
		,FklGrammer* g
		,FklHashTable* builtins
		,FklSymbolTable* symbolTable
		,FklSymbolTable* termTable
		,const char* name
		,FklBuiltinProdAction func)
{
	const char* ss;
	size_t len;
	for(ss=str;isspace(*ss);ss++);
	for(len=0;!isspace(ss[len]);len++);
	FklSid_t left=fklAddSymbolCharBuf(ss,len,symbolTable)->id;
	ss+=len;
	len=0;
	FklPtrStack st=FKL_STACK_INIT;
	fklInitPtrStack(&st,8,8);
	size_t joint_num=0;
	while(*ss)
	{
		for(;isspace(*ss);ss++);
		for(len=0;ss[len]&&!isspace(ss[len]);len++);
		if(ss[0]=='+')
			joint_num++;
		if(len)
			fklPushPtrStack(fklCreateString(len,ss),&st);
		ss+=len;
	}
	size_t prod_len=st.top-joint_num;
	FklGrammerProduction* prod=create_empty_production(left,prod_len,name,func);
	int next_delim=1;
	size_t symIdx=0;
	for(uint32_t i=0;i<st.top;i++)
	{
		FklGrammerSym* u=&prod->syms[symIdx];
		u->isbuiltin=0;
		u->delim=next_delim;
		next_delim=1;
		FklString* s=st.base[i];
		switch(*(s->str))
		{
			case '+':
				next_delim=0;
				continue;
				break;
			case '#':
				u->isterm=1;
				u->nt=fklAddSymbolCharBuf(&s->str[1],s->size-1,termTable)->id;
				break;
			case '&':
				{
					FklSid_t id=fklAddSymbolCharBuf(&s->str[1],s->size-1,symbolTable)->id;
					const SidBuiltinHashItem* builtin=fklGetHashItem(&id,builtins);
					if(builtin)
					{
						u->isterm=1;
						u->isbuiltin=1;
						u->b.t=builtin->t;
						u->b.c=NULL;
					}
					else
					{
						u->isterm=0;
						u->nt=id;
					}
				}
				break;
		}
		symIdx++;
	}
	while(!fklIsPtrStackEmpty(&st))
		free(fklPopPtrStack(&st));
	fklUninitPtrStack(&st);
	return prod;
}

static inline FklGrammerIgnore* prod_to_ignore(FklGrammerProduction* prod,const FklSymbolTable* tt)
{
	FklGrammerSym* syms=prod->syms;
	size_t len=prod->len;
	for(size_t i=len;i>0;i--)
	{
		FklGrammerSym* sym=&syms[i-1];
		if(!sym->isterm)
			return NULL;
		if(sym->isbuiltin)
		{
			int failed=0;
			FklLalrBuiltinGrammerSym* b=&sym->b;
			if(b->t->ctx_global_create&&!b->t->ctx_create)
				return NULL;
			if(b->c)
				destroy_builtin_grammer_sym(b);
			if(b->t->ctx_create)
				b->c=b->t->ctx_create(sym_at_idx(prod,i+1),tt,&failed);
			if(failed)
				return NULL;
		}
	}
	FklGrammerIgnore* ig=(FklGrammerIgnore*)malloc(sizeof(FklGrammerIgnore)+len*sizeof(FklGrammerIgnoreSym));
	FKL_ASSERT(ig);
	ig->len=len;
	ig->next=NULL;
	FklGrammerIgnoreSym* igss=ig->ig;
	for(size_t i=0;i<len;i++)
	{
		FklGrammerSym* sym=&syms[i];
		FklGrammerIgnoreSym* igs=&igss[i];
		igs->isbuiltin=sym->isbuiltin;
		if(igs->isbuiltin)
			igs->b=sym->b;
		else
			igs->str=fklGetSymbolWithId(sym->nt,tt)->symbol;
	}
	return ig;
}

static inline FklGrammerIgnore* create_grammer_ignore_from_cstr(const char* str
		,FklGrammer* g
		,FklHashTable* builtins
		,FklSymbolTable* symbolTable
		,FklSymbolTable* termTable)
{
	const char* ss;
	size_t len;
	for(ss=str;isspace(*ss);ss++);
	for(len=0;!isspace(ss[len]);len++);
	FklSid_t left=0;
	ss+=len;
	len=0;
	FklPtrStack st=FKL_STACK_INIT;
	fklInitPtrStack(&st,8,8);
	size_t joint_num=0;
	while(*ss)
	{
		for(;isspace(*ss);ss++);
		for(len=0;ss[len]&&!isspace(ss[len]);len++);
		if(ss[0]=='+')
			joint_num++;
		if(len)
			fklPushPtrStack(fklCreateString(len,ss),&st);
		ss+=len;
	}
	size_t prod_len=st.top-joint_num;
	if(!prod_len)
	{
		while(!fklIsPtrStackEmpty(&st))
			free(fklPopPtrStack(&st));
		fklUninitPtrStack(&st);
		return NULL;
	}
	FklGrammerProduction* prod=create_empty_production(left,prod_len,NULL,NULL);
	int next_delim=1;
	size_t symIdx=0;
	for(uint32_t i=0;i<st.top;i++)
	{
		FklGrammerSym* u=&prod->syms[symIdx];
		u->isbuiltin=0;
		u->delim=next_delim;
		next_delim=1;
		FklString* s=st.base[i];
		switch(*(s->str))
		{
			case '+':
				next_delim=0;
				continue;
				break;
			case '#':
				u->isterm=1;
				u->nt=fklAddSymbolCharBuf(&s->str[1],s->size-1,termTable)->id;
				break;
			case '&':
				{
					FklSid_t id=fklAddSymbolCharBuf(&s->str[1],s->size-1,symbolTable)->id;
					const SidBuiltinHashItem* builtin=fklGetHashItem(&id,builtins);
					if(builtin)
					{
						u->isterm=1;
						u->isbuiltin=1;
						u->b.t=builtin->t;
						u->b.c=NULL;
					}
					else
					{
						u->isterm=0;
						u->nt=id;
					}
				}
				break;
		}
		symIdx++;
	}
	while(!fklIsPtrStackEmpty(&st))
		free(fklPopPtrStack(&st));
	fklUninitPtrStack(&st);
	FklGrammerIgnore* ig=prod_to_ignore(prod,termTable);
	destroy_prod(prod);
	return ig;
}

static inline void init_prod_hash_table(FklHashTable* s)
{
	fklInitHashTable(s,&ProdHashMetaTable);
}

static inline int prod_sym_equal(const FklGrammerSym* u0,const FklGrammerSym* u1)
{
	if(u0->isbuiltin==u1->isbuiltin
			&&u0->delim==u1->delim
			&&u0->isterm==u1->isterm)
	{
		if(u0->isbuiltin)
		{
			if(u0->b.t==u1->b.t)
			{
				if(u0->b.t->ctx_equal)
					return u0->b.t->ctx_equal(u0->b.c,u1->b.c);
				return 1;
			}
			return 0;
		}
		else
			return u0->nt==u1->nt;
	}
	return 0;
}

static inline int prod_equal(const FklGrammerProduction* prod0,const FklGrammerProduction* prod1)
{
	if(prod0->len!=prod1->len)
		return 0;
	size_t len=prod0->len;
	FklGrammerSym* u0=prod0->syms;
	FklGrammerSym* u1=prod1->syms;
	for(size_t i=0;i<len;i++)
		if(!prod_sym_equal(&u0[i],&u1[i]))
			return 0;
	return 1;
}

static inline FklGrammerProduction* create_extra_production(FklSid_t start)
{
	FklGrammerProduction* prod=create_empty_production(0,1,NULL,NULL);
	prod->idx=0;
	FklGrammerSym* u=&prod->syms[0];
	u->delim=1;
	u->isbuiltin=0;
	u->isterm=0;
	u->nt=start;
	return prod;
}

static inline int add_prod_to_grammer(FklGrammer* grammer,FklGrammerProduction* prod)
{
	FklSid_t left=prod->left;
	if(fklGetHashItem(&left,&grammer->builtins))
		return 1;
	FklHashTable* productions=&grammer->productions;
	ProdHashItem* item=fklGetHashItem(&left,productions);
	if(item)
	{
		FklGrammerProduction** pp=&item->prods;
		FklGrammerProduction* cur=NULL;
		for(;*pp;pp=&((*pp)->next))
		{
			if(prod_equal(*pp,prod))
			{
				cur=*pp;
				break;
			}
		}
		if(cur)
		{
			// prod->next=cur->next;
			// *pp=prod;
			// destroy_prod(cur);
			destroy_prod(prod);
		}
		else
		{
			prod->idx=grammer->prodNum;
			grammer->prodNum++;
			prod->next=NULL;
			*pp=prod;
		}
	}
	else
	{
		if(!grammer->start)
		{
			grammer->start=left;
			FklGrammerProduction* extra_prod=create_extra_production(left);
			// fklPutHashItem(&extra_prod->left,&grammer->nonterminals);
			extra_prod->next=NULL;
			item=fklPutHashItem(&extra_prod->left,productions);
			item->prods=extra_prod;
			item->prods->idx=grammer->prodNum;
			grammer->prodNum++;
		}
		fklPutHashItem(&left,&grammer->nonterminals);
		prod->next=NULL;
		item=fklPutHashItem(&left,productions);
		prod->idx=grammer->prodNum;
		item->prods=prod;
		grammer->prodNum++;
	}
	return 0;
}

static inline int builtin_grammer_sym_cmp(const FklLalrBuiltinGrammerSym* b0,const FklLalrBuiltinGrammerSym* b1)
{
	if(b0->t->ctx_cmp)
		return b0->t->ctx_cmp(b0->c,b1->c);
	return 0;
}

static inline int grammer_sym_cmp(const FklGrammerSym* s0,const FklGrammerSym* s1)
{
	if(s0->isterm>s1->isterm)
		return -1;
	else if(s0->isterm<s1->isterm)
		return 1;
	else if(s0->isbuiltin<s1->isbuiltin)
		return -1;
	else if(s0->isbuiltin>s1->isbuiltin)
		return 1;
	else
	{
		int r=0;
		if(s0->isbuiltin&&s0->b.t<s1->b.t)
			return -1;
		else if(s0->isbuiltin&&s0->b.t>s1->b.t)
			return 1;
		else if(s0->isbuiltin&&(r=builtin_grammer_sym_cmp(&s0->b,&s1->b)))
			return r;
		else if(s0->nt<s1->nt)
			return -1;
		else if(s0->nt>s1->nt)
			return 1;
		else
		{
			unsigned int f0=(s0->delim<<1);
			unsigned int f1=(s1->delim<<1);
			if(f0<f1)
				return -1;
			else if(f0>f1)
				return 1;
		}
	}
	return 0;
}

static inline int builtin_grammer_sym_equal(const FklLalrBuiltinGrammerSym* s0,const FklLalrBuiltinGrammerSym* s1)
{
	if(s0->t==s1->t)
	{
		if(s0->t->ctx_equal)
			return s0->t->ctx_equal(s0->c,s1->c);
		return 1;
	}
	return 0;
}

static inline int grammer_sym_equal(const FklGrammerSym* s0,const FklGrammerSym* s1)
{
	return s0->isterm==s1->isterm
		&&s0->isbuiltin==s1->isbuiltin
		&&(s0->isbuiltin?builtin_grammer_sym_equal(&s0->b,&s1->b):s0->nt==s1->nt)
		&&s0->delim==s1->delim;
}

static uintptr_t builtin_grammer_sym_hash(const FklLalrBuiltinGrammerSym* s)
{
	if(s->t->ctx_hash)
		return s->t->ctx_hash(s->c);
	return 0;
}

static inline uintptr_t grammer_sym_hash(const FklGrammerSym* s)
{
	return (s->isterm<<3)
		+(s->isbuiltin?builtin_grammer_sym_hash(&s->b)<<2:s->nt<<2)
		+(s->delim<<1);
}

static int builtin_match_qstr_cmp(const void* c0,const void* c1)
{
	if(c0==c1)
		return 0;
	if(c0==NULL)
		return -1;
	if(c1==NULL)
		return 1;
	const FklString* q0=(const FklString*)c0;
	const FklString* q1=(const FklString*)c1;
	return fklStringCmp(q0,q1);
}

static int builtin_match_qstr_equal(const void* c0,const void* c1)
{
	if(c0==c1)
		return 1;
	if(c0==NULL||c1==NULL) 
		return 0;
	const FklString* q0=(const FklString*)c0;
	const FklString* q1=(const FklString*)c1;
	return q0==q1;
}

static uintptr_t builtin_match_qstr_hash(const void* c0)
{
	if(c0)
	{
		const FklString* q0=(const FklString*)c0;
		return (uintptr_t)q0;
	}
	return 0;
}

static void* builtin_match_qstr_create(const FklGrammerSym* next
		,const FklSymbolTable* tt
		,int* failed)
{
	*failed=0;
	if(!next)
		return NULL;
	if(next->isterm)
	{
		if(next->isbuiltin)
		{
			*failed=1;
			return NULL;
		}
		return fklGetSymbolWithId(next->nt,tt)->symbol;
	}
	return NULL;
}

static void builtin_match_qstr_destroy(void* ctx)
{
	if(ctx)
		free(ctx);
}

static int builtin_match_qstr_match(void* c
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* matchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	const FklString* s=c;
	if(s)
	{
		size_t len=0;
		for(;len<restLen;len++)
		{
			if(cstr[len]=='\\')
			{
				len++;
				continue;
			}
			if(fklStringCharBufMatch(s,&cstr[len],restLen-len))
			{
				*matchLen=len;
				return 1;
			}
		}
	}
	else
	{
		*matchLen=restLen;
		return 1;
	}
	return 0;
}

static void builtin_match_qstr_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_qstr(const char* start\n"
			"\t\t,const char* cstr\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* matchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx\n"
			"\t\t,const char* end\n"
			"\t\t,size_t end_size)\n"
			"{\n"
			"\tif(end)\n"
			"\t{\n"
			"\t\tsize_t len=0;\n"
			"\t\tfor(;len<restLen;len++)\n"
			"\t\t{\n"
			"\t\t\tif(cstr[len]=='\\\\')\n"
			"\t\t\t{\n"
			"\t\t\t\tlen++;\n"
			"\t\t\t\tcontinue;\n"
			"\t\t\t}\n"
			"\t\t\tif(fklCharBufMatch(end,end_size,&cstr[len],restLen-len))\n"
			"\t\t\t{\n"
			"\t\t\t\t*matchLen=len;\n"
			"\t\t\t\treturn 1;\n"
			"\t\t\t}\n"
			"\t\t}\n"
			"\t}\n"
			"\telse\n"
			"\t{\n"
			"\t\t*matchLen=restLen;\n"
			"\t\treturn 1;\n"
			"\t}\n"
			"\treturn 0;\n"
			"}\n",fp);
}

static inline void print_string_in_hex(const FklString* stri,FILE* fp)
{
	size_t size=stri->size;
	const char* str=stri->str;
	for(size_t i=0;i<size;i++)
		fprintf(fp,"\\x%02X",str[i]);
}

static void builtin_match_qstr_print_c_match_cond(void* c,FILE* fp)
{
	fputs("builtin_match_qstr(start,*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,outerCtx,",fp);
	if(c)
	{
		const FklString* s=c;
		fputc('"',fp);
		print_string_in_hex(s,fp);
		fprintf(fp,"\",%lu",s->size);
	}
	else
		fputs("NULL,0",fp);
	fputc(')',fp);
}

static const FklLalrBuiltinMatch builtin_match_qstr=
{
	.match=builtin_match_qstr_match,
	.name="qstr",
	.ctx_cmp=builtin_match_qstr_cmp,
	.ctx_equal=builtin_match_qstr_equal,
	.ctx_hash=builtin_match_qstr_hash,
	.ctx_create=builtin_match_qstr_create,
	.ctx_destroy=builtin_match_qstr_destroy,
	.print_src=builtin_match_qstr_print_src,
	.print_c_match_cond=builtin_match_qstr_print_c_match_cond,
};

inline int fklIsDecInt(const char* cstr,size_t maxLen)
{
	// [-+]?(0|[1-9]\d*)
	if(!maxLen)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(cstr[idx]=='0')
	{
		if(idx+1==maxLen)
			return 1;
		else
			return 0;
	}
	if(isdigit(cstr[idx])&&cstr[idx]!='0')
		idx++;
	else
		return 0;
	for(;idx<maxLen;idx++)
		if(!isdigit(cstr[idx]))
			return 0;
	return 1;
}

inline int fklIsOctInt(const char* cstr,size_t maxLen)
{
	// [-+]?0[0-7]+
	if(maxLen<2)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(maxLen-idx<2)
		return 0;
	if(cstr[idx]=='0')
		idx++;
	else
		return 0;
	for(;idx<maxLen;idx++)
		if(cstr[idx]<'0'||cstr[idx]>'7')
			return 0;
	return 1;
}

inline int fklIsHexInt(const char* cstr,size_t maxLen)
{
	// [-+]?0[xX][0-9a-fA-F]+
	if(maxLen<3)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(maxLen-idx<3)
		return 0;
	if(cstr[idx]=='0')
		idx++;
	else
		return 0;
	if(cstr[idx]=='x'||cstr[idx]=='X')
		idx++;
	else
		return 0;
	for(;idx<maxLen;idx++)
		if(!isxdigit(cstr[idx]))
			return 0;
	return 1;
}

inline int fklIsDecFloat(const char* cstr,size_t maxLen)
{
	// [-+]?(\.\d+([eE][-+]?\d+)?|\d+(\.\d*([eE][-+]?\d+)?|[eE][-+]?\d+))
	if(maxLen<2)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(maxLen-idx<2)
		return 0;
	if(cstr[idx]=='.')
	{
		idx++;
		if(maxLen-idx<1||cstr[idx]=='e'||cstr[idx]=='E')
			return 0;
		goto after_dot;
	}
	else
	{
		for(;idx<maxLen;idx++)
			if(!isdigit(cstr[idx]))
				break;
		if(idx==maxLen)
			return 0;
		if(cstr[idx]=='.')
		{
			idx++;
after_dot:
			for(;idx<maxLen;idx++)
				if(!isdigit(cstr[idx]))
					break;
			if(idx==maxLen)
				return 1;
			else if(cstr[idx]=='e'||cstr[idx]=='E')
				goto after_e;
			else
				return 0;
		}
		else if(cstr[idx]=='e'||cstr[idx]=='E')
		{
after_e:
			idx++;
			if(cstr[idx]=='-'||cstr[idx]=='+')
				idx++;
			if(maxLen-idx<1)
				return 0;
			for(;idx<maxLen;idx++)
				if(!isdigit(cstr[idx]))
					return 0;
		}
		else
			return 0;
	}
	return 1;
}

inline int fklIsHexFloat(const char* cstr,size_t maxLen)
{
	// [-+]?0[xX](\.[0-9a-fA-F]+[pP][-+]?[0-9a-fA-F]+|[0-9a-fA-F]+(\.[0-9a-fA-F]*[pP][-+]?[0-9a-fA-F]+|[pP][-+]?[0-9a-fA-F]+))
	if(maxLen<5)
		return 0;
	size_t idx=0;
	if(cstr[idx]=='-'||cstr[idx]=='+')
		idx++;
	if(maxLen-idx<5)
		return 0;
	if(cstr[idx]=='0')
		idx++;
	else
		return 0;
	if(cstr[idx]=='x'||cstr[idx]=='X')
		idx++;
	else
		return 0;
	if(cstr[idx]=='.')
	{
		idx++;
		if(maxLen-idx<3||cstr[idx]=='p'||cstr[idx]=='P')
			return 0;
		goto after_dot;
	}
	else
	{
		for(;idx<maxLen;idx++)
			if(!isxdigit(cstr[idx]))
				break;
		if(idx==maxLen)
			return 0;
		if(cstr[idx]=='.')
		{
			idx++;
after_dot:
			for(;idx<maxLen;idx++)
				if(!isxdigit(cstr[idx]))
					break;
			if(idx==maxLen)
				return 0;
			else if(cstr[idx]=='p'||cstr[idx]=='P')
				goto after_p;
			else
				return 0;
		}
		else if(cstr[idx]=='p'||cstr[idx]=='P')
		{
after_p:
			idx++;
			if(cstr[idx]=='-'||cstr[idx]=='+')
				idx++;
			if(maxLen-idx<1)
				return 0;
			for(;idx<maxLen;idx++)
				if(!isxdigit(cstr[idx]))
					return 0;
		}
		else
			return 0;
	}
	return 1;
}

static inline int ignore_match(FklGrammerIgnore* ig
		,const char* start
		,const char* str
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	size_t matchLen=0;
	FklGrammerIgnoreSym* igss=ig->ig;
	size_t len=ig->len;
	for(size_t i=0;i<len;i++)
	{
		FklGrammerIgnoreSym* ig=&igss[i];
		if(ig->isbuiltin)
		{
			size_t len=0;
			if(ig->b.t->match(ig->b.c,start,str,restLen-matchLen,&len,outerCtx))
			{
				str+=len;
				matchLen+=len;
			}
			else
				return 0;
		}
		else
		{
			const FklString* laString=ig->str;
			if(fklStringCharBufMatch(laString,str,restLen-matchLen))
			{
				str+=laString->size;
				matchLen+=laString->size;
			}
			else
				return 0;
		}
	}
	*pmatchLen=matchLen;
	return 1;
}

static inline size_t get_max_non_term_length(const FklGrammer* g
		,FklGrammerMatchOuterCtx* ctx
		,const char* start
		,const char* cur
		,size_t restLen)
{
	if(restLen)
	{
		if(start==ctx->start&&cur==ctx->cur)
			return ctx->maxNonterminalLen;
		ctx->start=start;
		ctx->cur=cur;
		FklGrammerIgnore* ignores=g->ignores;
		const FklString** terms=g->sortedTerminals;
		size_t num=g->sortedTerminalsNum;
		size_t len=0;
		while(len<restLen)
		{
			for(FklGrammerIgnore* ig=ignores;ig;ig=ig->next)
			{
				size_t matchLen=0;
				if(ignore_match(ig,start,cur,restLen-len,&matchLen,ctx))
					goto break_loop;
			}
			for(size_t i=0;i<num;i++)
				if(fklStringCharBufMatch(terms[i],cur,restLen-len))
					goto break_loop;
			len++;
			cur++;
		}
break_loop:
		ctx->maxNonterminalLen=len;
		return len;
	}
	return 0;
}

static int builtin_match_dec_int_func(void* ctx
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	if(restLen)
	{
		size_t maxLen=get_max_non_term_length(ctx,outerCtx,start,cstr,restLen);
		if(maxLen&&fklIsDecInt(cstr,maxLen))
		{
			*pmatchLen=maxLen;
			return 1;
		}
	}
	return 0;
}

static int string_len_cmp(const void* a,const void* b)
{
	const FklString* s0=*(FklString* const*)a;
	const FklString* s1=*(FklString* const*)b;
	if(s0->size<s1->size)
		return 1;
	if(s0->size>s1->size)
		return -1;
	return 0;
}

static void* builtin_match_number_global_create(size_t idx
		,FklGrammerProduction* prod
		,FklGrammer* g
		,int* failed)
{
	if(!g->sortedTerminals)
	{
		size_t num=g->terminals->num;
		g->sortedTerminalsNum=num;
		const FklString** terms=(const FklString**)malloc(sizeof(FklString*)*num);
		FKL_ASSERT(terms);
		FklSymTabNode** symList=g->terminals->list;
		for(size_t i=0;i<num;i++)
			terms[i]=symList[i]->symbol;
		qsort(terms,num,sizeof(FklString*),string_len_cmp);
		g->sortedTerminals=terms;
	}
	return g;
}

DEFINE_DEFAULT_C_MATCH_COND(dec_int);

static void builtin_match_dec_int_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_dec_int(const char* start\n"
			"\t\t,const char* cstr\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* pmatchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tif(restLen)\n"
			"\t{\n"
			"\t\tsize_t maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
			"\t\tif(maxLen&&fklIsDecInt(cstr,maxLen))\n"
			"\t\t{\n"
			"\t\t\t*pmatchLen=maxLen;\n"
			"\t\t\treturn 1;\n"
			"\t\t}\n"
			"\t}\n"
			"\treturn 0;\n"
			"}\n",fp);
}

static const FklLalrBuiltinMatch builtin_match_dec_int=
{
	.match=builtin_match_dec_int_func,
	.name="dec-int",
	.ctx_global_create=builtin_match_number_global_create,
	.print_src=builtin_match_dec_int_print_src,
	.print_c_match_cond=builtin_match_dec_int_print_c_match_cond,
};

static int builtin_match_hex_int_func(void* ctx
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	if(restLen)
	{
		size_t maxLen=get_max_non_term_length(ctx,outerCtx,start,cstr,restLen);
		if(maxLen&&fklIsHexInt(cstr,maxLen))
		{
			*pmatchLen=maxLen;
			return 1;
		}
	}
	return 0;
}

static void builtin_match_hex_int_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_hex_int(const char* start\n"
			"\t\t,const char* cstr\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* pmatchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tif(restLen)\n"
			"\t{\n"
			"\t\tsize_t maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
			"\t\tif(maxLen&&fklIsHexInt(cstr,maxLen))\n"
			"\t\t{\n"
			"\t\t\t*pmatchLen=maxLen;\n"
			"\t\t\treturn 1;\n"
			"\t\t}\n"
			"\t}\n"
			"\treturn 0;\n"
			"}\n",fp);
}

DEFINE_DEFAULT_C_MATCH_COND(hex_int);

static const FklLalrBuiltinMatch builtin_match_hex_int=
{
	.name="hex-int",
	.match=builtin_match_hex_int_func,
	.ctx_global_create=builtin_match_number_global_create,
	.print_src=builtin_match_hex_int_print_src,
	.print_c_match_cond=builtin_match_hex_int_print_c_match_cond,
};

static int builtin_match_oct_int_func(void* ctx
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	if(restLen)
	{
		size_t maxLen=get_max_non_term_length(ctx,outerCtx,start,cstr,restLen);
		if(maxLen&&fklIsOctInt(cstr,maxLen))
		{
			*pmatchLen=maxLen;
			return 1;
		}
	}
	return 0;
}

static void builtin_match_oct_int_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_oct_int(const char* start\n"
			"\t\t,const char* cstr\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* pmatchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tif(restLen)\n"
			"\t{\n"
			"\t\tsize_t maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
			"\t\tif(maxLen&&fklIsOctInt(cstr,maxLen))\n"
			"\t\t{\n"
			"\t\t\t*pmatchLen=maxLen;\n"
			"\t\t\treturn 1;\n"
			"\t\t}\n"
			"\t}\n"
			"\treturn 0;\n"
			"}\n",fp);
}

DEFINE_DEFAULT_C_MATCH_COND(oct_int);

static const FklLalrBuiltinMatch builtin_match_oct_int=
{
	.name="oct-int",
	.match=builtin_match_oct_int_func,
	.ctx_global_create=builtin_match_number_global_create,
	.print_src=builtin_match_oct_int_print_src,
	.print_c_match_cond=builtin_match_oct_int_print_c_match_cond,
};

static int builtin_match_dec_float_func(void* ctx
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	if(restLen)
	{
		size_t maxLen=get_max_non_term_length(ctx,outerCtx,start,cstr,restLen);
		if(maxLen&&fklIsDecFloat(cstr,maxLen))
		{
			*pmatchLen=maxLen;
			return 1;
		}
	}
	return 0;
}

static void builtin_match_dec_float_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_dec_float(const char* start\n"
			"\t\t,const char* cstr\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* pmatchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tif(restLen)\n"
			"\t{\n"
			"\t\tsize_t maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
			"\t\tif(maxLen&&fklIsDecFloat(cstr,maxLen))\n"
			"\t\t{\n"
			"\t\t\t*pmatchLen=maxLen;\n"
			"\t\t\treturn 1;\n"
			"\t\t}\n"
			"\t}\n"
			"\treturn 0;\n"
			"}\n",fp);
}

DEFINE_DEFAULT_C_MATCH_COND(dec_float);

static const FklLalrBuiltinMatch builtin_match_dec_float=
{
	.name="dec-float",
	.match=builtin_match_dec_float_func,
	.ctx_global_create=builtin_match_number_global_create,
	.print_src=builtin_match_dec_float_print_src,
	.print_c_match_cond=builtin_match_dec_float_print_c_match_cond,
};

static int builtin_match_hex_float_func(void* ctx
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	if(restLen)
	{
		size_t maxLen=get_max_non_term_length(ctx,outerCtx,start,cstr,restLen);
		if(maxLen&&fklIsHexFloat(cstr,maxLen))
		{
			*pmatchLen=maxLen;
			return 1;
		}
	}
	return 0;
}

static void builtin_match_hex_float_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_hex_float(const char* start\n"
			"\t\t,const char* cstr\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* pmatchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tif(restLen)\n"
			"\t{\n"
			"\t\tsize_t maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
			"\t\tif(maxLen&&fklIsHexFloat(cstr,maxLen))\n"
			"\t\t{\n"
			"\t\t\t*pmatchLen=maxLen;\n"
			"\t\t\treturn 1;\n"
			"\t\t}\n"
			"\t}\n"
			"\treturn 0;\n"
			"}\n",fp);
}

DEFINE_DEFAULT_C_MATCH_COND(hex_float);

static const FklLalrBuiltinMatch builtin_match_hex_float=
{
	.name="hex-float",
	.match=builtin_match_hex_float_func,
	.ctx_global_create=builtin_match_number_global_create,
	.print_src=builtin_match_hex_float_print_src,
	.print_c_match_cond=builtin_match_hex_float_print_c_match_cond,
};

static int builtin_match_identifier_func(void* c
		,const char* cstrStart
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	size_t maxLen=get_max_non_term_length(c,outerCtx,cstrStart,cstr,restLen);
	if(!maxLen
			||fklIsDecInt(cstr,maxLen)
			||fklIsOctInt(cstr,maxLen)
			||fklIsHexInt(cstr,maxLen)
			||fklIsDecFloat(cstr,maxLen)
			||fklIsHexFloat(cstr,maxLen))
		return 0;
	*pmatchLen=maxLen;
	return 1;
}

static void builtin_match_identifier_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_identifier(const char* cstrStart\n"
			"\t,const char* cstr\n"
			"\t,size_t restLen\n"
			"\t,size_t* pmatchLen\n"
			"\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tsize_t maxLen=get_max_non_term_length(outerCtx,cstrStart,cstr,restLen);\n"
			"\tif(!maxLen\n"
			"\t\t\t||fklIsDecInt(cstr,maxLen)\n"
			"\t\t\t||fklIsOctInt(cstr,maxLen)\n"
			"\t\t\t||fklIsHexInt(cstr,maxLen)\n"
			"\t\t\t||fklIsDecFloat(cstr,maxLen)\n"
			"\t\t\t||fklIsHexFloat(cstr,maxLen))\n"
			"\t\treturn 0;\n"
			"\t*pmatchLen=maxLen;\n"
			"\treturn 1;\n"
			"}\n",fp);
}

DEFINE_DEFAULT_C_MATCH_COND(identifier);

static const FklLalrBuiltinMatch builtin_match_identifier=
{
	.name="identifier",
	.match=builtin_match_identifier_func,
	.ctx_global_create=builtin_match_number_global_create,
	.print_src=builtin_match_identifier_print_src,
	.print_c_match_cond=builtin_match_identifier_print_c_match_cond,
};

typedef struct
{
	const FklGrammer* g;
	const FklString* start;
}SymbolNumberCtx;

static void* s_number_ctx_global_create(size_t idx
		,FklGrammerProduction* prod
		,FklGrammer* g
		,int* failed)
{
	if(prod->len>2||idx!=0)
	{
		*failed=1;
		return NULL;
	}
	const FklString* start=NULL;
	if(prod->len==2)
	{
		if(!prod->syms[1].isterm
				||prod->syms[1].isbuiltin)
		{
			*failed=1;
			return NULL;
		}
		start=fklGetSymbolWithId(prod->syms[1].nt,g->terminals)->symbol;
	}
	prod->len=1;
	SymbolNumberCtx* ctx=(SymbolNumberCtx*)malloc(sizeof(SymbolNumberCtx));
	FKL_ASSERT(ctx);
	ctx->start=start;
	ctx->g=g;
	if(!g->sortedTerminals)
	{
		size_t num=g->terminals->num;
		g->sortedTerminalsNum=num;
		const FklString** terms=(const FklString**)malloc(sizeof(FklString*)*num);
		FKL_ASSERT(terms);
		FklSymTabNode** symList=g->terminals->list;
		for(size_t i=0;i<num;i++)
			terms[i]=symList[i]->symbol;
		qsort(terms,num,sizeof(FklString*),string_len_cmp);
		g->sortedTerminals=terms;
	}
	return ctx;
}

static int s_number_ctx_cmp(const void* c0,const void* c1)
{
	const SymbolNumberCtx* ctx0=c0;
	const SymbolNumberCtx* ctx1=c1;
	if(ctx0->start==ctx1->start)
		return 0;
	else if(ctx0==NULL)
		return -1;
	else if(ctx1==NULL)
		return 1;
	else
		return fklStringCmp(ctx0->start,ctx1->start);
}

static int s_number_ctx_equal(const void* c0,const void* c1)
{
	const SymbolNumberCtx* ctx0=c0;
	const SymbolNumberCtx* ctx1=c1;
	if(ctx0->start==ctx1->start)
		return 1;
	else if(ctx0==NULL||ctx1==NULL)
		return 0;
	else
		return !fklStringCmp(ctx0->start,ctx1->start);
}

static uintptr_t s_number_ctx_hash(const void* c0)
{
	const SymbolNumberCtx* ctx0=c0;
	return (uintptr_t)ctx0->start;
}

static void s_number_ctx_destroy(void* c)
{
	free(c);
}

#define SYMBOL_NUMBER_MATCH(F) SymbolNumberCtx* ctx=c;\
	if(restLen)\
	{\
		size_t maxLen=get_max_non_term_length(ctx->g,outerCtx,start,cstr,restLen);\
		if(maxLen&&(!ctx->start||!fklStringCharBufMatch(ctx->start,cstr+maxLen,restLen-maxLen))&&F(cstr,maxLen))\
		{\
			*pmatchLen=maxLen;\
			return 1;\
		}\
	}\
	return 0

static int builtin_match_s_dint_func(void* c
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	SymbolNumberCtx* ctx=c;
	if(restLen)
	{
		size_t maxLen=get_max_non_term_length(ctx->g,outerCtx,start,cstr,restLen);
		if(maxLen&&(!ctx->start||!fklStringCharBufMatch(ctx->start,cstr+maxLen,restLen-maxLen))
				&&fklIsDecInt(cstr,maxLen))
		{
			*pmatchLen=maxLen;
			return 1;
		}
	}
	return 0;
}

#define DEFINE_LISP_NUMBER_PRINT_SRC(NAME,F) static void builtin_match_##NAME##_print_src(const FklGrammer* g,FILE* fp)\
{\
	fputs("static int builtin_match_"#NAME"(const char* start\n"\
			"\t\t,const char* cstr\n"\
			"\t\t,size_t restLen\n"\
			"\t\t,size_t* pmatchLen\n"\
			"\t\t,FklGrammerMatchOuterCtx* outerCtx\n"\
			"\t\t,const char* term\n"\
			"\t\t,size_t termLen)\n"\
			"{\n"\
			"\tif(restLen)\n"\
			"\t{\n"\
			"\t\tsize_t maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"\
			"\t\tif(maxLen&&(!term||!fklCharBufMatch(term,termLen,cstr+maxLen,restLen-maxLen))\n"\
			"\t\t\t\t&&"#F"(cstr,maxLen))\n"\
			"\t\t{\n"\
			"\t\t\t*pmatchLen=maxLen;\n"\
			"\t\t\treturn 1;\n"\
			"\t\t}\n"\
			"\t}\n"\
			"\treturn 0;\n"\
			"}\n",fp);\
}\

#define DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(NAME) static void builtin_match_##NAME##_print_c_match_cond(void* c,FILE* fp)\
{\
	fputs("builtin_match_"#NAME"(start,*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,outerCtx,",fp);\
	SymbolNumberCtx* ctx=c;\
	if(ctx->start)\
	{\
		fputc('\"',fp);\
		print_string_in_hex(ctx->start,fp);\
		fputc('\"',fp);\
		fprintf(fp,",%lu",ctx->start->size);\
	}\
	else\
		fputs("NULL,0",fp);\
	fputc(')',fp);\
}\

DEFINE_LISP_NUMBER_PRINT_SRC(s_dint,fklIsDecInt);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_dint);

DEFINE_LISP_NUMBER_PRINT_SRC(s_xint,fklIsHexInt);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_xint);

DEFINE_LISP_NUMBER_PRINT_SRC(s_oint,fklIsOctInt);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_oint);

DEFINE_LISP_NUMBER_PRINT_SRC(s_dfloat,fklIsDecFloat);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_dfloat);

DEFINE_LISP_NUMBER_PRINT_SRC(s_xfloat,fklIsHexFloat);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_xfloat);

static int builtin_match_s_xint_func(void* c
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	SYMBOL_NUMBER_MATCH(fklIsHexInt);
}

static int builtin_match_s_oint_func(void* c
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	SYMBOL_NUMBER_MATCH(fklIsOctInt);
}

static int builtin_match_s_dfloat_func(void* c
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	SYMBOL_NUMBER_MATCH(fklIsDecFloat);
}

static int builtin_match_s_xfloat_func(void* c
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	SYMBOL_NUMBER_MATCH(fklIsHexFloat);
}

#undef SYMBOL_NUMBER_MATCH

static const FklLalrBuiltinMatch builtin_match_s_dint=
{
	.name="s-dint",
	.match=builtin_match_s_dint_func,
	.ctx_global_create=s_number_ctx_global_create,
	.ctx_cmp=s_number_ctx_cmp,
	.ctx_equal=s_number_ctx_equal,
	.ctx_hash=s_number_ctx_hash,
	.ctx_destroy=s_number_ctx_destroy,
	.print_src=builtin_match_s_dint_print_src,
	.print_c_match_cond=builtin_match_s_dint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_xint=
{
	.name="s-xint",
	.match=builtin_match_s_xint_func,
	.ctx_global_create=s_number_ctx_global_create,
	.ctx_cmp=s_number_ctx_cmp,
	.ctx_equal=s_number_ctx_equal,
	.ctx_hash=s_number_ctx_hash,
	.ctx_destroy=s_number_ctx_destroy,
	.print_src=builtin_match_s_xint_print_src,
	.print_c_match_cond=builtin_match_s_xint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_oint=
{
	.name="s-oint",
	.match=builtin_match_s_oint_func,
	.ctx_global_create=s_number_ctx_global_create,
	.ctx_cmp=s_number_ctx_cmp,
	.ctx_equal=s_number_ctx_equal,
	.ctx_hash=s_number_ctx_hash,
	.ctx_destroy=s_number_ctx_destroy,
	.print_src=builtin_match_s_oint_print_src,
	.print_c_match_cond=builtin_match_s_oint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_dfloat=
{
	.name="s-dfloat",
	.match=builtin_match_s_dfloat_func,
	.ctx_global_create=s_number_ctx_global_create,
	.ctx_cmp=s_number_ctx_cmp,
	.ctx_equal=s_number_ctx_equal,
	.ctx_hash=s_number_ctx_hash,
	.ctx_destroy=s_number_ctx_destroy,
	.print_src=builtin_match_s_dfloat_print_src,
	.print_c_match_cond=builtin_match_s_dfloat_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_xfloat=
{
	.name="s-xfloat",
	.match=builtin_match_s_xfloat_func,
	.ctx_global_create=s_number_ctx_global_create,
	.ctx_cmp=s_number_ctx_cmp,
	.ctx_equal=s_number_ctx_equal,
	.ctx_hash=s_number_ctx_hash,
	.ctx_destroy=s_number_ctx_destroy,
	.print_src=builtin_match_s_xfloat_print_src,
	.print_c_match_cond=builtin_match_s_xfloat_print_c_match_cond,
};

typedef struct
{
	const FklGrammer* g;
	const FklString* start;
	const FklString* end;
}MatchSymbolCtx;

size_t fklQuotedStringMatch(const char* cstr,size_t restLen,const FklString* end)
{
	if(restLen<end->size)
		return 0;
	size_t matchLen=0;
	size_t len=0;
	for(;cstr[len];len++)
	{
		if(cstr[len]=='\\')
		{
			len++;
			continue;
		}
		if(fklStringCharBufMatch(end,&cstr[len],restLen-len))
		{
			matchLen+=len+end->size;
			return matchLen;
		}
	}
	return 0;
}

size_t fklQuotedCharBufMatch(const char* cstr,size_t restLen,const char* end,size_t end_size)
{
	if(restLen<end_size)
		return 0;
	size_t matchLen=0;
	size_t len=0;
	for(;cstr[len];len++)
	{
		if(cstr[len]=='\\')
		{
			len++;
			continue;
		}
		if(fklCharBufMatch(end,end_size,&cstr[len],restLen-len))
		{
			matchLen+=len+end_size;
			return matchLen;
		}
	}
	return 0;
}

static int builtin_match_symbol_func(void* c
		,const char* cstrStart
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	MatchSymbolCtx* ctx=c;
	const FklString* start=ctx->start;
	const FklString* end=ctx->end;
	if(start)
	{
		size_t matchLen=0;
		for(;;)
		{
			if(fklStringCharBufMatch(start,cstr,restLen-matchLen))
			{
				matchLen+=start->size;
				cstr+=start->size;
				size_t len=fklQuotedStringMatch(cstr,restLen-matchLen,end);
				if(!len)
					return 0;
				matchLen+=len;
				cstr+=len;
				continue;
			}
			size_t maxLen=get_max_non_term_length(ctx->g,outerCtx,cstrStart,cstr,restLen-matchLen);
			if((!matchLen&&!maxLen)
					||(!fklStringCharBufMatch(start,cstr+maxLen,restLen-maxLen-matchLen)
						&&(fklIsDecInt(cstr,maxLen)
							||fklIsOctInt(cstr,maxLen)
							||fklIsHexInt(cstr,maxLen)
							||fklIsDecFloat(cstr,maxLen)
							||fklIsHexFloat(cstr,maxLen))))
				return 0;
			matchLen+=maxLen;
			cstr+=maxLen;
			if(!fklStringCharBufMatch(start,cstr,restLen-matchLen))
				break;
		}
		*pmatchLen=matchLen;
		return matchLen!=0;
	}
	else
	{
		size_t maxLen=get_max_non_term_length(ctx->g,outerCtx,cstrStart,cstr,restLen);
		if(!maxLen
				||fklIsDecInt(cstr,maxLen)
				||fklIsOctInt(cstr,maxLen)
				||fklIsHexInt(cstr,maxLen)
				||fklIsDecFloat(cstr,maxLen)
				||fklIsHexFloat(cstr,maxLen))
			return 0;
		*pmatchLen=maxLen;
	}
	return 1;
}

static void builtin_match_symbol_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_symbol(const char* cstrStart\n"
			"\t\t,const char* cstr\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* pmatchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx\n"
			"\t\t,const char* start\n"
			"\t\t,size_t start_size\n"
			"\t\t,const char* end\n"
			"\t\t,size_t end_size)\n"
			"{\n"
			"\tif(start)\n"
			"\t{\n"
			"\t\tsize_t matchLen=0;\n"
			"\t\tfor(;;)\n"
			"\t\t{\n"
			"\t\t\tif(fklCharBufMatch(start,start_size,cstr,restLen-matchLen))\n"
			"\t\t\t{\n"
			"\t\t\t\tmatchLen+=start_size;\n"
			"\t\t\t\tcstr+=start_size;\n"
			"\t\t\t\tsize_t len=fklQuotedCharBufMatch(cstr,restLen-matchLen,end,end_size);\n"
			"\t\t\t\tif(!len)\n"
			"\t\t\t\t\treturn 0;\n"
			"\t\t\t\tmatchLen+=len;\n"
			"\t\t\t\tcstr+=len;\n"
			"\t\t\t\tcontinue;\n"
			"\t\t\t}\n"
			"\t\t\tsize_t maxLen=get_max_non_term_length(outerCtx,cstrStart,cstr,restLen-matchLen);\n"
			"\t\t\tif((!matchLen&&!maxLen)\n"
			"\t\t\t\t\t||(!fklCharBufMatch(start,start_size,cstr+maxLen,restLen-maxLen-matchLen)\n"
			"\t\t\t\t\t\t&&(fklIsDecInt(cstr,maxLen)\n"
			"\t\t\t\t\t\t\t||fklIsOctInt(cstr,maxLen)\n"
			"\t\t\t\t\t\t\t||fklIsHexInt(cstr,maxLen)\n"
			"\t\t\t\t\t\t\t||fklIsDecFloat(cstr,maxLen)\n"
			"\t\t\t\t\t\t\t||fklIsHexFloat(cstr,maxLen))))\n"
			"\t\t\t\treturn 0;\n"
			"\t\t\tmatchLen+=maxLen;\n"
			"\t\t\tcstr+=maxLen;\n"
			"\t\t\tif(!fklCharBufMatch(start,start_size,cstr,restLen-matchLen))\n"
			"\t\t\t\tbreak;\n"
			"\t\t}\n"
			"\t\t*pmatchLen=matchLen;\n"
			"\t\treturn matchLen!=0;\n"
			"\t}\n"
			"\telse\n"
			"\t{\n"
			"\t\tsize_t maxLen=get_max_non_term_length(outerCtx,cstrStart,cstr,restLen);\n"
			"\t\tif(!maxLen\n"
			"\t\t\t\t||fklIsDecInt(cstr,maxLen)\n"
			"\t\t\t\t||fklIsOctInt(cstr,maxLen)\n"
			"\t\t\t\t||fklIsHexInt(cstr,maxLen)\n"
			"\t\t\t\t||fklIsDecFloat(cstr,maxLen)\n"
			"\t\t\t\t||fklIsHexFloat(cstr,maxLen))\n"
			"\t\t\treturn 0;\n"
			"\t\t*pmatchLen=maxLen;\n"
			"\t}\n"
			"\treturn 1;\n"
			"}\n",fp);
}

static void builtin_match_symbol_print_c_match_cond(void* c,FILE* fp)
{
	MatchSymbolCtx* ctx=c;
	const FklString* start=ctx->start;
	const FklString* end=ctx->end;
	fputs("builtin_match_symbol(start,*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,outerCtx,",fp);
	fputc('\"',fp);
	print_string_in_hex(start,fp);
	fprintf(fp,"\",%lu,\"",start->size);
	print_string_in_hex(end,fp);
	fprintf(fp,"\",%lu)",end->size);
}

static int builtin_match_symbol_cmp(const void* c0,const void* c1)
{
	const MatchSymbolCtx* ctx0=c0;
	const MatchSymbolCtx* ctx1=c1;
	if(ctx0->start==ctx1->start&&ctx0->end==ctx1->end)
		return 0;
	if(ctx0->start==NULL&&ctx1->start)
		return -1;
	else if(ctx1->start==NULL)
		return 1;
	else
	{
		int r=fklStringCmp(ctx0->start,ctx1->start);
		if(r)
			return r;
		return fklStringCmp(ctx0->end,ctx1->end);
	}
}

static int builtin_match_symbol_equal(const void* c0,const void* c1)
{
	const MatchSymbolCtx* ctx0=c0;
	const MatchSymbolCtx* ctx1=c1;
	if(ctx0->start==ctx1->start&&ctx0->end==ctx1->end)
		return 1;
	else if(ctx0->start||ctx1->start)
		return !fklStringCmp(ctx0->start,ctx1->start)&&!fklStringCmp(ctx0->end,ctx1->end);
	return 0;
}

static void builtin_match_symbol_destroy(void* c)
{
	free(c);
}

static void* builtin_match_symbol_global_create(size_t idx
		,FklGrammerProduction* prod
		,FklGrammer* g
		,int* failed)
{
	if(prod->len>3||idx!=0)
	{
		*failed=1;
		return NULL;
	}
	const FklString* start=NULL;
	const FklString* end=NULL;
	if(prod->len==2)
	{
		if(!prod->syms[1].isterm
				||prod->syms[1].isbuiltin)
		{
			*failed=1;
			return NULL;
		}
		start=fklGetSymbolWithId(prod->syms[1].nt,g->terminals)->symbol;
		end=start;
	}
	else if(prod->len==3)
	{
		if(!prod->syms[1].isterm
				||prod->syms[1].isbuiltin
				||!prod->syms[2].isterm
				||prod->syms[2].isbuiltin)
		{
			*failed=1;
			return NULL;
		}
		start=fklGetSymbolWithId(prod->syms[1].nt,g->terminals)->symbol;
		end=fklGetSymbolWithId(prod->syms[2].nt,g->terminals)->symbol;
	}
	prod->len=1;
	MatchSymbolCtx* ctx=(MatchSymbolCtx*)malloc(sizeof(MatchSymbolCtx));
	FKL_ASSERT(ctx);
	ctx->start=start;
	ctx->end=end;
	ctx->g=g;
	if(!g->sortedTerminals)
	{
		size_t num=g->terminals->num;
		g->sortedTerminalsNum=num;
		const FklString** terms=(const FklString**)malloc(sizeof(FklString*)*num);
		FKL_ASSERT(terms);
		FklSymTabNode** symList=g->terminals->list;
		for(size_t i=0;i<num;i++)
			terms[i]=symList[i]->symbol;
		qsort(terms,num,sizeof(FklString*),string_len_cmp);
		g->sortedTerminals=terms;
	}
	return ctx;
}

static uintptr_t builtin_match_symbol_hash(const void* c)
{
	const MatchSymbolCtx* ctx=c;
	return (uintptr_t)ctx->start+(uintptr_t)ctx->end;
}

static const FklLalrBuiltinMatch builtin_match_symbol=
{
	.name="symbol",
	.match=builtin_match_symbol_func,
	.ctx_cmp=builtin_match_symbol_cmp,
	.ctx_equal=builtin_match_symbol_equal,
	.ctx_hash=builtin_match_symbol_hash,
	.ctx_global_create=builtin_match_symbol_global_create,
	.ctx_destroy=builtin_match_symbol_destroy,
	.print_src=builtin_match_symbol_print_src,
	.print_c_match_cond=builtin_match_symbol_print_c_match_cond,
};

static inline int char_in_cstr(char c,const char* s)
{
	for(;*s;s++)
		if(*s==c)
			return 1;
	return 0;
}

static int builtin_match_esc_func(void* c
		,const char* cstrStart
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	static const char* escape_char=FKL_ESCAPE_CHARS;
	size_t matchLen=0;
	if(isdigit(*cstr))
		return 0;
	if(restLen&&(!isspace(*cstr)||char_in_cstr(toupper(*cstr),escape_char)))
		matchLen++;
	*pmatchLen=matchLen;
	return 1;
}

DEFINE_DEFAULT_C_MATCH_COND(esc);

static void builtin_match_esc_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static inline int char_in_cstr(char c,const char* s)\n"
			"{\n"
			"\tfor(;*s;s++)\n"
			"\t\tif(*s==c)\n"
			"\t\t\treturn 1;\n"
			"\treturn 0;\n"
			"}\n\n"
			"static int builtin_match_esc(const char* cstrStart\n"
			"\t\t,const char* cstr\n"
			"\t\t,size_t restLen\n"
			"\t\t,size_t* pmatchLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
			"{\n"
			"\tstatic const char* escape_char=FKL_ESCAPE_CHARS;\n"
			"\tsize_t matchLen=0;\n"
			"\tif(isdigit(*cstr))\n"
			"\t\treturn 0;\n"
			"\tif(restLen&&(!isspace(*cstr)||char_in_cstr(toupper(*cstr),escape_char)))\n"
			"\t\tmatchLen++;\n"
			"\t*pmatchLen=matchLen;\n"
			"\treturn 1;\n"
			"}\n"
			,fp);
}

static const FklLalrBuiltinMatch builtin_match_esc=
{
	.name="esc",
	.match=builtin_match_esc_func,
	.print_src=builtin_match_esc_print_src,
	.print_c_match_cond=builtin_match_esc_print_c_match_cond,
};

typedef struct
{
	const FklString* start;
	const FklString* end;
}MatchStringCtx;

static int builtin_match_string_cmp(const void* c0,const void* c1)
{
	const MatchStringCtx* ctx0=c0;
	const MatchStringCtx* ctx1=c1;
	if(ctx0->start==ctx1->start&&ctx0->end==ctx1->end)
		return 0;
	if(ctx0->start==NULL&&ctx1->start)
		return -1;
	else if(ctx1->start==NULL)
		return 1;
	else
	{
		int r=fklStringCmp(ctx0->start,ctx1->start);
		if(r)
			return r;
		return fklStringCmp(ctx0->end,ctx1->end);
	}
}

static int builtin_match_string_equal(const void* c0,const void* c1)
{
	const MatchStringCtx* ctx0=c0;
	const MatchStringCtx* ctx1=c1;
	if(ctx0->start==ctx1->start&&ctx0->end==ctx1->end)
		return 1;
	else if(ctx0->start||ctx1->start)
		return !fklStringCmp(ctx0->start,ctx1->start)&&!fklStringCmp(ctx0->end,ctx1->end);
	return 0;
}

static void builtin_match_string_destroy(void* c)
{
	free(c);
}

static uintptr_t builtin_match_string_hash(const void* c)
{
	const MatchStringCtx* ctx=c;
	return (uintptr_t)ctx->start+(uintptr_t)ctx->end;
}

static void* builtin_match_string_global_create(size_t idx
		,FklGrammerProduction* prod
		,FklGrammer* g
		,int* failed)
{
	if(prod->len<2||prod->len>3||idx!=0)
	{
		*failed=1;
		return NULL;
	}
	const FklString* start=NULL;
	const FklString* end=NULL;
	if(prod->len==2)
	{
		if(!prod->syms[1].isterm
				||prod->syms[1].isbuiltin)
		{
			*failed=1;
			return NULL;
		}
		start=fklGetSymbolWithId(prod->syms[1].nt,g->terminals)->symbol;
		end=start;
	}
	else if(prod->len==3)
	{
		if(!prod->syms[1].isterm
				||prod->syms[1].isbuiltin
				||!prod->syms[2].isterm
				||prod->syms[2].isbuiltin)
		{
			*failed=1;
			return NULL;
		}
		start=fklGetSymbolWithId(prod->syms[1].nt,g->terminals)->symbol;
		end=fklGetSymbolWithId(prod->syms[2].nt,g->terminals)->symbol;
	}
	prod->len=1;
	MatchStringCtx* ctx=(MatchStringCtx*)malloc(sizeof(MatchStringCtx));
	FKL_ASSERT(ctx);
	ctx->start=start;
	ctx->end=end;
	return ctx;
}

static int builtin_match_string_func(void* c
		,const char* cstrStart
		,const char* cstr
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	MatchStringCtx* ctx=c;
	const FklString* start=ctx->start;
	const FklString* end=ctx->end;
	size_t matchLen=0;
	if(fklStringCharBufMatch(start,cstr,restLen))
	{
		matchLen+=start->size;
		cstr+=start->size;
		size_t len=fklQuotedStringMatch(cstr,restLen-matchLen,end);
		if(!len)
			return 0;
		matchLen+=len;
		cstr+=len;
		*pmatchLen=matchLen;
		return 1;
	}
	return 0;
}

static void builtin_match_string_print_src(const FklGrammer* g,FILE* fp)
{
	fputs("static int builtin_match_string(const char* cstrStart\n"
	"\t\t,const char* cstr\n"
	"\t\t,size_t restLen\n"
	"\t\t,size_t* pmatchLen\n"
	"\t\t,FklGrammerMatchOuterCtx* outerCtx\n"
	"\t\t,const char* start\n"
	"\t\t,size_t start_size\n"
	"\t\t,const char* end\n"
	"\t\t,size_t end_size)\n"
	"{\n"
	"\tsize_t matchLen=0;\n"
	"\tif(fklCharBufMatch(start,start_size,cstr,restLen))\n"
	"\t{\n"
	"\t\tmatchLen+=start_size;\n"
	"\t\tcstr+=start_size;\n"
	"\t\tsize_t len=fklQuotedCharBufMatch(cstr,restLen-matchLen,end,end_size);\n"
	"\t\tif(!len)\n"
	"\t\t\treturn 0;\n"
	"\t\tmatchLen+=len;\n"
	"\t\tcstr+=len;\n"
	"\t\t*pmatchLen=matchLen;\n"
	"\t\treturn 1;\n"
	"\t}\n"
	"\treturn 0;\n"
	"}\n",fp);
}

static void builtin_match_string_print_c_match_cond(void* c,FILE* fp)
{
	MatchStringCtx* ctx=c;
	const FklString* start=ctx->start;
	const FklString* end=ctx->end;
	fputs("builtin_match_string(start,*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,outerCtx,",fp);
	fputc('\"',fp);
	print_string_in_hex(start,fp);
	fprintf(fp,"\",%lu,\"",start->size);
	print_string_in_hex(end,fp);
	fprintf(fp,"\",%lu)",end->size);
}

static const FklLalrBuiltinMatch builtin_match_string=
{
	.name="string",
	.match=builtin_match_string_func,
	.ctx_cmp=builtin_match_string_cmp,
	.ctx_equal=builtin_match_string_equal,
	.ctx_hash=builtin_match_string_hash,
	.ctx_global_create=builtin_match_string_global_create,
	.ctx_destroy=builtin_match_string_destroy,
	.print_src=builtin_match_string_print_src,
	.print_c_match_cond=builtin_match_string_print_c_match_cond,
};

#undef DEFINE_DEFAULT_C_MATCH_COND
#undef DEFINE_LISP_NUMBER_PRINT_SRC
#undef DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND

static const struct BuiltinGrammerSymList
{
	const char* name;
	const FklLalrBuiltinMatch* t;
}builtin_grammer_sym_list[]=
{
	{"%any",        &builtin_match_any,        },
	{"%esc",        &builtin_match_esc,        },
	{"%dec3",       &builtin_match_dec3,       },
	{"%hex2",       &builtin_match_hex2,       },
	{"%oct3",       &builtin_match_oct3,       },
	{"%space",      &builtin_match_space,      },
	{"%qstr",       &builtin_match_qstr,       },
	{"%eol",        &builtin_match_eol,        },

	{"%dec-int",    &builtin_match_dec_int,    },
	{"%hex-int",    &builtin_match_hex_int,    },
	{"%oct-int",    &builtin_match_oct_int,    },
	{"%dec-float",  &builtin_match_dec_float,  },
	{"%hex-float",  &builtin_match_hex_float,  },
	{"%identifier", &builtin_match_identifier, },

	{"%s-dint",     &builtin_match_s_dint,     },
	{"%s-xint",     &builtin_match_s_xint,     },
	{"%s-oint",     &builtin_match_s_oint,     },
	{"%s-dfloat",   &builtin_match_s_dfloat,   },
	{"%s-xfloat",   &builtin_match_s_xfloat,   },
	{"%symbol",     &builtin_match_symbol,     },

	{"%string",     &builtin_match_string,     },
	{NULL,          NULL,                      },
};

static inline void init_builtin_grammer_sym_table(FklHashTable* s,FklSymbolTable* st)
{
	fklInitHashTable(s,&SidBuiltinHashMetaTable);
	for(const struct BuiltinGrammerSymList* l=&builtin_grammer_sym_list[0];l->name;l++)
	{
		FklSid_t id=fklAddSymbolCstr(l->name,st)->id;
		SidBuiltinHashItem i={.id=id,.t=l->t};
		fklGetOrPutHashItem(&i,s);
	}
}

static inline void clear_analysis_table(FklGrammer* g,size_t last)
{
	size_t end=last+1;
	FklAnalysisState* states=g->aTable.states;
	for(size_t i=0;i<end;i++)
	{
		FklAnalysisState* curState=&states[i];
		FklAnalysisStateAction* actions=curState->state.action;
		while(actions)
		{
			FklAnalysisStateAction* next=actions->next;
			free(actions);
			actions=next;
		}

		FklAnalysisStateGoto* gt=curState->state.gt;
		while(gt)
		{
			FklAnalysisStateGoto* next=gt->next;
			free(gt);
			gt=next;
		}
	}
	free(states);
	g->aTable.states=NULL;
	g->aTable.num=0;
}

static inline void destroy_ignore(FklGrammerIgnore* ig)
{
	size_t len=ig->len;
	for(size_t i=0;i<len;i++)
	{
		FklGrammerIgnoreSym* igs=&ig->ig[i];
		if(igs->isbuiltin)
			destroy_builtin_grammer_sym(&igs->b);
	}
	free(ig);
}

void fklDestroyGrammer(FklGrammer* g)
{
	fklUninitHashTable(&g->productions);
	fklUninitHashTable(&g->builtins);
	fklUninitHashTable(&g->nonterminals);
	fklUninitHashTable(&g->firstSets);
	fklDestroySymbolTable(g->terminals);
	clear_analysis_table(g,g->aTable.num-1);
	free(g->sortedTerminals);
	FklGrammerIgnore* ig=g->ignores;
	while(ig)
	{
		FklGrammerIgnore* next=ig->next;
		destroy_ignore(ig);
		ig=next;
	}
	free(g);
}

static inline int init_all_builtin_grammer_sym(FklGrammer* g)
{
	int failed=0;
	for(FklHashTableItem* pl=g->productions.first;pl;pl=pl->next)
	{
		ProdHashItem* i=(ProdHashItem*)pl->data;
		for(FklGrammerProduction* prod=i->prods;prod;prod=prod->next)
		{
			for(size_t i=prod->len;i>0;i--)
			{
				size_t idx=i-1;
				FklGrammerSym* u=&prod->syms[idx];
				if(u->isbuiltin)
				{
					if(!u->b.c)
						u->b.c=init_builtin_grammer_sym(u->b.t,idx,prod,g,&failed);
					else if(u->b.t->ctx_global_create)
					{
						destroy_builtin_grammer_sym(&u->b);
						u->b.c=init_builtin_grammer_sym(u->b.t,idx,prod,g,&failed);
					}
				}
				if(failed)
					break;
			}
		}
	}
	return failed;
}

static inline int add_ignore_to_grammer(FklGrammer* g,FklGrammerIgnore* ig)
{
	FklGrammerIgnore** pp=&g->ignores;
	for(;*pp;pp=&(*pp)->next)
	{
		FklGrammerIgnore* cur=*pp;
		FklGrammerIgnoreSym* igss0=cur->ig;
		FklGrammerIgnoreSym* igss1=ig->ig;
		size_t len=cur->len<ig->len?cur->len:ig->len;
		for(size_t i=0;i<len;i++)
		{
			FklGrammerIgnoreSym* igs0=&igss0[i];
			FklGrammerIgnoreSym* igs1=&igss1[i];
			if(igs0->isbuiltin==igs1->isbuiltin)
			{
				if(igs0->isbuiltin&&builtin_grammer_sym_equal(&igs0->b,&igs1->b))
					return 1;
				else if(igs0->str==igs1->str)
					return 1;
			}
			break;
		}
	}
	*pp=ig;
	return 0;
}

typedef struct
{
	FklSid_t left;
	FklHashTable first;
	int hasEpsilon;
}FirstSetItem;

static void first_set_item_set_val(void* d0,const void* d1)
{
	FirstSetItem* s=(FirstSetItem*)d0;
	*s=*(const FirstSetItem*)d1;
}

static void first_set_item_uninit(void* d)
{
	fklUninitHashTable(&((FirstSetItem*)d)->first);
}

static const FklHashTableMetaTable FirstSetHashMetaTable=
{
	.size=sizeof(FirstSetItem),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=fklSetSidKey,
	.__hashFunc=fklSidHashFunc,
	.__keyEqual=fklSidKeyEqual,
	.__setVal=first_set_item_set_val,
	.__uninitItem=first_set_item_uninit,
};

static inline int lalr_look_ahead_equal(const FklLalrItemLookAhead* la0,const FklLalrItemLookAhead* la1);
static inline uintptr_t lalr_look_ahead_hash_func(const FklLalrItemLookAhead* la);

static int look_ahead_equal(const void* d0,const void* d1)
{
	return lalr_look_ahead_equal((const FklLalrItemLookAhead*)d0
			,(const FklLalrItemLookAhead*)d1);
}

static void look_ahead_set_key(void* d0,const void* d1)
{
	*(FklLalrItemLookAhead*)d0=*(const FklLalrItemLookAhead*)d1;
}

static uintptr_t look_ahead_hash_func(const void* d)
{
	return lalr_look_ahead_hash_func((const FklLalrItemLookAhead*)d);
}

static const FklHashTableMetaTable LookAheadHashMetaTable=
{
	.size=sizeof(FklLalrItemLookAhead),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=look_ahead_set_key,
	.__setVal=look_ahead_set_key,
	.__hashFunc=look_ahead_hash_func,
	.__keyEqual=look_ahead_equal,
	.__uninitItem=fklDoNothingUnintHashItem,
};

static inline void compute_all_first_set(FklGrammer* g)
{
	FklHashTable* firsetSets=&g->firstSets;
	const FklSymbolTable* tt=g->terminals;

	for(const FklHashTableItem* sidl=g->nonterminals.first;sidl;sidl=sidl->next)
	{
		FklSid_t id=*(const FklSid_t*)sidl->data;
		FirstSetItem* item=fklPutHashItem(&id,firsetSets);
		item->hasEpsilon=0;
		fklInitHashTable(&item->first,&LookAheadHashMetaTable);
	}

	int change;
	do
	{
		change=0;
		for(const FklHashTableItem* leftProds=g->productions.first;leftProds;leftProds=leftProds->next)
		{
			const ProdHashItem* leftProd=(const ProdHashItem*)leftProds->data;
			if(leftProd->left==0)
				continue;
			FirstSetItem* firstItem=fklGetHashItem(&leftProd->left,firsetSets);
			const FklGrammerProduction* prods=leftProd->prods;
			for(;prods;prods=prods->next)
			{
				size_t len=prods->len;
				if(!len)
				{
					change|=firstItem->hasEpsilon!=1;
					firstItem->hasEpsilon=1;
					continue;
				}
				const FklGrammerSym* syms=prods->syms;
				for(size_t i=0;i<len;i++)
				{
					const FklGrammerSym* sym=&syms[i];
					if(sym->isterm)
					{
						if(sym->isbuiltin)
						{
							FklLalrItemLookAhead la=
							{
								.t=FKL_LALR_MATCH_BUILTIN,
								.delim=sym->delim,
								.b=sym->b,
							};
							if(!fklGetHashItem(&la,&firstItem->first))
							{
								change=1;
								fklPutHashItem(&la,&firstItem->first);
							}
							break;
						}
						else
						{
							const FklString* s=fklGetSymbolWithId(sym->nt,tt)->symbol;
							if(s->size==0)
							{
								if(i==len-1)
								{
									change|=firstItem->hasEpsilon!=1;
									firstItem->hasEpsilon=1;
								}
							}
							else
							{
								FklLalrItemLookAhead la=
								{
									.t=FKL_LALR_MATCH_STRING,
									.delim=sym->delim,
									.s=s,
								};
								if(!fklGetHashItem(&la,&firstItem->first))
								{
									change=1;
									fklPutHashItem(&la,&firstItem->first);
								}
								break;
							}
						}
					}
					else
					{
						const FirstSetItem* curFirstItem=fklGetHashItem(&sym->nt,firsetSets);
						for(const FklHashTableItem* syms=curFirstItem->first.first;syms;syms=syms->next)
						{
							const FklLalrItemLookAhead* la=(const FklLalrItemLookAhead*)syms->data;
							if(!fklGetHashItem(la,&firstItem->first))
							{
								change=1;
								fklPutHashItem(la,&firstItem->first);
							}
						}
						if(curFirstItem->hasEpsilon&&i==len-1)
						{
							change|=firstItem->hasEpsilon!=1;
							firstItem->hasEpsilon=1;
						}
						if(!curFirstItem->hasEpsilon)
							break;
					}
				}
			}
		}
	}while(change);
}

static inline FklGrammer* create_empty_lalr1_grammer(void)
{
	FklGrammer* r=(FklGrammer*)calloc(1,sizeof(FklGrammer));
	FKL_ASSERT(r);
	r->terminals=fklCreateSymbolTable();
	r->ignores=NULL;
	r->sortedTerminals=NULL;
	r->sortedTerminalsNum=0;
	fklInitSidSet(&r->nonterminals);
	fklInitHashTable(&r->firstSets,&FirstSetHashMetaTable);
	init_prod_hash_table(&r->productions);
	return r;
}

FklGrammer* fklCreateGrammerFromCstr(const char* str[],FklSymbolTable* st)
{
	FklGrammer* grammer=create_empty_lalr1_grammer();
	FklSymbolTable* tt=grammer->terminals;
	FklHashTable* builtins=&grammer->builtins;
	init_builtin_grammer_sym_table(builtins,st);
	for(;*str;str++)
	{
		if(**str=='+')
		{
			FklGrammerIgnore* ignore=create_grammer_ignore_from_cstr(*str,grammer,builtins,st,tt);
			if(!ignore)
			{
				fklDestroyGrammer(grammer);
				return NULL;
			}
			if(add_ignore_to_grammer(grammer,ignore))
			{
				destroy_ignore(ignore);
				fklDestroyGrammer(grammer);
				return NULL;
			}
		}
		else
		{
			FklGrammerProduction* prod=create_grammer_prod_from_cstr(*str,grammer,builtins,st,tt,NULL,NULL);
			if(add_prod_to_grammer(grammer,prod))
			{
				destroy_prod(prod);
				fklDestroyGrammer(grammer);
				return NULL;
			}
		}
	}
	if(init_all_builtin_grammer_sym(grammer))
	{
		fklDestroyGrammer(grammer);
		return NULL;
	}
	compute_all_first_set(grammer);
	return grammer;
}

FklGrammer* fklCreateGrammerFromCstrAction(const FklGrammerCstrAction pa[],FklSymbolTable* st)
{
	FklGrammer* grammer=create_empty_lalr1_grammer();
	FklSymbolTable* tt=grammer->terminals;
	FklHashTable* builtins=&grammer->builtins;
	init_builtin_grammer_sym_table(builtins,st);
	for(;pa->cstr;pa++)
	{
		const char* str=pa->cstr;
		if(*str=='+')
		{
			FklGrammerIgnore* ignore=create_grammer_ignore_from_cstr(str,grammer,builtins,st,tt);
			if(!ignore)
			{
				fklDestroyGrammer(grammer);
				return NULL;
			}
			if(add_ignore_to_grammer(grammer,ignore))
			{
				destroy_ignore(ignore);
				fklDestroyGrammer(grammer);
				return NULL;
			}
		}
		else
		{
			FklGrammerProduction* prod=create_grammer_prod_from_cstr(str,grammer,builtins,st,tt,pa->action_name,pa->func);
			if(add_prod_to_grammer(grammer,prod))
			{
				destroy_prod(prod);
				fklDestroyGrammer(grammer);
				return NULL;
			}
		}
	}
	if(init_all_builtin_grammer_sym(grammer))
	{
		fklDestroyGrammer(grammer);
		return NULL;
	}
	compute_all_first_set(grammer);
	return grammer;
}

static inline void print_prod_sym(FILE* fp,const FklGrammerSym* u,const FklSymbolTable* st,const FklSymbolTable* tt)
{
	if(u->isterm)
	{
		if(u->isbuiltin)
		{
			putc('%',fp);
			fputs(u->b.t->name,fp);
		}
		else
		{
			putc('#',fp);
			fklPrintString(fklGetSymbolWithId(u->nt,tt)->symbol,fp);
		}
	}
	else
	{
		putc('&',fp);
		fklPrintString(fklGetSymbolWithId(u->nt,st)->symbol,fp);
	}
}

static inline void print_string_as_dot(const uint8_t* str,char se,size_t size,FILE* out)
{
	uint64_t i=0;
	while(i<size)
	{
		unsigned int l=fklGetByteNumOfUtf8(&str[i],size-i);
		if(l==7)
		{
			uint8_t j=str[i];
			fprintf(out,"\\x");
			fprintf(out,"%02X",j);
			i++;
		}
		else if(l==1)
		{
			if(str[i]==se)
				fprintf(out,"\\%c",se);
			else if(str[i]=='"')
				fputs("\\\"",out);
			else if(str[i]=='\'')
				fputs("\\'",out);
			else if(str[i]=='\\')
				fputs("\\\\",out);
			else if(isgraph(str[i]))
				putc(str[i],out);
			else if(fklIsSpecialCharAndPrint(str[i],out));
			else
			{
				uint8_t j=str[i];
				fprintf(out,"\\x");
				fprintf(out,"%02X",j);
			}
			i++;
		}
		else
		{
			for(unsigned int j=0;j<l;j++)
				putc(str[i+j],out);
			i+=l;
		}
	}
}
static inline void print_prod_sym_as_dot(FILE* fp,const FklGrammerSym* u,const FklSymbolTable* st,const FklSymbolTable* tt)
{
	if(u->isterm)
	{
		if(u->isbuiltin)
		{
			fputc('%',fp);
			fputs(u->b.t->name,fp);
		}
		else
		{
			const FklString* str=fklGetSymbolWithId(u->nt,tt)->symbol;
			fputs("\\\'",fp);
			print_string_as_dot((const uint8_t*)str->str,'"',str->size,fp);
			fputs("\\\'",fp);
		}
	}
	else
	{
		const FklString* str=fklGetSymbolWithId(u->nt,st)->symbol;
		fputc('|',fp);
		print_string_as_dot((const uint8_t*)str->str,'|',str->size,fp);
		fputc('|',fp);
	}
}

static void lalr_item_set_key(void* d0,const void* d1)
{
	*(FklLalrItem*)d0=*(const FklLalrItem*)d1;
}

static inline int lalr_look_ahead_equal(const FklLalrItemLookAhead* la0,const FklLalrItemLookAhead* la1)
{
	if(la0->t!=la1->t
			||la0->delim!=la1->delim)
		return 0;
	switch(la0->t)
	{
		case FKL_LALR_MATCH_NONE:
		case FKL_LALR_MATCH_EOF:
			return 1;
			break;
		case FKL_LALR_MATCH_STRING:
			return la0->s==la1->s;
			break;
		case FKL_LALR_MATCH_BUILTIN:
			return builtin_grammer_sym_equal(&la0->b,&la1->b);
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	return 0;
}

static int lalr_item_equal(const void* d0,const void* d1)
{
	const FklLalrItem* i0=(const FklLalrItem*)d0;
	const FklLalrItem* i1=(const FklLalrItem*)d1;
	return i0->prod==i1->prod
		&&i0->idx==i1->idx
		&&lalr_look_ahead_equal(&i0->la,&i1->la);
}

static inline uintptr_t lalr_look_ahead_hash_func(const FklLalrItemLookAhead* la)
{
	uintptr_t rest=la->t==FKL_LALR_MATCH_STRING
		?(uintptr_t)la->s
		:(la->t==FKL_LALR_MATCH_BUILTIN
				?builtin_grammer_sym_hash(&la->b)
				:0);

	return rest+((uintptr_t)la->t)+(la->delim<<1);
}

static uintptr_t lalr_item_hash_func(const void* d)
{
	const FklLalrItem* i=(const FklLalrItem*)d;
	return (uintptr_t)i->prod+i->idx+lalr_look_ahead_hash_func(&i->la);
}

static const FklHashTableMetaTable LalrItemHashMetaTable=
{
	.size=sizeof(FklLalrItem),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=lalr_item_set_key,
	.__setVal=lalr_item_set_key,
	.__hashFunc=lalr_item_hash_func,
	.__keyEqual=lalr_item_equal,
	.__uninitItem=fklDoNothingUnintHashItem,
};

static inline void init_empty_item_set(FklHashTable* t)
{
	fklInitHashTable(t,&LalrItemHashMetaTable);
}

static inline FklGrammerSym* get_item_next(const FklLalrItem* item)
{
	if(item->idx>=item->prod->len)
		return NULL;
	return &item->prod->syms[item->idx];
}

static inline FklLalrItem lalr_item_init(FklGrammerProduction* prod,size_t idx,const FklLalrItemLookAhead* la)
{
	FklLalrItem item=
	{
		.prod=prod,
		.idx=idx,
	};
	if(la)
		item.la=*la;
	else
		item.la=FKL_LALR_MATCH_NONE_INIT;
	return item;
}

static inline FklLalrItem get_item_advance(const FklLalrItem* i)
{
	FklLalrItem item=
	{
		.prod=i->prod,
		.idx=i->idx+1,
		.la=i->la,
	};
	return item;
}

static inline int lalr_look_ahead_cmp(const FklLalrItemLookAhead* la0,const FklLalrItemLookAhead* la1)
{
	if(la0->t==la1->t)
	{
		switch(la0->t)
		{
			case FKL_LALR_MATCH_NONE:
			case FKL_LALR_MATCH_EOF:
				return 0;
				break;
			case FKL_LALR_MATCH_BUILTIN:
				{
					if(la0->b.t!=la1->b.t)
					{
						uintptr_t f0=(uintptr_t)la0->b.t;
						uintptr_t f1=(uintptr_t)la1->b.t;
						if(f0>f1)
							return 1;
						else if(f0<f1)
							return -1;
						else
							return 0;
					}
					else
						return builtin_grammer_sym_cmp(&la0->b,&la1->b);
				}
				break;
			case FKL_LALR_MATCH_STRING:
				return fklStringCmp(la0->s,la1->s);
				break;
			default:
				FKL_ASSERT(0);
				break;
		}
	}
	else
	{
		int t0=la0->t;
		int t1=la1->t;
		return t0>t1?1:-1;
	}
	return 0;
}

static inline int lalr_item_cmp(const FklLalrItem* i0,const FklLalrItem* i1)
{
	FklGrammerProduction* p0=i0->prod;
	FklGrammerProduction* p1=i1->prod;
	if(p0==p1)
	{
		if(i0->idx<i1->idx)
			return -1;
		else if(i0->idx>i1->idx)
			return 1;
		else
			return lalr_look_ahead_cmp(&i0->la,&i1->la);
	}
	else if(p0->left<p1->left)
		return -1;
	else if(p0->left>p1->left)
		return 1;
	else if(p0->len>p1->len)
		return -1;
	else if(p0->len<p1->len)
		return 1;
	else
	{
		size_t len=p0->len;
		FklGrammerSym* syms0=p0->syms;
		FklGrammerSym* syms1=p1->syms;
		for(size_t i=0;i<len;i++)
		{
			int r=grammer_sym_cmp(&syms0[i],&syms1[i]);
			if(r)
				return r;
		}
		return 0;
	}
}

static int lalr_item_qsort_cmp(const void* i0,const void* i1)
{
	return lalr_item_cmp((const FklLalrItem*)i0,(const FklLalrItem*)i1);
}

static inline void lalr_item_set_sort(FklHashTable* itemSet)
{
	size_t num=itemSet->num;
	FklLalrItem* item_array=(FklLalrItem*)malloc(sizeof(FklLalrItem)*num);
	FKL_ASSERT(item_array);

	size_t i=0;
	for(FklHashTableItem* l=itemSet->first;l;l=l->next,i++)
	{
		FklLalrItem* item=(FklLalrItem*)l->data;
		item_array[i]=*item;
	}
	qsort(item_array,num,sizeof(FklLalrItem),lalr_item_qsort_cmp);
	fklClearHashTable(itemSet);
	for(i=0;i<num;i++)
		fklPutHashItem(&item_array[i],itemSet);
	free(item_array);
}

static inline void lr0_item_set_closure(FklHashTable* itemSet,FklGrammer* g)
{
	int change;
	FklHashTable* sidSet=fklCreateSidSet();
	FklHashTable* changeSet=fklCreateSidSet();
	do
	{
		change=0;
		for(FklHashTableItem* l=itemSet->first;l;l=l->next)
		{
			FklLalrItem* i=(FklLalrItem*)l->data;
			FklGrammerSym* sym=get_item_next(i);
			if(sym&&!sym->isterm)
			{
				FklSid_t left=sym->nt;
				if(!fklGetHashItem(&left,sidSet))
				{
					change=1;
					fklPutHashItem(&left,sidSet);
					fklPutHashItem(&left,changeSet);
				}
			}
		}

		for(FklHashTableItem* lefts=changeSet->first;lefts;lefts=lefts->next)
		{
			FklSid_t left=*((FklSid_t*)lefts->data);
			FklGrammerProduction* prod=fklGetGrammerProductions(g,left);
			for(;prod;prod=prod->next)
			{
				FklLalrItem item=lalr_item_init(prod,0,NULL);
				fklPutHashItem(&item,itemSet);
			}
		}
		fklClearHashTable(changeSet);
	}while(change);
	fklDestroyHashTable(sidSet);
	fklDestroyHashTable(changeSet);
}

static inline void lr0_item_set_copy_and_closure(FklHashTable* dst,const FklHashTable* itemSet,FklGrammer* g)
{
	for(FklHashTableItem* il=itemSet->first;il;il=il->next)
	{
		FklLalrItem* item=(FklLalrItem*)il->data;
		fklPutHashItem(item,dst);
	}
	lr0_item_set_closure(dst,g);
}

static inline void init_first_item_set(FklHashTable* itemSet,FklGrammerProduction* prod)
{
	FklLalrItem item=lalr_item_init(prod,0,NULL);
	init_empty_item_set(itemSet);
	fklPutHashItem(&item,itemSet);
}

static inline void print_look_ahead(FILE* fp,const FklLalrItemLookAhead* la)
{
	switch(la->t)
	{
		case FKL_LALR_MATCH_STRING:
			putc('#',fp);
			fklPrintString(la->s,fp);
			break;
		case FKL_LALR_MATCH_EOF:
			fputs("$$",fp);
			break;
		case FKL_LALR_MATCH_BUILTIN:
			fputs("&%",fp);
			fputs(la->b.t->name,fp);
			break;
		case FKL_LALR_MATCH_NONE:
			fputs("()",fp);
			break;
	}
}

static inline void print_item(FILE* fp
		,const FklLalrItem* item
		,const FklSymbolTable* st
		,const FklSymbolTable* tt)
{
	size_t i=0;
	size_t idx=item->idx;
	FklGrammerProduction* prod=item->prod;
	size_t len=prod->len;
	FklGrammerSym* syms=prod->syms;
	if(prod->left)
		fklPrintString(fklGetSymbolWithId(prod->left,st)->symbol,fp);
	else
		fputs("S'",fp);
	fputs(" ->",fp);
	for(;i<idx;i++)
	{
		putc(' ',fp);
		print_prod_sym(fp,&syms[i],st,tt);
	}
	fputs(" *",fp);
	for(;i<len;i++)
	{
		putc(' ',fp);
		print_prod_sym(fp,&syms[i],st,tt);
	}
	fputs(" , ",fp);
	print_look_ahead(fp,&item->la);
}

void fklPrintItemSet(const FklHashTable* itemSet,const FklGrammer* g,const FklSymbolTable* st,FILE* fp)
{
	FklSymbolTable* tt=g->terminals;
	FklLalrItem* curItem=NULL;
	for(FklHashTableItem* list=itemSet->first;list;list=list->next)
	{
		FklLalrItem* item=(FklLalrItem*)list->data;
		if(!curItem||item->idx!=curItem->idx
				||item->prod!=curItem->prod)
		{
			if(curItem)
				putc('\n',fp);
			curItem=item;
			print_item(fp,curItem,st,tt);
		}
		else
		{
			fputs(" / ",fp);
			print_look_ahead(fp,&item->la);
		}
	}
	putc('\n',fp);
}

static void hash_grammer_sym_set_key(void* s0,const void* s1)
{
	*(FklGrammerSym*)s0=*(const FklGrammerSym*)s1;
}

static uintptr_t hash_grammer_sym_hash_func(const void* d)
{
	const FklGrammerSym* s=d;
	return grammer_sym_hash(s);
}

static int hash_grammer_sym_equal(const void* d0,const void* d1)
{
	return grammer_sym_equal((const FklGrammerSym*)d0,(const FklGrammerSym*)d1);
}

static const FklHashTableMetaTable GrammerSymMetaTable=
{
	.size=sizeof(FklGrammerSym),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=hash_grammer_sym_set_key,
	.__setVal=hash_grammer_sym_set_key,
	.__hashFunc=hash_grammer_sym_hash_func,
	.__uninitItem=fklDoNothingUnintHashItem,
	.__keyEqual=hash_grammer_sym_equal,
};

static inline void init_grammer_sym_set(FklHashTable* t)
{
	fklInitHashTable(t,&GrammerSymMetaTable);
}

static void item_set_set_val(void* d0,const void* d1)
{
	FklLalrItemSet* s=(FklLalrItemSet*)d0;
	*s=*(const FklLalrItemSet*)d1;
	s->links=NULL;
	s->spreads=NULL;
}

static uintptr_t item_set_hash_func(const void* d)
{
	const FklHashTable* i=(const FklHashTable*)d;
	uintptr_t v=0;
	for(FklHashTableItem* l=i->first;l;l=l->next)
	{
		FklLalrItem* i=(FklLalrItem*)l->data;
		v+=lalr_item_hash_func(i);
	}
	return v;
}

static int item_set_equal(const void* d0,const void* d1)
{
	const FklHashTable* s0=(const FklHashTable*)d0;
	const FklHashTable* s1=(const FklHashTable*)d1;
	if(s0->num!=s1->num)
		return 0;
	const FklHashTableItem* l0=s0->first;
	const FklHashTableItem* l1=s1->first;
	for(;l0;l0=l0->next,l1=l1->next)
		if(!lalr_item_equal(l0->data,l1->data))
			return 0;
	return 1;
}

static void item_set_uninit(void* d)
{
	FklLalrItemSet* s=(FklLalrItemSet*)d;
	fklUninitHashTable(&s->items);
	FklLalrItemSetLink* l=s->links;
	while(l)
	{
		FklLalrItemSetLink* next=l->next;
		free(l);
		l=next;
	}
	FklLookAheadSpreads* sp=s->spreads;
	while(sp)
	{
		FklLookAheadSpreads* next=sp->next;
		free(sp);
		sp=next;
	}
}

static void item_set_add_link(FklLalrItemSet* src,FklGrammerSym* sym,FklLalrItemSet* dst)
{
	FklLalrItemSetLink* l=(FklLalrItemSetLink*)malloc(sizeof(FklLalrItemSetLink));
	FKL_ASSERT(l);
	l->sym=*sym;
	l->dst=dst;
	l->next=src->links;
	src->links=l;
}

static void item_set_set_key(void* k0,const void* k1)
{
	*(FklHashTable*)k0=*(FklHashTable*)k1;
}

static const FklHashTableMetaTable ItemStateSetHashMetaTable=
{
	.size=sizeof(FklLalrItemSet),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=item_set_set_key,
	.__setVal=item_set_set_val,
	.__hashFunc=item_set_hash_func,
	.__keyEqual=item_set_equal,
	.__uninitItem=item_set_uninit,
};

static inline FklHashTable* create_item_state_set(void)
{
	return fklCreateHashTable(&ItemStateSetHashMetaTable);
}

static inline void lr0_item_set_goto(FklLalrItemSet* itemset
		,FklHashTable* itemsetSet
		,FklGrammer* g
		,FklPtrQueue* pending)
{
	FklHashTable checked;
	init_grammer_sym_set(&checked);
	FklHashTableItem* l;
	FklHashTable* items=&itemset->items;
	FklHashTable itemsClosure;
	init_empty_item_set(&itemsClosure);
	lr0_item_set_copy_and_closure(&itemsClosure,items,g);
	lalr_item_set_sort(&itemsClosure);
	for(l=itemsClosure.first;l;l=l->next)
	{
		FklLalrItem* i=(FklLalrItem*)l->data;
		FklGrammerSym* sym=get_item_next(i);
		if(sym)
			fklPutHashItem(sym,&checked);
	}
	for(l=checked.first;l;l=l->next)
	{
		FklHashTable newItems;
		init_empty_item_set(&newItems);
		FklGrammerSym* sym=(FklGrammerSym*)l->data;
		for(FklHashTableItem* l=itemsClosure.first;l;l=l->next)
		{
			FklLalrItem* i=(FklLalrItem*)l->data;
			FklGrammerSym* next=get_item_next(i);
			if(next&&hash_grammer_sym_equal(sym,next))
			{
				FklLalrItem item=get_item_advance(i);
				fklPutHashItem(&item,&newItems);
			}
		}
		FklLalrItemSet* itemsetptr=fklGetHashItem(&newItems,itemsetSet);
		if(!itemsetptr)
		{
			FklLalrItemSet itemset={.items=newItems,.links=NULL};
			itemsetptr=fklGetOrPutHashItem(&itemset,itemsetSet);
			fklPushPtrQueue(itemsetptr,pending);
		}
		else
			fklUninitHashTable(&newItems);
		item_set_add_link(itemset,sym,itemsetptr);
	}
	fklUninitHashTable(&itemsClosure);
	fklUninitHashTable(&checked);
}

FklHashTable* fklGenerateLr0Items(FklGrammer* grammer)
{
	FklHashTable* itemstate_set=create_item_state_set();
	FklSid_t left=0;
	FklGrammerProduction* prod=((ProdHashItem*)fklGetHashItem(&left,&grammer->productions))->prods;
	FklHashTable items;
	init_first_item_set(&items,prod);
	FklLalrItemSet itemset={.items=items,};
	FklLalrItemSet* itemsetptr=fklGetOrPutHashItem(&itemset,itemstate_set);
	FklPtrQueue pending;
	fklInitPtrQueue(&pending);
	fklPushPtrQueue(itemsetptr,&pending);
	while(!fklIsPtrQueueEmpty(&pending))
	{
		FklLalrItemSet* itemsetptr=fklPopPtrQueue(&pending);
		lr0_item_set_goto(itemsetptr,itemstate_set,grammer,&pending);
	}
	fklUninitPtrQueue(&pending);
	return itemstate_set;
}

static inline int get_first_set_from_first_sets(const FklGrammer* g
		,const FklGrammerProduction* prod
		,uint32_t idx
		,FklHashTable* first
		,FklHashTable* computedLeft)
{
	size_t len=prod->len;
	if(idx>=len)
		return 1;
	size_t lastIdx=len-1;
	FklSymbolTable* tt=g->terminals;
	int hasEpsilon=0;
	const FklHashTable* firstSets=&g->firstSets;
	for(uint32_t i=idx;i<len;i++)
	{
		FklGrammerSym* sym=&prod->syms[i];
		if(sym->isterm)
		{
			if(sym->isbuiltin)
			{
				FklLalrItemLookAhead la=
				{
					.t=FKL_LALR_MATCH_BUILTIN,
					.delim=sym->delim,
					.b=sym->b,
				};
				fklPutHashItem(&la,first);
				break;
			}
			else
			{
				FklString* s=fklGetSymbolWithId(sym->nt,tt)->symbol;
				if(s->size==0)
				{
					if(i==lastIdx)
						hasEpsilon=1;
				}
				else
				{
					FklLalrItemLookAhead la=
					{
						.t=FKL_LALR_MATCH_STRING,
						.delim=sym->delim,
						.s=s,
					};
					fklPutHashItem(&la,first);
					break;
				}
			}
		}
		else
		{
			const FirstSetItem* firstSetItem=fklGetHashItem(&sym->nt,firstSets);
			for(FklHashTableItem* symList=firstSetItem->first.first;symList;symList=symList->next)
			{
				const FklLalrItemLookAhead* la=(const FklLalrItemLookAhead*)symList->data;
				fklPutHashItem(la,first);
			}
			if(firstSetItem->hasEpsilon&&i==len-1)
				hasEpsilon=1;
			if(!firstSetItem->hasEpsilon)
				break;
		}
	}
	return hasEpsilon;
}

static inline int get_first_set(const FklGrammer* g
		,const FklGrammerProduction* prod
		,uint32_t idx
		,FklHashTable* first)
{
	FklHashTable computedLeft;
	fklInitSidSet(&computedLeft);
	int hasEpsilon=get_first_set_from_first_sets(g,prod,idx,first,&computedLeft);
	fklUninitHashTable(&computedLeft);
	return hasEpsilon;
}

static inline void get_la_first_set(const FklGrammer* g
		,const FklGrammerProduction* prod
		,uint32_t beta
		,FklLalrItemLookAhead* a
		,FklHashTable* first)
{
	int hasEpsilon=get_first_set(g,prod,beta,first);
	if(hasEpsilon)
		fklPutHashItem(a,first);
}

static inline void lr1_item_set_closure(FklHashTable* itemSet,FklGrammer* g)
{
	FklHashTable pendingSet;
	FklHashTable changeSet;
	FklHashTable first;
	fklInitHashTable(&pendingSet,&LalrItemHashMetaTable);
	fklInitHashTable(&changeSet,&LalrItemHashMetaTable);
	fklInitHashTable(&first,&LookAheadHashMetaTable);

	for(FklHashTableItem* l=itemSet->first;l;l=l->next)
	{
		FklLalrItem* i=(FklLalrItem*)l->data;
		fklPutHashItem(i,&pendingSet);
	}

	while(pendingSet.num)
	{
		for(FklHashTableItem* l=pendingSet.first;l;l=l->next)
		{
			FklLalrItem* i=(FklLalrItem*)l->data;
			FklGrammerSym* next=get_item_next(i);
			if(next&&!next->isterm)
			{
				uint32_t beta=i->idx+1;
				get_la_first_set(g,i->prod,beta,&i->la,&first);
				FklSid_t left=next->nt;
				FklGrammerProduction* prods=fklGetGrammerProductions(g,left);
				for(FklHashTableItem* first_list=first.first;first_list;first_list=first_list->next)
				{
					FklLalrItemLookAhead* a=(FklLalrItemLookAhead*)first_list->data;
					for(FklGrammerProduction* prod=prods;prod;prod=prod->next)
					{
						FklLalrItem newItem={.prod=prod,.la=*a,.idx=0};
						if(!fklGetHashItem(&newItem,itemSet))
						{
							fklPutHashItem(&newItem,itemSet);
							fklPutHashItem(&newItem,&changeSet);
						}
					}
				}
				fklClearHashTable(&first);
			}
		}

		fklClearHashTable(&pendingSet);
		for(FklHashTableItem* l=changeSet.first;l;l=l->next)
		{
			FklLalrItem* i=(FklLalrItem*)l->data;
			fklPutHashItem(i,&pendingSet);
		}

		fklClearHashTable(&changeSet);
	}
	fklUninitHashTable(&changeSet);
	fklUninitHashTable(&pendingSet);
	fklUninitHashTable(&first);
}

static inline void add_lookahead_spread(FklLalrItemSet* itemset
		,const FklLalrItem* src
		,FklHashTable* dstItems
		,const FklLalrItem* dst)
{
	FklLookAheadSpreads* sp=(FklLookAheadSpreads*)malloc(sizeof(FklLookAheadSpreads));
	FKL_ASSERT(sp);
	sp->next=itemset->spreads;
	sp->dstItems=dstItems;
	sp->src=*src;
	sp->dst=*dst;
	itemset->spreads=sp;
}

static inline void check_lookahead_self_generated_and_spread(FklGrammer* g
		,FklLalrItemSet* itemset)
{
	FklHashTable* items=&itemset->items;
	FklHashTable closure;
	init_empty_item_set(&closure);
	for(FklHashTableItem* il=items->first;il;il=il->next)
	{
		FklLalrItem* i=(FklLalrItem*)il->data;
		if(i->la.t==FKL_LALR_MATCH_NONE)
		{
			FklLalrItem item={.prod=i->prod,.idx=i->idx,.la=FKL_LALR_MATCH_NONE_INIT};
			fklPutHashItem(&item,&closure);
			lr1_item_set_closure(&closure,g);
			for(FklHashTableItem* cl=closure.first;cl;cl=cl->next)
			{
				FklLalrItem* i=(FklLalrItem*)cl->data;
				const FklGrammerSym* s=get_item_next(i);
				i->idx++;
				for(const FklLalrItemSetLink* x=itemset->links;x;x=x->next)
				{
					if(x->dst==itemset)
						continue;
					const FklGrammerSym* xsym=&x->sym;
					if(s&&grammer_sym_equal(s,xsym))
					{
						if(i->la.t==FKL_LALR_MATCH_NONE)
							add_lookahead_spread(itemset,&item,&x->dst->items,i);
						else
							fklPutHashItem(i,&x->dst->items);
					}
				}
			}
			fklClearHashTable(&closure);
		}
	}
	fklUninitHashTable(&closure);
}

static inline int lookahead_spread(FklLalrItemSet* itemset)
{
	int change=0;
	FklHashTable* items=&itemset->items;
	for(FklLookAheadSpreads* sp=itemset->spreads;sp;sp=sp->next)
	{
		FklLalrItem* srcItem=&sp->src;
		FklLalrItem* dstItem=&sp->dst;
		FklHashTable* dstItems=sp->dstItems;
		for(FklHashTableItem* il=items->first;il;il=il->next)
		{
			FklLalrItem* item=(FklLalrItem*)il->data;
			if(item->la.t!=FKL_LALR_MATCH_NONE
					&&item->prod==srcItem->prod
					&&item->idx==srcItem->idx)
			{
				FklLalrItem newItem=*dstItem;
				newItem.la=item->la;
				if(!fklGetHashItem(&newItem,dstItems))
				{
					change=1;
					fklPutHashItem(&newItem,dstItems);
				}
			}
		}
	}
	return change;
}

static inline void init_lalr_look_ahead(FklHashTable* lr0,FklGrammer* g)
{
	for(FklHashTableItem* isl=lr0->first;isl;isl=isl->next)
	{
		FklLalrItemSet* s=(FklLalrItemSet*)isl->data;
		check_lookahead_self_generated_and_spread(g,s);
	}
	FklHashTableItem* isl=lr0->first;
	FklLalrItemSet* s=(FklLalrItemSet*)isl->data;
	for(FklHashTableItem* il=s->items.first;il;il=il->next)
	{
		FklLalrItem* i=(FklLalrItem*)il->data;
		FklLalrItem item=*i;
		item.la=FKL_LALR_MATCH_EOF_INIT;
		fklPutHashItem(&item,&s->items);
	}
}

static inline void add_look_ahead_to_items(FklHashTable* items
		,FklGrammer* g)
{
	FklHashTable add;
	init_empty_item_set(&add);
	for(FklHashTableItem* il=items->first;il;il=il->next)
	{
		FklLalrItem* i=(FklLalrItem*)il->data;
		if(i->la.t!=FKL_LALR_MATCH_NONE)
			fklPutHashItem(i,&add);
	}
	fklClearHashTable(items);
	for(FklHashTableItem* il=add.first;il;il=il->next)
	{
		FklLalrItem* i=(FklLalrItem*)il->data;
		fklPutHashItem(i,items);
	}
	fklUninitHashTable(&add);
	lr1_item_set_closure(items,g);
	lalr_item_set_sort(items);
}

static inline void add_look_ahead_for_all_item_set(FklHashTable* lr0,FklGrammer* g)
{
	for(FklHashTableItem* isl=lr0->first;isl;isl=isl->next)
	{
		FklLalrItemSet* itemSet=(FklLalrItemSet*)isl->data;
		add_look_ahead_to_items(&itemSet->items,g);
	}
}

void fklLr0ToLalrItems(FklHashTable* lr0,FklGrammer* g)
{
	init_lalr_look_ahead(lr0,g);
	int change;
	do
	{
		change=0;
		for(FklHashTableItem* isl=lr0->first;isl;isl=isl->next)
		{
			FklLalrItemSet* s=(FklLalrItemSet*)isl->data;
			change|=lookahead_spread(s);
		}
	}while(change);
	add_look_ahead_for_all_item_set(lr0,g);
}

typedef struct
{
	FklLalrItemSet* set;
	size_t idx;
}ItemStateIdx;

static void itemsetcount_set_val(void* d0,const void* d1)
{
	*(ItemStateIdx*)d0=*(const ItemStateIdx*)d1;
}

static int itemsetcount_equal(const void* d0,const void* d1)
{
	return *(const void**)d0==*(const void**)d1;
}

static uintptr_t itemsetcount_hash_func(const void* d0)
{
	return (uintptr_t)(*(const void**)d0);
}

static const FklHashTableMetaTable ItemStateIdxHashMetaTable=
{
	.size=sizeof(ItemStateIdx),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=fklHashDefaultSetPtrKey,
	.__setVal=itemsetcount_set_val,
	.__keyEqual=itemsetcount_equal,
	.__hashFunc=itemsetcount_hash_func,
	.__uninitItem=fklDoNothingUnintHashItem,
};

static inline void print_look_ahead_as_dot(FILE* fp,const FklLalrItemLookAhead* la)
{
	switch(la->t)
	{
		case FKL_LALR_MATCH_STRING:
			{
				fputs("\\\'",fp);
				const FklString* str=la->s;
				print_string_as_dot((const uint8_t*)str->str,'\'',str->size,fp);
				fputs("\\\'",fp);
			}
			break;
		case FKL_LALR_MATCH_EOF:
			fputs("$$",fp);
			break;
		case FKL_LALR_MATCH_BUILTIN:
			fputs("%",fp);
			fputs(la->b.t->name,fp);
			break;
		case FKL_LALR_MATCH_NONE:
			fputs("()",fp);
			break;
	}
}

static inline void print_item_as_dot(FILE* fp
		,const FklLalrItem* item
		,const FklSymbolTable* st
		,const FklSymbolTable* tt)
{
	size_t i=0;
	size_t idx=item->idx;
	FklGrammerProduction* prod=item->prod;
	size_t len=prod->len;
	FklGrammerSym* syms=prod->syms;
	if(prod->left)
	{
		const FklString* str=fklGetSymbolWithId(prod->left,st)->symbol;
		print_string_as_dot((const uint8_t*)str->str,'"',str->size,fp);
	}
	else
		fputs("S'",fp);
	fputs(" ->",fp);
	for(;i<idx;i++)
	{
		putc(' ',fp);
		print_prod_sym_as_dot(fp,&syms[i],st,tt);
	}
	fputs(" *",fp);
	for(;i<len;i++)
	{
		putc(' ',fp);
		print_prod_sym_as_dot(fp,&syms[i],st,tt);
	}
	fputs(" , ",fp);
	print_look_ahead_as_dot(fp,&item->la);
}

static inline void print_item_set_as_dot(const FklHashTable* itemSet,const FklGrammer* g,const FklSymbolTable* st,FILE* fp)
{
	FklSymbolTable* tt=g->terminals;
	FklLalrItem* curItem=NULL;
	for(FklHashTableItem* list=itemSet->first;list;list=list->next)
	{
		FklLalrItem* item=(FklLalrItem*)list->data;
		if(!curItem||item->idx!=curItem->idx
				||item->prod!=curItem->prod)
		{
			if(curItem)
				fputs("\\l\\\n",fp);
			curItem=item;
			print_item_as_dot(fp,curItem,st,tt);
		}
		else
		{
			fputs(" / ",fp);
			print_look_ahead_as_dot(fp,&item->la);
		}
	}
	fputs("\\l\\\n",fp);
}

void fklPrintItemStateSetAsDot(const FklHashTable* i
		,const FklGrammer* g
		,const FklSymbolTable* st
		,FILE* fp)
{
	fputs("digraph {\n",fp);
	fputs("\trankdir=\"LR\"\n",fp);
	fputs("\tranksep=1\n",fp);
	FklHashTable idxTable;
	fklInitHashTable(&idxTable,&ItemStateIdxHashMetaTable);
	size_t idx=0;
	for(const FklHashTableItem* l=i->first;l;l=l->next,idx++)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->data;
		ItemStateIdx* c=fklPutHashItem(&s,&idxTable);
		c->idx=idx;
	}
	for(const FklHashTableItem* l=i->first;l;l=l->next)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->data;
		const FklHashTable* i=&s->items;
		ItemStateIdx* c=fklGetHashItem(&s,&idxTable);
		idx=c->idx;
		fprintf(fp,"\t\"I%lu\"[fontname=\"Courier\" nojustify=true shape=\"box\" label=\"I%lu\\l\\\n"
				,idx
				,idx);
		print_item_set_as_dot(i,g,st,fp);
		fputs("\"]\n",fp);
		for(FklLalrItemSetLink* l=s->links;l;l=l->next)
		{
			FklLalrItemSet* dst=l->dst;
			ItemStateIdx* c=fklGetHashItem(&dst,&idxTable);
			fprintf(fp,"\tI%lu->I%lu[fontname=\"Courier\" label=\"",idx,c->idx);
			print_prod_sym_as_dot(fp,&l->sym,st,g->terminals);
			fputs("\"]\n",fp);
		}
		putc('\n',fp);
	}
	fklUninitHashTable(&idxTable);
	fputs("}",fp);
}

static inline FklAnalysisStateAction* create_shift_action(const FklGrammerSym* sym
		,const FklSymbolTable* tt
		,FklAnalysisState* state)
{
	FklAnalysisStateAction* action=(FklAnalysisStateAction*)malloc(sizeof(FklAnalysisStateAction));
	FKL_ASSERT(action);
	action->next=NULL;
	action->action=FKL_ANALYSIS_SHIFT;
	action->state=state;
	if(sym->isbuiltin)
	{
		action->match.t=FKL_LALR_MATCH_BUILTIN;
		action->match.func=sym->b;
	}
	else
	{
		const FklString* s=fklGetSymbolWithId(sym->nt,tt)->symbol;
		action->match.t=FKL_LALR_MATCH_STRING;
		action->match.str=s;
	}
	return action;
}

static inline FklAnalysisStateGoto* create_state_goto(const FklGrammerSym* sym
		,const FklSymbolTable* tt
		,FklAnalysisStateGoto* next
		,FklAnalysisState* state)
{
	FklAnalysisStateGoto* gt=(FklAnalysisStateGoto*)malloc(sizeof(FklAnalysisStateGoto));
	FKL_ASSERT(gt);
	gt->next=next;
	gt->state=state;
	gt->nt=sym->nt;
	return gt;
}

static inline int lalr_look_ahead_and_action_match_equal(const FklAnalysisStateActionMatch* match,const FklLalrItemLookAhead* la)
{
	if(match->t==la->t)
	{
		switch(match->t)
		{
			case FKL_LALR_MATCH_STRING:
				return match->str==la->s;
				break;
			case FKL_LALR_MATCH_BUILTIN:
				return match->func.t==la->b.t&&builtin_grammer_sym_equal(&match->func,&la->b);
				break;
			case FKL_LALR_MATCH_EOF:
			case FKL_LALR_MATCH_NONE:
				return 0;
		}
	}
	return 0;
}

static int check_reduce_conflict(const FklAnalysisStateAction* actions,const FklLalrItemLookAhead* la)
{
	for(;actions;actions=actions->next)
		if(lalr_look_ahead_and_action_match_equal(&actions->match,la))
			return 1;
	return 0;
}

static inline void init_action_with_look_ahead(FklAnalysisStateAction* action,const FklLalrItemLookAhead* la)
{
	action->match.t=la->t;
	switch(action->match.t)
	{
		case FKL_LALR_MATCH_STRING:
			action->match.str=la->s;
			break;
		case FKL_LALR_MATCH_BUILTIN:
			action->match.func.t=la->b.t;
			action->match.func.c=la->b.c;
			break;
		case FKL_LALR_MATCH_NONE:
		case FKL_LALR_MATCH_EOF:
			break;
	}
}

static inline int add_reduce_action(FklAnalysisState* curState
		,const FklGrammerProduction* prod
		,const FklLalrItemLookAhead* la)
{
	if(check_reduce_conflict(curState->state.action,la))
		return 1;
	FklAnalysisStateAction* action=(FklAnalysisStateAction*)malloc(sizeof(FklAnalysisStateAction));
	FKL_ASSERT(action);
	init_action_with_look_ahead(action,la);
	if(prod->left==0)
		action->action=FKL_ANALYSIS_ACCEPT;
	else
	{
		action->action=FKL_ANALYSIS_REDUCE;
		action->prod=prod;
	}
	FklAnalysisStateAction** pa=&curState->state.action;
	if(la->t==FKL_LALR_MATCH_STRING)
	{
		const FklString* s=action->match.str;
		for(;*pa;pa=&(*pa)->next)
		{
			FklAnalysisStateAction* curAction=*pa;
			if(curAction->match.t!=FKL_LALR_MATCH_STRING
					||(curAction->match.t==FKL_LALR_MATCH_STRING
						&&s->size>curAction->match.str->size))
				break;
		}
		action->next=*pa;
		*pa=action;
	}
	else if(la->t==FKL_LALR_MATCH_EOF)
	{
		FklAnalysisStateAction** pa=&curState->state.action;
		for(;*pa;pa=&(*pa)->next);
		action->next=*pa;
		*pa=action;
	}
	else
	{
		FklAnalysisStateAction** pa=&curState->state.action;
		for(;*pa;pa=&(*pa)->next)
		{
			FklAnalysisStateAction* curAction=*pa;
			if(curAction->match.t!=FKL_LALR_MATCH_STRING)
				break;
		}
		action->next=*pa;
		*pa=action;
	}
	return 0;
}

static inline void add_shift_action(FklAnalysisState* curState
		,const FklGrammerSym* sym
		,const FklSymbolTable* tt
		,FklAnalysisState* dstState)
{
	FklAnalysisStateAction* action=create_shift_action(sym,tt,dstState);
	FklAnalysisStateAction** pa=&curState->state.action;
	if(action->match.t==FKL_LALR_MATCH_STRING)
	{
		const FklString* s=action->match.str;
		for(;*pa;pa=&(*pa)->next)
		{
			FklAnalysisStateAction* curAction=*pa;
			if(curAction->match.t!=FKL_LALR_MATCH_STRING
					||(curAction->match.t==FKL_LALR_MATCH_STRING
						&&s->size>curAction->match.str->size))
				break;
		}
	}
	else
		for(;*pa;pa=&(*pa)->next)
			if((*pa)->match.t==FKL_LALR_MATCH_EOF)
				break;
	action->next=*pa;
	*pa=action;
}

static int builtin_match_ignore_func(void* ctx
		,const char* start
		,const char* str
		,size_t restLen
		,size_t* pmatchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	FklGrammerIgnore* ig=(FklGrammerIgnore*)ctx;
	return ignore_match(ig,start,str,restLen,pmatchLen,outerCtx);
}

static inline void ignore_print_c_match_cond(const FklGrammerIgnore* ig,FILE* fp)
{
	fputc('(',fp);
	size_t len=ig->len;
	const FklGrammerIgnoreSym* igss=ig->ig;

	const FklGrammerIgnoreSym* igs=&igss[0];
	if(igs->isbuiltin)
		igs->b.t->print_c_match_cond(igs->b.c,fp);
	else
	{
		fputs("(matchLen=fklCharBufMatch(\"",fp);
		print_string_in_hex(igs->str,fp);
		fprintf(fp,"\",%lu,*in+otherMatchLen,*restLen-otherMatchLen))",igs->str->size);
	}
	fputs("&&((otherMatchLen+=matchLen)||1)",fp);

	for(size_t i=1;i<len;i++)
	{
		fputs("&&",fp);
		const FklGrammerIgnoreSym* igs=&igss[i];
		if(igs->isbuiltin)
			igs->b.t->print_c_match_cond(igs->b.c,fp);
		else
		{
			fputs("(matchLen+=fklCharBufMatch(\"",fp);
			print_string_in_hex(igs->str,fp);
			fprintf(fp,"\",%lu,*in+otherMatchLen,*restLen-otherMatchLen))",igs->str->size);
		}
		fputs("&&((otherMatchLen+=matchLen)||1)",fp);
	}
	fputs("&&((matchLen=otherMatchLen)||1)",fp);
	fputc(')',fp);
}

static void builtin_match_ignore_print_c_match_cond(void* c,FILE* fp)
{
	ignore_print_c_match_cond(c,fp);
}

static const FklLalrBuiltinMatch builtin_match_ignore=
{
	.name=NULL,
	.match=builtin_match_ignore_func,
	.ctx_create=NULL,
	.ctx_destroy=NULL,
	.ctx_global_create=NULL,
	.ctx_cmp=NULL,
	.ctx_hash=NULL,
	.print_src=default_builtin_match_print_src,
	.print_c_match_cond=builtin_match_ignore_print_c_match_cond,
};

static inline FklAnalysisStateAction* create_builtin_ignore_action(FklGrammer* g,FklGrammerIgnore* ig)
{
	FklAnalysisStateAction* action=(FklAnalysisStateAction*)malloc(sizeof(FklAnalysisStateAction));
	FKL_ASSERT(action);
	action->next=NULL;
	action->action=FKL_ANALYSIS_IGNORE;
	action->state=NULL;
	if(ig->len==1)
	{
		FklGrammerIgnoreSym* igs=&ig->ig[0];
		action->match.t=igs->isbuiltin?FKL_LALR_MATCH_BUILTIN:FKL_LALR_MATCH_STRING;
		if(igs->isbuiltin)
			action->match.func=igs->b;
		else
			action->match.str=igs->str;
	}
	else
	{
		action->match.t=FKL_LALR_MATCH_BUILTIN;
		action->match.func.t=&builtin_match_ignore;
		action->match.func.c=ig;
	}
	return action;
}

static inline void add_ignore_action(FklGrammer* g,FklAnalysisState* curState)
{
	for(FklGrammerIgnore* ig=g->ignores;ig;ig=ig->next)
	{
		FklAnalysisStateAction* action=create_builtin_ignore_action(g,ig);
		FklAnalysisStateAction** pa=&curState->state.action;
		for(;*pa;pa=&(*pa)->next)
			if((*pa)->match.t==FKL_LALR_MATCH_EOF)
				break;
		action->next=*pa;
		*pa=action;
	}
}

int fklGenerateLalrAnalyzeTable(FklGrammer* grammer,FklHashTable* states)
{
	int hasConflict=0;
	grammer->aTable.num=states->num;
	FklSymbolTable* tt=grammer->terminals;
	FklAnalysisState* astates=(FklAnalysisState*)malloc(sizeof(FklAnalysisState)*states->num);
	FKL_ASSERT(astates);
	grammer->aTable.states=astates;
	FklHashTable idxTable;
	fklInitHashTable(&idxTable,&ItemStateIdxHashMetaTable);
	size_t idx=0;
	for(const FklHashTableItem* l=states->first;l;l=l->next,idx++)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->data;
		ItemStateIdx* c=fklPutHashItem(&s,&idxTable);
		c->idx=idx;
	}
	idx=0;
	for(const FklHashTableItem* l=states->first;l;l=l->next,idx++)
	{
		int skip_space=0;
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->data;
		FklAnalysisState* curState=&astates[idx];
		curState->builtin=0;
		curState->func=NULL;
		curState->state.action=NULL;
		curState->state.gt=NULL;
		const FklHashTable* items=&s->items;
		for(FklLalrItemSetLink* ll=s->links;ll;ll=ll->next)
		{
			const FklGrammerSym* sym=&ll->sym;
			ItemStateIdx* c=fklGetHashItem(&ll->dst,&idxTable);
			size_t dstIdx=c->idx;
			FklAnalysisState* dstState=&astates[dstIdx];
			if(sym->isterm)
				add_shift_action(curState,sym,tt,dstState);
			else
				curState->state.gt=create_state_goto(sym,tt,curState->state.gt,dstState);
		}
		for(const FklHashTableItem* il=items->first;il;il=il->next)
		{
			FklLalrItem* item=(FklLalrItem*)il->data;
			FklGrammerSym* sym=get_item_next(item);
			if(sym)
				skip_space=sym->delim;
			else
			{
				hasConflict=add_reduce_action(curState,item->prod,&item->la);
				if(hasConflict)
				{
					clear_analysis_table(grammer,idx);
					goto break_loop;
				}
				if(item->la.delim)
					skip_space=1;
			}
		}
		if(skip_space)
			add_ignore_action(grammer,curState);
	}

break_loop:
	fklUninitHashTable(&idxTable);
	return hasConflict;
}

static inline void print_look_ahead_of_analysis_table(FILE* fp,const FklAnalysisStateActionMatch* match)
{
	switch(match->t)
	{
		case FKL_LALR_MATCH_STRING:
			{
				fputc('\'',fp);
				const FklString* str=match->str;
				print_string_as_dot((const uint8_t*)str->str,'\'',str->size,fp);
				fputc('\'',fp);
			}
			break;
		case FKL_LALR_MATCH_EOF:
			fputs("$$",fp);
			break;
		case FKL_LALR_MATCH_BUILTIN:
			fputs("%",fp);
			fputs(match->func.t->name,fp);
			break;
		case FKL_LALR_MATCH_NONE:
			fputs("()",fp);
			break;
	}
}

void fklPrintAnalysisTable(const FklGrammer* grammer,const FklSymbolTable* st,FILE* fp)
{
	size_t num=grammer->aTable.num;
	FklAnalysisState* states=grammer->aTable.states;

	for(size_t i=0;i<num;i++)
	{
		fprintf(fp,"%lu: ",i);
		FklAnalysisState* curState=&states[i];
		for(FklAnalysisStateAction* actions=curState->state.action
				;actions
				;actions=actions->next)
		{
			switch(actions->action)
			{
				case FKL_ANALYSIS_SHIFT:
					fputs("S(",fp);
					print_look_ahead_of_analysis_table(fp,&actions->match);
					{
						uintptr_t idx=actions->state-states;
						fprintf(fp," , %lu )",idx);
					}
					break;
				case FKL_ANALYSIS_REDUCE:
					fputs("R(",fp);
					print_look_ahead_of_analysis_table(fp,&actions->match);
					fprintf(fp," , %lu )",actions->prod->idx);
					break;
				case FKL_ANALYSIS_ACCEPT:
					fputs("acc(",fp);
					print_look_ahead_of_analysis_table(fp,&actions->match);
					fputc(')',fp);
					break;
				case FKL_ANALYSIS_IGNORE:
					break;
			}
			fputc('\t',fp);
		}
		fputs("|\t",fp);
		for(FklAnalysisStateGoto* gt=curState->state.gt;gt;gt=gt->next)
		{
			uintptr_t idx=gt->state-states;
			fputc('(',fp);
			fklPrintRawSymbol(fklGetSymbolWithId(gt->nt,st)->symbol,fp);
			fprintf(fp," , %lu)",idx);
			fputc('\t',fp);
		}
		fputc('\n',fp);
	}
}

static void action_match_set_key(void* d0,const void* d1)
{
	*(FklAnalysisStateActionMatch*)d0=*(FklAnalysisStateActionMatch*)d1;
}

static uintptr_t action_match_hash_func(const void* d0)
{
	const FklAnalysisStateActionMatch* match=(const FklAnalysisStateActionMatch*)d0;
	switch(match->t)
	{
		case FKL_LALR_MATCH_NONE:
			return 0;
			break;
		case FKL_LALR_MATCH_EOF:
			return 1;
			break;
		case FKL_LALR_MATCH_STRING:
			return (uintptr_t)match->str;
			break;
		case FKL_LALR_MATCH_BUILTIN:
			return (uintptr_t)match->func.t+(uintptr_t)match->func.c;
			break;
	}
	return 0;
}

static inline int state_action_match_equal(const FklAnalysisStateActionMatch* m0,const FklAnalysisStateActionMatch* m1)
{
	if(m0->t==m1->t)
	{
		switch(m0->t)
		{
			case FKL_LALR_MATCH_NONE:
			case FKL_LALR_MATCH_EOF:
				return 1;
				break;
			case FKL_LALR_MATCH_STRING:
				return m0->str==m1->str;
				break;
			case FKL_LALR_MATCH_BUILTIN:
				return m0->func.t==m1->func.t
					&&m0->func.c==m1->func.c;
		}
	}
	return 0;
}

static int action_match_equal(const void* d0,const void* d1)
{
	return state_action_match_equal((const FklAnalysisStateActionMatch*)d0
			,(const FklAnalysisStateActionMatch*)d1);
}

static const FklHashTableMetaTable ActionMatchHashMetaTable=
{
	.size=sizeof(FklAnalysisStateActionMatch),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=action_match_set_key,
	.__setVal=action_match_set_key,
	.__hashFunc=action_match_hash_func,
	.__keyEqual=action_match_equal,
	.__uninitItem=fklDoNothingUnintHashItem,
};

static inline void init_analysis_table_header(FklHashTable* la
		,FklHashTable* nt
		,FklAnalysisState* states
		,size_t stateNum)
{
	fklInitHashTable(la,&ActionMatchHashMetaTable);
	fklInitSidSet(nt);

	for(size_t i=0;i<stateNum;i++)
	{
		FklAnalysisState* curState=&states[i];
		for(FklAnalysisStateAction* action=curState->state.action;action;action=action->next)
			if(action->action!=FKL_ANALYSIS_IGNORE)
				fklPutHashItem(&action->match,la);
		for(FklAnalysisStateGoto* gt=curState->state.gt;gt;gt=gt->next)
			fklPutHashItem(&gt->nt,nt);
	}
}

static inline void print_symbol_for_grapheasy(const FklString* stri,FILE* fp)
{
	size_t size=stri->size;
	const uint8_t* str=(uint8_t*)stri->str;
	size_t i=0;
	while(i<size)
	{
		unsigned int l=fklGetByteNumOfUtf8(&str[i],size-i);
		if(l==7)
		{
			uint8_t j=str[i];
			fprintf(fp,"\\x");
			fprintf(fp,"%02X",j);
			i++;
		}
		else if(l==1)
		{
			if(str[i]=='\\')
				fputs("\\\\",fp);
			else if(str[i]=='|')
				fputs("\\\\|",fp);
			else if(str[i]==']')
				fputs("\\]",fp);
			else if(isgraph(str[i]))
				putc(str[i],fp);
			else if(fklIsSpecialCharAndPrint(str[i],fp));
			else
			{
				uint8_t j=str[i];
				fprintf(fp,"\\x");
				fprintf(fp,"%02X",j);
			}
			i++;
		}
		else
		{
			for(unsigned int j=0;j<l;j++)
				putc(str[i+j],fp);
			i+=l;
		}
	}
}

static inline void print_string_for_grapheasy(const FklString* stri,FILE* fp)
{
	size_t size=stri->size;
	const uint8_t* str=(uint8_t*)stri->str;
	size_t i=0;
	while(i<size)
	{
		unsigned int l=fklGetByteNumOfUtf8(&str[i],size-i);
		if(l==7)
		{
			uint8_t j=str[i];
			fprintf(fp,"\\x");
			fprintf(fp,"%02X",j);
			i++;
		}
		else if(l==1)
		{
			if(str[i]=='\\')
				fputs("\\\\",fp);
			else if(str[i]=='#')
				fputs("\\#",fp);
			else if(str[i]=='|')
				fputs("\\|",fp);
			else if(str[i]==']')
				fputs("\\]",fp);
			else if(isgraph(str[i]))
				putc(str[i],fp);
			else if(fklIsSpecialCharAndPrint(str[i],fp));
			else
			{
				uint8_t j=str[i];
				fprintf(fp,"\\x");
				fprintf(fp,"%02X",j);
			}
			i++;
		}
		else
		{
			for(unsigned int j=0;j<l;j++)
				putc(str[i+j],fp);
			i+=l;
		}
	}
}

static inline void print_look_ahead_for_grapheasy(const FklLalrItemLookAhead* la,FILE* fp)
{
	switch(la->t)
	{
		case FKL_LALR_MATCH_STRING:
			{
				fputc('\'',fp);
				print_string_for_grapheasy(la->s,fp);
				fputc('\'',fp);
			}
			break;
		case FKL_LALR_MATCH_EOF:
			fputc('$',fp);
			break;
		case FKL_LALR_MATCH_BUILTIN:
			fputc('%',fp);
			fputs(la->b.t->name,fp);
			break;
		case FKL_LALR_MATCH_NONE:
			fputs("()",fp);
			break;
	}
}

static inline void print_table_header_for_grapheasy(const FklHashTable* la
		,const FklHashTable* sid
		,const FklSymbolTable* st
		,FILE* fp)
{
	fputs("\\n|",fp);
	for(FklHashTableItem* al=la->first;al;al=al->next)
	{
		FklLalrItemLookAhead* la=(FklLalrItemLookAhead*)al->data;
		print_look_ahead_for_grapheasy(la,fp);
		fputc('|',fp);
	}
	fputs("\\n|\\n",fp);
	for(FklHashTableItem* sl=sid->first;sl;sl=sl->next)
	{
		fputc('|',fp);
		FklSid_t id=*(FklSid_t*)sl->data;
		fputs("\\|",fp);
		print_symbol_for_grapheasy(fklGetSymbolWithId(id,st)->symbol,fp);
		fputs("\\|",fp);
	}
	fputs("||\n",fp);
}

static inline FklAnalysisStateAction* find_action(FklAnalysisStateAction* action
		,const FklAnalysisStateActionMatch* match)
{
	for(;action;action=action->next)
	{
		if(state_action_match_equal(match,&action->match))
			return action;
	}
	return NULL;
}

static inline FklAnalysisStateGoto* find_gt(FklAnalysisStateGoto* gt,FklSid_t id)
{
	for(;gt;gt=gt->next)
	{
		if(gt->nt==id)
			return gt;
	}
	return NULL;
}

void fklPrintAnalysisTableForGraphEasy(const FklGrammer* g
		,const FklSymbolTable* st
		,FILE* fp)
{
	size_t num=g->aTable.num;
	FklAnalysisState* states=g->aTable.states;

	fputs("[\n",fp);

	FklHashTable laTable;
	FklHashTable sidSet;
	init_analysis_table_header(&laTable,&sidSet,states,num);

	print_table_header_for_grapheasy(&laTable,&sidSet,st,fp);

	FklHashTableItem* laList=laTable.first;
	FklHashTableItem* sidList=sidSet.first;
	for(size_t i=0;i<num;i++)
	{
		const FklAnalysisState* curState=&states[i];
		fprintf(fp,"%lu: |",i);
		for(FklHashTableItem* al=laList;al;al=al->next)
		{
			FklAnalysisStateActionMatch* match=(FklAnalysisStateActionMatch*)al->data;
			FklAnalysisStateAction* action=find_action(curState->state.action,match);
			if(action)
			{
				switch(action->action)
				{
					case FKL_ANALYSIS_SHIFT:
						{
							uintptr_t idx=action->state-states;
							fprintf(fp,"s%lu",idx);
						}
						break;
					case FKL_ANALYSIS_REDUCE:
						fprintf(fp,"r%lu",action->prod->idx);
						break;
					case FKL_ANALYSIS_ACCEPT:
						fputs("acc",fp);
						break;
					case FKL_ANALYSIS_IGNORE:
						break;
				}
			}
			else
				fputs("\\n",fp);
			fputc('|',fp);
		}
		fputs("\\n|\\n",fp);
		for(FklHashTableItem* sl=sidList;sl;sl=sl->next)
		{
			fputc('|',fp);
			FklSid_t id=*(FklSid_t*)sl->data;
			FklAnalysisStateGoto* gt=find_gt(curState->state.gt,id);
			if(gt)
			{
				uintptr_t idx=gt->state-states;
				fprintf(fp,"%lu",idx);
			}
			else
				fputs("\\n",fp);
		}
		fputs("||\n",fp);
	}
	fputc(']',fp);
	fklUninitHashTable(&laTable);
	fklUninitHashTable(&sidSet);
}

static inline void print_get_max_non_term_length_prototype_to_c_file(FILE* fp)
{
	fputs("static inline size_t get_max_non_term_length(FklGrammerMatchOuterCtx*"
			",const char*"
			",const char*"
			",size_t);\n",fp);
}

static inline void print_get_max_non_term_length_to_c_file(const FklGrammer* g,FILE* fp)
{
	fputs("static inline size_t get_max_non_term_length(FklGrammerMatchOuterCtx* outerCtx\n"
			"\t\t,const char* start\n"
			"\t\t,const char* cur\n"
			"\t\t,size_t rLen)\n{\n"
			"\tif(rLen)\n\t{\n"
			"\t\tif(start==outerCtx->start&&cur==outerCtx->cur)\n\t\t\treturn outerCtx->maxNonterminalLen;\n"
			"\t\touterCtx->start=start;\n\t\touterCtx->cur=cur;\n"
			"\t\tsize_t len=0;\n"
			"\t\tsize_t matchLen=0;\n"
			"\t\tsize_t otherMatchLen=0;\n"
			"\t\tsize_t* restLen=&rLen;\n"
			"\t\tconst char** in=&cur;\n"
			"\t\t(void)otherMatchLen;\n"
			"\t\t(void)restLen;\n"
			"\t\t(void)in;\n"
			"\t\twhile(len<rLen)\n\t\t{\n"
			"\t\t\tif(",fp);
	if(g->ignores)
	{
		const FklGrammerIgnore* igns=g->ignores;
		ignore_print_c_match_cond(igns,fp);
		igns=igns->next;
		for(;igns;igns=igns->next)
		{
			fputs("\n\t\t\t\t\t||",fp);
			ignore_print_c_match_cond(igns,fp);
		}
	}
	if(g->ignores&&g->sortedTerminalsNum)
		fputs("\n\t\t\t\t\t||",fp);
	if(g->sortedTerminalsNum)
	{
		size_t num=g->sortedTerminalsNum;
		const FklString** terminals=g->sortedTerminals;
		const FklString* cur=terminals[0];
		fputs("(matchLen=fklCharBufMatch(\"",fp);
		print_string_in_hex(cur,fp);
		fprintf(fp,"\",%lu,*in+otherMatchLen,*restLen-otherMatchLen))",cur->size);
		for(size_t i=1;i<num;i++)
		{
			fputs("\n\t\t\t\t\t||",fp);
			const FklString* cur=terminals[i];
			fputs("(matchLen=fklCharBufMatch(\"",fp);
			print_string_in_hex(cur,fp);
			fprintf(fp,"\",%lu,*in+otherMatchLen,*restLen-otherMatchLen))",cur->size);
		}
	}
	fputs(")\n",fp);
	fputs("\t\t\t\tbreak;\n"
			"\t\t\tlen++;\n"
			"\t\t\tcur++;\n"
			"\t\t}\n"
			"\t\touterCtx->maxNonterminalLen=len;\n"
			"\t\treturn len;\n"
			"\t}\n\treturn 0;\n}\n"
			,fp);
}

static inline void print_state_action_match_to_c_file(const FklAnalysisStateAction* ac,FILE* fp)
{
	switch(ac->match.t)
	{
		case FKL_LALR_MATCH_STRING:
			fputs("(matchLen=fklCharBufMatch(\"",fp);
			print_string_in_hex(ac->match.str,fp);
			fprintf(fp,"\",%lu,*in+otherMatchLen,*restLen-otherMatchLen))",ac->match.str->size);
			break;
		case FKL_LALR_MATCH_BUILTIN:
			ac->match.func.t->print_c_match_cond(ac->match.func.c,fp);
			break;
		case FKL_LALR_MATCH_EOF:
			fputs("(matchLen=1)",fp);
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

static inline void print_state_action_to_c_file(const FklAnalysisStateAction* ac,const FklAnalysisState* states,FILE* fp)
{
	fputs("\t\t{\n",fp);
	switch(ac->action)
	{
		case FKL_ANALYSIS_SHIFT:
			fprintf(fp,"\t\t\tfklPushPtrStack((void*)state_%lu,stateStack);\n",ac->state-states);
			fputs("\t\t\tfklPushPtrStack(create_term_analyzing_symbol(*in,matchLen,outerCtx->line),symbolStack);\n"
					"\t\t\t*in+=matchLen;\n"
					"\t\t\t*restLen-=matchLen;\n"
					,fp);
			break;
		case FKL_ANALYSIS_ACCEPT:
			fputs("\t\t\t*accept=1;\n",fp);
			break;
		case FKL_ANALYSIS_REDUCE:
			fprintf(fp,"\t\t\tstateStack->top-=%lu;\n",ac->prod->len);
			fprintf(fp,"\t\t\tsymbolStack->top-=%lu;\n",ac->prod->len);
			fputs("\t\t\tFklStateFuncPtr func=(FklStateFuncPtr)fklTopPtrStack(stateStack);\n",fp);
			fputs("\t\t\tFklStateFuncPtr nextState=NULL;\n",fp);
			fprintf(fp,"\t\t\tfunc(NULL,NULL,0,%lu,(void**)&nextState,NULL,NULL,NULL,NULL,NULL,NULL);\n",ac->prod->left);
			fputs("\t\t\tif(!nextState)\n\t\t\t\treturn FKL_PARSE_REDUCE_FAILED;\n",fp);
			fputs("\t\t\tfklPushPtrStack((void*)nextState,stateStack);\n",fp);

			if(ac->prod->len)
				fprintf(fp,"\t\t\tFklNastNode** nodes=(FklNastNode**)malloc(sizeof(FklNastNode*)*%lu);\n"
						"\t\t\tFKL_ASSERT(nodes);\n"
						"\t\t\tFklAnalyzingSymbol** base=(FklAnalyzingSymbol**)&symbolStack->base[symbolStack->top];\n",ac->prod->len);
			else
				fputs("\t\t\tFklNastNode** nodes=NULL;\n",fp);

			if(ac->prod->len)
				fprintf(fp,"\t\t\tfor(size_t i=0;i<%lu;i++)\n"
						"\t\t\t{\n"
						"\t\t\t\tFklAnalyzingSymbol* as=base[i];\n"
						"\t\t\t\tnodes[i]=as->ast;\n"
						"\t\t\t\tfree(as);\n"
						"\t\t\t}\n",ac->prod->len);

			fprintf(fp,"\t\t\tFklNastNode* ast=%s(nodes,%lu,outerCtx->line,st);\n",ac->prod->name,ac->prod->len);
			if(ac->prod->len)
				fprintf(fp,"\t\t\tfor(size_t i=0;i<%lu;i++)\n"
						"\t\t\t\tfklDestroyNastNode(nodes[i]);\n"
						"\t\t\tfree(nodes);\n"
						,ac->prod->len);
			fputs("\t\t\tif(!ast)\n\t\t\t\treturn FKL_PARSE_REDUCE_FAILED;\n",fp);
			fprintf(fp,"\t\t\tfklPushPtrStack((void*)create_nonterm_analyzing_symbol(%lu,ast),symbolStack);\n",ac->prod->left);
			break;
		case FKL_ANALYSIS_IGNORE:
			fputs("\t\t\t*in+=matchLen;\n"
					"\t\t\t*restLen-=matchLen;\n"
					"\t\t\tgoto action_match_start;\n",fp);
			break;
	}
	fputs("\t\t}\n",fp);
}

static inline void print_state_prototype_to_c_file(const FklAnalysisState* states,size_t idx,FILE* fp)
{
	fprintf(fp,"static int state_%lu(FklPtrStack*"
			",FklPtrStack*"
			",int"
			",FklSid_t"
			",void** pfunc"
			",const char*"
			",const char**"
			",size_t*"
			",FklGrammerMatchOuterCtx*"
			",FklSymbolTable*"
			",int*);\n",idx);
}

static inline void print_state_to_c_file(const FklAnalysisState* states,size_t idx,FILE* fp)
{
	const FklAnalysisState* state=&states[idx];
	fprintf(fp,"static int state_%lu(FklPtrStack* stateStack\n"
			"\t\t,FklPtrStack* symbolStack\n"
			"\t\t,int is_action\n"
			"\t\t,FklSid_t left\n"
			"\t\t,void** pfunc\n"
			"\t\t,const char* start\n"
			"\t\t,const char** in\n"
			"\t\t,size_t* restLen\n"
			"\t\t,FklGrammerMatchOuterCtx* outerCtx\n"
			"\t\t,FklSymbolTable* st\n"
			"\t\t,int* accept)\n{\n",idx);
	fputs("\tif(is_action)\n\t{\n",fp);
	for(const FklAnalysisStateAction* ac=state->state.action;ac;ac=ac->next)
		if(ac->action==FKL_ANALYSIS_IGNORE)
		{
			fputs("action_match_start:\n\t\t;\n",fp);
			break;
		}
	fputs("\t\tsize_t matchLen=0;\n",fp);
	fputs("\t\tsize_t otherMatchLen=0;\n",fp);
	const FklAnalysisStateAction* ac=state->state.action;
	if(ac)
	{
		fputs("\t\tif(",fp);
		print_state_action_match_to_c_file(ac,fp);
		fputs(")\n",fp);
		print_state_action_to_c_file(ac,states,fp);
		ac=ac->next;
		for(;ac;ac=ac->next)
		{
			fputs("\t\telse if(",fp);
			print_state_action_match_to_c_file(ac,fp);
			fputs(")\n",fp);
			print_state_action_to_c_file(ac,states,fp);
		}
		fputs("\t\telse\n\t\t\treturn FKL_PARSE_TERMINAL_MATCH_FAILED;\n",fp);
	}
	else
		fputs("\t\treturn FKL_PARSE_TERMINAL_MATCH_FAILED;\n",fp);
	fputs("\t\t(void)otherMatchLen;\n",fp);
	fputs("\t}\n\telse\n\t{\n",fp);
	const FklAnalysisStateGoto* gt=state->state.gt;
	if(gt)
	{
		fprintf(fp,"\t\tif(left==%lu)\n\t\t{\n",gt->nt);
		fprintf(fp,"\t\t\t*pfunc=(void*)state_%lu;\n\t\t\treturn 0;\n\t\t}\n",gt->state-states);
		gt=gt->next;
		for(;gt;gt=gt->next)
		{
			fprintf(fp,"\t\telse if(left==%lu)\n\t\t{\n",gt->nt);
			fprintf(fp,"\t\t\t*pfunc=(void*)state_%lu;\n\t\t\treturn 0;\n\t\t}\n",gt->state-states);
		}
		fputs("\t\telse\n\t\t\treturn FKL_PARSE_REDUCE_FAILED;\n",fp);
	}
	else
		fputs("\t\treturn FKL_PARSE_REDUCE_FAILED;\n",fp);
	fputs("\t}\n",fp);
	fputs("\treturn 0;\n}\n\n",fp);
}

static inline void get_all_match_method_table(const FklGrammer* g,FklHashTable* ptrSet)
{
	for(const FklGrammerIgnore* ig=g->ignores;ig;ig=ig->next)
	{
		size_t len=ig->len;
		for(size_t i=0;i<len;i++)
			if(ig->ig[i].isbuiltin)
				fklPutHashItem(&ig->ig[i].b.t,ptrSet);
	}

	const FklAnalysisState* states=g->aTable.states;
	size_t num=g->aTable.num;
	for(size_t i=0;i<num;i++)
	{
		for(const FklAnalysisStateAction* ac=states[i].state.action;ac;ac=ac->next)
			if(ac->match.t==FKL_LALR_MATCH_BUILTIN)
				fklPutHashItem(&ac->match.func.t,ptrSet);
	}
}

void print_all_builtin_match_func(const FklGrammer* g,FILE* fp)
{
	FklHashTable builtin_match_method_table_set;
	fklInitPtrSet(&builtin_match_method_table_set);
	get_all_match_method_table(g,&builtin_match_method_table_set);
	for(FklHashTableItem* il=builtin_match_method_table_set.first;il;il=il->next)
	{
		FklLalrBuiltinMatch* t=*(FklLalrBuiltinMatch**)il->data;
		t->print_src(g,fp);
		fputc('\n',fp);
	}
	fklUninitHashTable(&builtin_match_method_table_set);
}

void fklPrintAnalysisTableAsCfunc(const FklGrammer* g
		,const FklSymbolTable* st
		,const char* actions_src[]
		,size_t actions_src_num
		,FILE *fp)
{
	static const char* create_term_analyzing_symbol_src=
		"static inline FklAnalyzingSymbol* create_term_analyzing_symbol(const char* s,size_t len,size_t line)"
		"{\n"
		"\tFklAnalyzingSymbol* sym=(FklAnalyzingSymbol*)malloc(sizeof(FklAnalyzingSymbol));\n"
		"\tFKL_ASSERT(sym);\n"
		"\tFklNastNode* ast=fklCreateNastNode(FKL_NAST_STR,line);\n"
		"\tast->str=fklCreateString(len,s);\n"
		"\tsym->nt=0;\n"
		"\tsym->ast=ast;\n"
		"\treturn sym;\n"
		"}\n\n";

	static const char* create_nonterm_analyzing_symbol_src=
		"static inline FklAnalyzingSymbol* create_nonterm_analyzing_symbol(FklSid_t id,FklNastNode* ast)\n"
		"{\n"
		"\tFklAnalyzingSymbol* sym=(FklAnalyzingSymbol*)malloc(sizeof(FklAnalyzingSymbol));\n"
		"\tFKL_ASSERT(sym);\n"
		"\tsym->nt=id;\n"
		"\tsym->ast=ast;\n"
		"\treturn sym;\n"
		"}\n\n";

	fputs("// Do not edit!\n\n",fp);
	fputs("#include<fakeLisp/base.h>\n",fp);
	fputs("#include<fakeLisp/grammer.h>\n",fp);
	fputs("#include<fakeLisp/parser.h>\n",fp);
	fputs("#include<fakeLisp/utils.h>\n",fp);
	fputs("#include<stdint.h>\n",fp);
	fputs("#include<string.h>\n",fp);
	fputs("#include<ctype.h>\n",fp);
	fputc('\n',fp);
	fputc('\n',fp);
	fputs(create_term_analyzing_symbol_src,fp);
	fputs(create_nonterm_analyzing_symbol_src,fp);

	if(g->sortedTerminals)
	{
		print_get_max_non_term_length_prototype_to_c_file(fp);
		fputc('\n',fp);
	}

	print_all_builtin_match_func(g,fp);
	size_t stateNum=g->aTable.num;
	const FklAnalysisState* states=g->aTable.states;
	fputs("typedef int (*StateFuncPtr)(FklPtrStack*"
			",FklPtrStack*"
			",int"
			",FklSid_t"
			",void**"
			",const char*"
			",const char**"
			",size_t*"
			",FklGrammerMatchOuterCtx*"
			",FklSymbolTable*"
			",int*);\n\n",fp);

	if(g->sortedTerminals)
	{
		print_get_max_non_term_length_to_c_file(g,fp);
		fputc('\n',fp);
	}
	for(size_t i=0;i<stateNum;i++)
		print_state_prototype_to_c_file(states,i,fp);
	fputc('\n',fp);
	for(size_t i=0;i<actions_src_num;i++)
		fputs(actions_src[i],fp);
	fputc('\n',fp);
	for(size_t i=0;i<stateNum;i++)
		print_state_to_c_file(states,i,fp);
	fputs("void fklPushState0ToStack(FklPtrStack* stateStack){fklPushPtrStack((void*)state_0,stateStack);}\n\n",fp);
}

void fklPrintItemStateSet(const FklHashTable* i
		,const FklGrammer* g
		,const FklSymbolTable* st
		,FILE* fp)
{
	FklHashTable idxTable;
	fklInitHashTable(&idxTable,&ItemStateIdxHashMetaTable);
	size_t idx=0;
	for(const FklHashTableItem* l=i->first;l;l=l->next,idx++)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->data;
		ItemStateIdx* c=fklPutHashItem(&s,&idxTable);
		c->idx=idx;
	}
	for(const FklHashTableItem* l=i->first;l;l=l->next)
	{
		const FklLalrItemSet* s=(const FklLalrItemSet*)l->data;
		const FklHashTable* i=&s->items;
		ItemStateIdx* c=fklGetHashItem(&s,&idxTable);
		idx=c->idx;
		fprintf(fp,"===\nI%lu: ",idx);
		putc('\n',fp);
		fklPrintItemSet(i,g,st,fp);
		putc('\n',fp);
		for(FklLalrItemSetLink* l=s->links;l;l=l->next)
		{
			FklLalrItemSet* dst=l->dst;
			ItemStateIdx* c=fklGetHashItem(&dst,&idxTable);
			fprintf(fp,"I%lu--{ ",idx);
			print_prod_sym(fp,&l->sym,st,g->terminals);
			fprintf(fp," }-->I%lu\n",c->idx);
		}
		putc('\n',fp);
	}
	fklUninitHashTable(&idxTable);
}

void fklPrintGrammerProduction(FILE* fp,const FklGrammerProduction* prod,const FklSymbolTable* st,const FklSymbolTable* tt)
{
	if(prod->left)
		fklPrintString(fklGetSymbolWithId(prod->left,st)->symbol,fp);
	else
		fputs("S'",fp);
	fputs(" ->",fp);
	size_t len=prod->len;
	FklGrammerSym* syms=prod->syms;
	for(size_t i=0;i<len;i++)
	{
		putc(' ',fp);
		print_prod_sym(fp,&syms[i],st,tt);
	}
	putc('\n',fp);
}

FklGrammerProduction* fklGetGrammerProductions(const FklGrammer* g,FklSid_t left)
{
	ProdHashItem* item=fklGetHashItem(&left,&g->productions);
	if(item)
		return item->prods;
	return NULL;
}

static inline void print_ignores(const FklGrammerIgnore* ig,FILE* fp)
{
	for(;ig;ig=ig->next)
	{
		for(size_t i=0;i<ig->len;i++)
		{
			const FklGrammerIgnoreSym* igs=&ig->ig[i];
			if(igs->isbuiltin)
			{
				fputc('%',fp);
				fputs(igs->b.t->name,fp);
			}
			else
			{
				fputc('\'',fp);
				print_string_for_grapheasy(igs->str,fp);
				fputc('\'',fp);
			}
			fputc(' ',fp);
		}
		fputc('\n',fp);
	}
}

void fklPrintGrammer(FILE* fp,const FklGrammer* grammer,FklSymbolTable* st)
{
	FklSymbolTable* tt=grammer->terminals;
	for(FklHashTableItem* list=grammer->productions.first;list;list=list->next)
	{
		FklGrammerProduction* prods=((ProdHashItem*)list->data)->prods;
		for(;prods;prods=prods->next)
		{
			fprintf(fp,"(%lu) ",prods->idx);
			fklPrintGrammerProduction(fp,prods,st,tt);
		}
	}
	fputs("\nignore:\n",fp);
	print_ignores(grammer->ignores,fp);
}

FklNastNode* fklReaderParserForCstr(const char* cstr
		,FklGrammerMatchOuterCtx* outerCtx
		,FklSymbolTable* st
		,int* err
		,FklPtrStack* symbolStack
		,FklPtrStack* stateStack)
{
	const char* start=cstr;
	size_t restLen=strlen(start);
	FklNastNode* ast=NULL;
	for(;;)
	{
		int accept=0;
		FklStateFuncPtr state=fklTopPtrStack(stateStack);
		*err=state(stateStack,symbolStack,1,0,NULL,start,&cstr,&restLen,outerCtx,st,&accept);
		if(*err)
			break;
		if(accept)
		{
			FklAnalyzingSymbol* s=fklPopPtrStack(symbolStack);
			ast=s->ast;
			break;
		}
	}
	return ast;
}

FklNastNode* fklReaderParserForCharBuf(const char* cstr
		,size_t len
		,size_t* restLen
		,FklGrammerMatchOuterCtx* outerCtx
		,FklSymbolTable* st
		,int* err
		,FklPtrStack* symbolStack
		,FklPtrStack* stateStack)
{
	const char* start=cstr;
	*restLen=len;
	FklNastNode* ast=NULL;
	for(;;)
	{
		int accept=0;
		FklStateFuncPtr state=fklTopPtrStack(stateStack);
		*err=state(stateStack,symbolStack,1,0,NULL,start,&cstr,restLen,outerCtx,st,&accept);
		if(*err)
			break;
		if(accept)
		{
			FklAnalyzingSymbol* s=fklPopPtrStack(symbolStack);
			ast=s->ast;
			break;
		}
	}
	return ast;
}

static inline int is_state_action_match(const FklAnalysisStateActionMatch* match
		,const char* start
		,const char* cstr
		,size_t restLen
		,size_t* matchLen
		,FklGrammerMatchOuterCtx* outerCtx)
{
	switch(match->t)
	{
		case FKL_LALR_MATCH_STRING:
			{
				const FklString* laString=match->str;
				if(fklStringCharBufMatch(laString,cstr,restLen))
				{
					*matchLen=laString->size;
					return 1;
				}
			}
			break;
		case FKL_LALR_MATCH_EOF:
			*matchLen=1;
			return 1;
			break;
		case FKL_LALR_MATCH_BUILTIN:
			if(match->func.t->match(match->func.c,start,cstr,restLen,matchLen,outerCtx))
				return 1;
			break;
		case FKL_LALR_MATCH_NONE:
			FKL_ASSERT(0);
			break;
	}
	return 0;
}

static inline void dbg_print_state_stack(FklPtrStack* stateStack,FklAnalysisState* states)
{
	for(size_t i=0;i<stateStack->top;i++)
	{
		const FklAnalysisState* curState=stateStack->base[i];
		fprintf(stderr,"%lu ",curState-states);
	}
	fputc('\n',stderr);
}

static inline FklAnalyzingSymbol* create_term_analyzing_symbol(const char* s,size_t len,size_t line)
{
	FklAnalyzingSymbol* sym=(FklAnalyzingSymbol*)malloc(sizeof(FklAnalyzingSymbol));
	FKL_ASSERT(sym);
	FklNastNode* ast=fklCreateNastNode(FKL_NAST_STR,line);
	ast->str=fklCreateString(len,s);
	sym->nt=0;
	sym->ast=ast;
	return sym;
}

static inline FklAnalyzingSymbol* create_nonterm_analyzing_symbol(FklSid_t id,FklNastNode* ast)
{
	FklAnalyzingSymbol* sym=(FklAnalyzingSymbol*)malloc(sizeof(FklAnalyzingSymbol));
	FKL_ASSERT(sym);
	sym->nt=id;
	sym->ast=ast;
	return sym;
}

static inline int do_reduce_action(FklPtrStack* stateStack
		,FklPtrStack* symbolStack
		,const FklGrammerProduction* prod
		,FklGrammerMatchOuterCtx* outerCtx
		,FklSymbolTable* st)
{

	size_t len=prod->len;
	stateStack->top-=len;
	FklAnalysisStateGoto* gt=((const FklAnalysisState*)fklTopPtrStack(stateStack))->state.gt;
	const FklAnalysisState* state=NULL;
	FklSid_t left=prod->left;
	for(;gt;gt=gt->next)
		if(gt->nt==left)
			state=gt->state;
	if(!state)
		return 1;
	symbolStack->top-=len;
	FklNastNode** nodes=(FklNastNode**)malloc(sizeof(FklNastNode*)*len);
	FKL_ASSERT(nodes);
	FklAnalyzingSymbol** base=(FklAnalyzingSymbol**)&symbolStack->base[symbolStack->top];
	for(size_t i=0;i<len;i++)
	{
		FklAnalyzingSymbol* as=base[i];
		nodes[i]=as->ast;
		free(as);
	}
	fklPushPtrStack(create_nonterm_analyzing_symbol(left,prod->func(nodes,len,outerCtx->line,st)),symbolStack);
	for(size_t i=0;i<len;i++)
		fklDestroyNastNode(nodes[i]);
	free(nodes);
	fklPushPtrStack((void*)state,stateStack);
	return 0;
}

FklNastNode* fklParseWithTableForCstrDbg(const FklAnalysisTable* t
		,const char* cstr
		,FklGrammerMatchOuterCtx* outerCtx
		,FklSymbolTable* st
		,int* err)
{
#warning incomplete
	FklNastNode* ast=NULL;
	const char* start=cstr;
	size_t restLen=strlen(start);
	FklPtrStack symbolStack;
	FklPtrStack stateStack;
	fklInitPtrStack(&stateStack,16,8);
	fklInitPtrStack(&symbolStack,16,8);
	fklPushPtrStack(&t->states[0],&stateStack);
	size_t matchLen=0;
	for(;;)
	{
		const FklAnalysisState* state=fklTopPtrStack(&stateStack);
		const FklAnalysisStateAction* action=state->state.action;
		dbg_print_state_stack(&stateStack,t->states);
		for(;action;action=action->next)
			if(is_state_action_match(&action->match,start,cstr,restLen,&matchLen,outerCtx))
				break;
		if(action)
		{
			FKL_ASSERT(action->match.t!=FKL_LALR_MATCH_EOF||action->next==NULL);
			switch(action->action)
			{
				case FKL_ANALYSIS_IGNORE:
					cstr+=matchLen;
					restLen-=matchLen;
					continue;
					break;
				case FKL_ANALYSIS_SHIFT:
					{
						fklPushPtrStack((void*)action->state,&stateStack);
						fklPushPtrStack(create_term_analyzing_symbol(cstr,matchLen,outerCtx->line),&symbolStack);
						cstr+=matchLen;
						restLen-=matchLen;
					}
					break;
				case FKL_ANALYSIS_ACCEPT:
					{
						FklAnalyzingSymbol* top=fklTopPtrStack(&symbolStack);
						ast=top->ast;
					}
					goto break_for;
					break;
				case FKL_ANALYSIS_REDUCE:
					if(do_reduce_action(&stateStack
								,&symbolStack
								,action->prod
								,outerCtx
								,st))
					{
						*err=2;
						goto break_for;
					}
					break;
			}
		}
		else
		{
			*err=1;
			break;
		}
	}
break_for:
	fklUninitPtrStack(&stateStack);
	while(!fklIsPtrStackEmpty(&symbolStack))
		free(fklPopPtrStack(&symbolStack));
	fklUninitPtrStack(&symbolStack);
	return ast;
}

FklNastNode* fklParseWithTableForCstr(const FklAnalysisTable* t
		,const char* cstr
		,FklGrammerMatchOuterCtx* outerCtx
		,FklSymbolTable* st
		,int* err)
{
#warning incomplete
	FklNastNode* ast=NULL;
	const char* start=cstr;
	size_t restLen=strlen(start);
	FklPtrStack symbolStack;
	FklPtrStack stateStack;
	fklInitPtrStack(&stateStack,16,8);
	fklInitPtrStack(&symbolStack,16,8);
	fklPushPtrStack(&t->states[0],&stateStack);
	size_t matchLen=0;
	for(;;)
	{
		const FklAnalysisState* state=fklTopPtrStack(&stateStack);
		const FklAnalysisStateAction* action=state->state.action;
		for(;action;action=action->next)
			if(is_state_action_match(&action->match,start,cstr,restLen,&matchLen,outerCtx))
				break;
		if(action)
		{
			switch(action->action)
			{
				case FKL_ANALYSIS_IGNORE:
					cstr+=matchLen;
					restLen-=matchLen;
					continue;
					break;
				case FKL_ANALYSIS_SHIFT:
					{
						fklPushPtrStack((void*)action->state,&stateStack);
						fklPushPtrStack(create_term_analyzing_symbol(cstr,matchLen,outerCtx->line),&symbolStack);
						cstr+=matchLen;
						restLen-=matchLen;
					}
					break;
				case FKL_ANALYSIS_ACCEPT:
					{
						FklAnalyzingSymbol* top=fklTopPtrStack(&symbolStack);
						ast=top->ast;
					}
					goto break_for;
					break;
				case FKL_ANALYSIS_REDUCE:
					if(do_reduce_action(&stateStack
								,&symbolStack
								,action->prod
								,outerCtx
								,st))
					{
						*err=2;
						goto break_for;
					}
					break;
			}
		}
		else
		{
			*err=1;
			break;
		}
	}
break_for:
	fklUninitPtrStack(&stateStack);
	while(!fklIsPtrStackEmpty(&symbolStack))
		free(fklPopPtrStack(&symbolStack));
	fklUninitPtrStack(&symbolStack);
	return ast;
}

