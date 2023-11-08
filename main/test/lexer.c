#include<fakeLisp/grammer.h>
#include<fakeLisp/base.h>
#include<fakeLisp/nast.h>
#include<fakeLisp/utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>

static void* fklNastTerminalCreate(const char* s,size_t len,size_t line,void* ctx)
{
	FklNastNode* ast=fklCreateNastNode(FKL_NAST_STR,line);
	ast->str=fklCreateString(len,s);
	return ast;
}

static inline void* prod_action_symbol(void* ctx
		,void* outerCtx
		,void* ast[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode** nodes=(FklNastNode**)ast;
	const char* start="|";
	size_t start_size=1;
	const char* end="|";
	size_t end_size=1;

	const FklString* str=nodes[0]->str;
	const char* cstr=str->str;
	size_t cstr_size=str->size;

	FklStringBuffer buffer;
	fklInitStringBuffer(&buffer);
	const char* end_cstr=cstr+str->size;
	while(cstr<end_cstr)
	{
		if(fklCharBufMatch(start,start_size,cstr,cstr_size))
		{
			cstr+=start_size;
			cstr_size-=start_size;
			size_t len=fklQuotedCharBufMatch(cstr,cstr_size,end,end_size);
			if(!len)
				return 0;
			size_t size=0;
			char* s=fklCastEscapeCharBuf(cstr,len-end_size,&size);
			fklStringBufferBincpy(&buffer,s,size);
			free(s);
			cstr+=len;
			cstr_size-=len;
			continue;
		}
		size_t len=0;
		for(;(cstr+len)<end_cstr;len++)
			if(fklCharBufMatch(start,start_size,cstr+len,cstr_size-len))
				break;
		fklStringBufferBincpy(&buffer,cstr,len);
		cstr+=len;
		cstr_size-=len;
	}
	FklSid_t id=fklAddSymbolCharBuf(buffer.buf,buffer.index,st)->id;
	FklNastNode* node=fklCreateNastNode(FKL_NAST_SYM,nodes[0]->curline);
	node->sym=id;
	fklUninitStringBuffer(&buffer);
	return node;
}

static inline void* prod_action_return_first(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklMakeNastNodeRef(nodes[0]);
}

static inline void* prod_action_return_second(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklMakeNastNodeRef(nodes[1]);
}

static inline void* prod_action_string(void* ctx
		,void* outerCtx
		,void* ast[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode** nodes=(FklNastNode**)ast;
	size_t start_size=1;
	size_t end_size=1;

	const FklString* str=nodes[0]->str;
	const char* cstr=str->str;

	size_t size=0;
	char* s=fklCastEscapeCharBuf(&cstr[start_size],str->size-end_size-start_size,&size);
	FklNastNode* node=fklCreateNastNode(FKL_NAST_STR,nodes[0]->curline);
	node->str=fklCreateString(size,s);
	free(s);
	return node;
}

static inline void* prod_action_nil(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklCreateNastNode(FKL_NAST_NIL,line);
}

static inline void* prod_action_pair(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* car=nodes[0];
	FklNastNode* cdr=nodes[2];
	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
	pair->pair=fklCreateNastPair();
	pair->pair->car=fklMakeNastNodeRef(car);
	pair->pair->cdr=fklMakeNastNodeRef(cdr);
	return pair;
}

static inline void* prod_action_list(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* car=nodes[0];
	FklNastNode* cdr=nodes[1];
	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
	pair->pair=fklCreateNastPair();
	pair->pair->car=fklMakeNastNodeRef(car);
	pair->pair->cdr=fklMakeNastNodeRef(cdr);
	return pair;
}

static inline void* prod_action_dec_integer(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=((FklNastNode*)nodes[0])->str;
	int64_t i=strtoll(str->str,NULL,10);
	FklNastNode* r=fklCreateNastNode(FKL_NAST_FIX,line);
	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
	{
		FklBigInt bInt=FKL_BIG_INT_INIT;
		fklInitBigIntFromDecString(&bInt,str);
		FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));
		FKL_ASSERT(bi);
		*bi=bInt;
		r->bigInt=bi;
		r->type=FKL_NAST_BIG_INT;
	}
	else
	{
		r->type=FKL_NAST_FIX;
		r->fix=i;
	}
	return r;
}

static inline void* prod_action_hex_integer(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=((FklNastNode*)nodes[0])->str;
	int64_t i=strtoll(str->str,NULL,16);
	FklNastNode* r=fklCreateNastNode(FKL_NAST_FIX,line);
	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
	{
		FklBigInt bInt=FKL_BIG_INT_INIT;
		fklInitBigIntFromHexString(&bInt,str);
		FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));
		FKL_ASSERT(bi);
		*bi=bInt;
		r->bigInt=bi;
		r->type=FKL_NAST_BIG_INT;
	}
	else
	{
		r->type=FKL_NAST_FIX;
		r->fix=i;
	}
	return r;
}

static inline void* prod_action_oct_integer(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=((FklNastNode*)nodes[0])->str;
	int64_t i=strtoll(str->str,NULL,8);
	FklNastNode* r=fklCreateNastNode(FKL_NAST_FIX,line);
	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
	{
		FklBigInt bInt=FKL_BIG_INT_INIT;
		fklInitBigIntFromOctString(&bInt,str);
		FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));
		FKL_ASSERT(bi);
		*bi=bInt;
		r->bigInt=bi;
		r->type=FKL_NAST_BIG_INT;
	}
	else
	{
		r->type=FKL_NAST_FIX;
		r->fix=i;
	}
	return r;
}

static inline void* prod_action_float(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=((FklNastNode*)nodes[0])->str;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_F64,line);
	double i=strtod(str->str,NULL);
	r->f64=i;
	return r;
}

static inline void* prod_action_char(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=((FklNastNode*)nodes[0])->str;
	if(!fklIsValidCharBuf(str->str+2,str->size-2))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_CHR,line);
	r->chr=fklCharBufToChar(str->str+2,str->size-2);
	return r;
}

static inline void* prod_action_box(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);
	box->box=fklMakeNastNodeRef(nodes[1]);
	return box;
}

static inline void* prod_action_vector(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_VECTOR,line);
	size_t len=fklNastListLength(list);
	FklNastVector* vec=fklCreateNastVector(len);
	r->vec=vec;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
		vec->base[i]=fklMakeNastNodeRef(list->pair->car);
	return r;
}

static inline FklNastNode* create_nast_list(FklNastNode* a[],size_t num,uint64_t line)
{
	FklNastNode* r=NULL;
	FklNastNode** cur=&r;
	for(size_t i=0;i<num;i++)
	{
		(*cur)=fklCreateNastNode(FKL_NAST_PAIR,a[i]->curline);
		(*cur)->pair=fklCreateNastPair();
		(*cur)->pair->car=a[i];
		cur=&(*cur)->pair->cdr;
	}
	(*cur)=fklCreateNastNode(FKL_NAST_NIL,line);
	return r;
}

static inline void* prod_action_quote(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklSid_t id=fklAddSymbolCstr("quote",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* prod_action_unquote(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklSid_t id=fklAddSymbolCstr("unquote",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* prod_action_qsquote(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklSid_t id=fklAddSymbolCstr("qsquote",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* prod_action_unqtesp(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklSid_t id=fklAddSymbolCstr("unqtesp",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* prod_action_kv_list(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* car=nodes[1];
	FklNastNode* cdr=nodes[3];
	FklNastNode* rest=nodes[5];

	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
	pair->pair=fklCreateNastPair();
	pair->pair->car=fklMakeNastNodeRef(car);
	pair->pair->cdr=fklMakeNastNodeRef(cdr);

	FklNastNode* r=fklCreateNastNode(FKL_NAST_PAIR,line);
	r->pair=fklCreateNastPair();
	r->pair->car=pair;
	r->pair->cdr=fklMakeNastNodeRef(rest);
	return r;
}

static inline void* prod_action_hasheq(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQ,len);
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
	}
	return r;
}

static inline void* prod_action_hasheqv(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQV,len);
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
	}
	return r;
}

static inline void* prod_action_hashequal(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQUAL,len);
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
	}
	return r;
}

static inline void* prod_action_bytevector(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_BYTEVECTOR,line);
	size_t len=fklNastListLength(list);
	FklBytevector* bv=fklCreateBytevector(len,NULL);
	r->bvec=bv;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		FklNastNode* cur=list->pair->car;
		if(cur->type==FKL_NAST_FIX)
			bv->ptr[i]=cur->fix>UINT8_MAX?UINT8_MAX:(cur->fix<0?0:cur->fix);
		else
			bv->ptr[i]=cur->bigInt->neg?0:UINT8_MAX;
	}
	return r;
}

static const FklGrammerCstrAction example_grammer_action[]=
{
	{"s-exp &list",                   "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &vector",                 "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &bytevector",             "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &box",                    "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &hasheq",                 "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &hasheqv",                "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &hashequal",              "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &string",                 "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &symbol",                 "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &integer",                "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &float",                  "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &char",                   "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &quote",                  "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &qsquote",                "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &unquote",                "prod_action_return_first",  prod_action_return_first,  },
	{"s-exp &unqtesp",                "prod_action_return_first",  prod_action_return_first,  },

	{"quote #' &s-exp",               "prod_action_quote",         prod_action_quote,         },
	{"qsquote #` &s-exp",             "prod_action_qsquote",       prod_action_qsquote,       },
	{"unquote #~ &s-exp",             "prod_action_unquote",       prod_action_unquote,       },
	{"unqtesp #~@ &s-exp",            "prod_action_unqtesp",       prod_action_unqtesp,       },

	{"symbol &?symbol + #|",          "prod_action_symbol",        prod_action_symbol,        },
	{"string /^\"(\\\\.|.)*\"$",      "prod_action_string",        prod_action_string,        },

	{"integer &?s-dint + #|",         "prod_action_dec_integer",   prod_action_dec_integer,   },
	{"integer &?s-xint + #|",         "prod_action_hex_integer",   prod_action_hex_integer,   },
	{"integer &?s-oint + #|",         "prod_action_oct_integer",   prod_action_oct_integer,   },

	{"float &?s-dfloat + #|",         "prod_action_float",         prod_action_float,         },
	{"float &?s-xfloat + #|",         "prod_action_float",         prod_action_float,         },

	{"char &?char + ##\\",            "prod_action_char",          prod_action_char,          },

	{"box ##& &s-exp",                "prod_action_box",           prod_action_box,           },

	{"list #( &l #)",                 "prod_action_return_second", prod_action_return_second, },
	{"list #[ &l #]",                 "prod_action_return_second", prod_action_return_second, },

	{"l ",                            "prod_action_nil",           prod_action_nil,           },
	{"l &s-exp &l",                   "prod_action_list",          prod_action_list,          },
	{"l &s-exp #, &s-exp",            "prod_action_pair",          prod_action_pair,          },

	{"vector ##( &v #)",              "prod_action_vector",        prod_action_vector,        },
	{"vector ##[ &v #]",              "prod_action_vector",        prod_action_vector,        },

	{"v ",                            "prod_action_nil",           prod_action_nil,           },
	{"v &s-exp &v",                   "prod_action_list",          prod_action_list,          },

	{"hasheq ##hash( &kv #)",         "prod_action_hasheq",        prod_action_hasheq,        },
	{"hasheq ##hash[ &kv #]",         "prod_action_hasheq",        prod_action_hasheq,        },

	{"hasheqv ##hasheqv( &kv #)",     "prod_action_hasheqv",       prod_action_hasheqv,       },
	{"hasheqv ##hasheqv[ &kv #]",     "prod_action_hasheqv",       prod_action_hasheqv,       },

	{"hashequal ##hashequal( &kv #)", "prod_action_hashequal",     prod_action_hashequal,     },
	{"hashequal ##hashequal[ &kv #]", "prod_action_hashequal",     prod_action_hashequal,     },

	{"kv ",                           "prod_action_nil",           prod_action_nil,           },
	{"kv #( &s-exp #, &s-exp #) &kv", "prod_action_kv_list",       prod_action_kv_list,       },
	{"kv #[ &s-exp #, &s-exp #] &kv", "prod_action_kv_list",       prod_action_kv_list,       },

	{"bytevector ##vu8( &v8 #)",      "prod_action_bytevector",    prod_action_bytevector,    },
	{"bytevector ##vu8[ &v8 #]",      "prod_action_bytevector",    prod_action_bytevector,    },

	{"v8 ",                           "prod_action_nil",           prod_action_nil,           },
	{"v8 &integer &v8",               "prod_action_list",          prod_action_list,          },

	{"+ /\\s+",                       NULL,                        NULL,                      },
	{"+ /^;[^\\n]*\\n?",              NULL,                        NULL,                      },
	{"+ /^#![^\\n]*\\n?",             NULL,                        NULL,                      },
	{NULL,                            NULL,                        NULL,                      },
};

int main()
{
	FklSymbolTable* st=fklCreateSymbolTable();
	// FklGrammer* g=fklCreateGrammerFromCstrAction(example_grammer,st);
	FklGrammer* g=fklCreateGrammerFromCstrAction(example_grammer_action,st);
	if(!g)
	{
		fklDestroySymbolTable(st);
		fprintf(stderr,"garmmer create fail\n");
		exit(1);
	}
	if(g->sortedTerminals)
	{
		fputs("\nterminals:\n",stdout);
		for(size_t i=0;i<g->sortedTerminalsNum;i++)
			fprintf(stdout,"%s\n",g->sortedTerminals[i]->str);
		fputc('\n',stdout);
	}
	fputs("grammer:\n",stdout);
	fklPrintGrammer(stdout, g, st);
	FklHashTable* itemSet=fklGenerateLr0Items(g);
	fputc('\n',stdout);
	fputs("item sets:\n",stdout);
	FILE* gzf=fopen("items.gz.txt","w");
	FILE* lalrgzf=fopen("items-lalr.gz.txt","w");
	fklPrintItemStateSetAsDot(itemSet,g,st,gzf);
	fklLr0ToLalrItems(itemSet,g);
	fklPrintItemStateSet(itemSet,g,st,stdout);
	fklPrintItemStateSetAsDot(itemSet,g,st,lalrgzf);

	if(fklGenerateLalrAnalyzeTable(g,itemSet))
	{
		fklDestroySymbolTable(st);
		fprintf(stderr,"not lalr garmmer\n");
		exit(1);
	}
	fklPrintAnalysisTable(g,st,stdout);
	fklDestroyHashTable(itemSet);

	FILE* tablef=fopen("table.txt","w");
	fklPrintAnalysisTableForGraphEasy(g,st,tablef);

	fclose(tablef);
	fclose(gzf);
	fclose(lalrgzf);

	fputc('\n',stdout);
	
	const char* exps[]=
	{
		"#\\\\11",
		"#\\\\z",
		"#\\\\n",
		"#\\\\",
		"#\\;",
		"#\\|",
		"#\\\"",
		"#\\(",
		"#\\\\s",
		"(abcd)abcd",
		";comments\nabcd",
		"foobar|foo|foobar|bar|",
		"(\"foo\" \"bar\" \"foobar\",;abcd\n\"i\")",
		"[(foobar;comments\nfoo bar),abcd]",
		"(foo bar abcd|foo \\|bar|efgh foo \"foo\\\"\",bar)",
		"#hash((a,1) (b,2))",
		"#hashequal((a,1) (b,2))",
		"#vu8(114 514 114514)",
		"114514",
		"#\\ ",
		"'#&#(foo 0x114514 \"foobar\" .1 0x1p1 114514|foo|bar #\\a #\\\\0 #\\\\x11 #\\\\0123 #\\\\0177 #\\\\0777)",
		"\"foobar\"",
		"114514",
		NULL,
	};

	int retval=0;
	FklGrammerMatchOuterCtx outerCtx={.maxNonterminalLen=0,.line=1,.start=NULL,.cur=NULL,.create=fklNastTerminalCreate,.destroy=(void(*)(void*))fklDestroyNastNode,};

	for(const char** exp=&exps[0];*exp;exp++)
	{
		FklNastNode* ast=fklParseWithTableForCstr(g,*exp,&outerCtx,st,&retval);

		if(retval)
			break;

		fklPrintNastNode(ast,stdout,st);
		fklDestroyNastNode(ast);
		fputc('\n',stdout);

	}
	fklDestroyGrammer(g);
	fklDestroySymbolTable(st);
	return retval;
}
