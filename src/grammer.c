#include<fakeLisp/lexer.h>
#include<fakeLisp/base.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>

extern FklNastNode* prod_action_symbol(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st);

static FklNastNode* prod_action_return_first(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklMakeNastNodeRef(nodes[0]);
}

static FklNastNode* prod_action_return_second(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklMakeNastNodeRef(nodes[1]);
}

static FklNastNode* prod_action_string(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* s=nodes[1]->u.str;
	size_t size=0;
	char* cstr=fklCastEscapeCharBuf(s->str,s->size,&size);
	FklString* str=fklCreateString(size,cstr);
	FklNastNode* node=fklCreateNastNode(FKL_NAST_STR,nodes[1]->curline);
	node->u.str=str;
	free(cstr);
	return node;
}

static FklNastNode* prod_action_nil(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklCreateNastNode(FKL_NAST_NIL,line);
}

static FklNastNode* prod_action_pair(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* car=nodes[0];
	FklNastNode* cdr=nodes[2];
	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
	pair->u.pair=fklCreateNastPair();
	pair->u.pair->car=fklMakeNastNodeRef(car);
	pair->u.pair->cdr=fklMakeNastNodeRef(cdr);
	return pair;
}

static FklNastNode* prod_action_list(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* car=nodes[0];
	FklNastNode* cdr=nodes[1];
	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
	pair->u.pair=fklCreateNastPair();
	pair->u.pair->car=fklMakeNastNodeRef(car);
	pair->u.pair->cdr=fklMakeNastNodeRef(cdr);
	return pair;
}

static FklNastNode* prod_action_dec_integer(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=nodes[0]->u.str;
	int64_t i=strtoll(str->str,NULL,10);
	FklNastNode* r=fklCreateNastNode(FKL_NAST_FIX,line);
	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
	{
		FklBigInt bInt=FKL_BIG_INT_INIT;
		fklInitBigIntFromDecString(&bInt,str);
		FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));
		FKL_ASSERT(bi);
		*bi=bInt;
		r->u.bigInt=bi;
		r->type=FKL_NAST_BIG_INT;
	}
	else
	{
		r->type=FKL_NAST_FIX;
		r->u.fix=i;
	}
	return r;
}

static FklNastNode* prod_action_hex_integer(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=nodes[0]->u.str;
	int64_t i=strtoll(str->str,NULL,16);
	FklNastNode* r=fklCreateNastNode(FKL_NAST_FIX,line);
	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
	{
		FklBigInt bInt=FKL_BIG_INT_INIT;
		fklInitBigIntFromHexString(&bInt,str);
		FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));
		FKL_ASSERT(bi);
		*bi=bInt;
		r->u.bigInt=bi;
		r->type=FKL_NAST_BIG_INT;
	}
	else
	{
		r->type=FKL_NAST_FIX;
		r->u.fix=i;
	}
	return r;
}

static FklNastNode* prod_action_oct_integer(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=nodes[0]->u.str;
	int64_t i=strtoll(str->str,NULL,8);
	FklNastNode* r=fklCreateNastNode(FKL_NAST_FIX,line);
	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
	{
		FklBigInt bInt=FKL_BIG_INT_INIT;
		fklInitBigIntFromOctString(&bInt,str);
		FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));
		FKL_ASSERT(bi);
		*bi=bInt;
		r->u.bigInt=bi;
		r->type=FKL_NAST_BIG_INT;
	}
	else
	{
		r->type=FKL_NAST_FIX;
		r->u.fix=i;
	}
	return r;
}

static FklNastNode* prod_action_float(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=nodes[0]->u.str;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_F64,line);
	double i=strtod(str->str,NULL);
	r->u.f64=i;
	return r;
}

static FklNastNode* prod_action_backslash(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	c->u.chr='\\';
	return c;
}

static FklNastNode* prod_action_any_char(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	c->u.chr=nodes[1]->u.str->str[0];
	return c;
}

static FklNastNode* prod_action_dec_char(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	c->u.chr=strtol(nodes[1]->u.str->str,NULL,10);
	return c;
}

static FklNastNode* prod_action_hex_char(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	c->u.chr=strtol(nodes[1]->u.str->str,NULL,16);
	return c;
}

static FklNastNode* prod_action_oct_char(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	const FklString* s=nodes[1]->u.str;
	if(s->size)
		c->u.chr=strtol(s->str,NULL,8);
	else
		c->u.chr=0;
	return c;
}

static FklNastNode* prod_action_box(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);
	box->u.box=fklMakeNastNodeRef(nodes[1]);
	return box;
}

static FklNastNode* prod_action_vector(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_VECTOR,line);
	size_t len=fklNastListLength(list);
	FklNastVector* vec=fklCreateNastVector(len);
	r->u.vec=vec;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr,i++)
		vec->base[i]=fklMakeNastNodeRef(list->u.pair->car);
	return r;
}

static inline FklNastNode* create_nast_list(FklNastNode** a,size_t num,uint64_t line)
{
	FklNastNode* r=NULL;
	FklNastNode** cur=&r;
	for(size_t i=0;i<num;i++)
	{
		(*cur)=fklCreateNastNode(FKL_NAST_PAIR,a[i]->curline);
		(*cur)->u.pair=fklCreateNastPair();
		(*cur)->u.pair->car=a[i];
		cur=&(*cur)->u.pair->cdr;
	}
	(*cur)=fklCreateNastNode(FKL_NAST_NIL,line);
	return r;
}

static FklNastNode* prod_action_quote(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklSid_t id=fklAddSymbolCstr("quote",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->u.sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static FklNastNode* prod_action_unquote(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklSid_t id=fklAddSymbolCstr("unquote",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->u.sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static FklNastNode* prod_action_qsquote(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklSid_t id=fklAddSymbolCstr("qsquote",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->u.sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static FklNastNode* prod_action_unqtesp(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklSid_t id=fklAddSymbolCstr("unqtesp",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->u.sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static FklNastNode* prod_action_kv_list(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* car=nodes[1];
	FklNastNode* cdr=nodes[3];
	FklNastNode* rest=nodes[5];

	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
	pair->u.pair=fklCreateNastPair();
	pair->u.pair->car=fklMakeNastNodeRef(car);
	pair->u.pair->cdr=fklMakeNastNodeRef(cdr);

	FklNastNode* r=fklCreateNastNode(FKL_NAST_PAIR,line);
	r->u.pair=fklCreateNastPair();
	r->u.pair->car=pair;
	r->u.pair->cdr=fklMakeNastNodeRef(rest);
	return r;
}

static FklNastNode* prod_action_hasheq(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQ,len);
	r->u.hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->u.pair->car->u.pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->u.pair->car->u.pair->cdr);
	}
	return r;
}

static FklNastNode* prod_action_hasheqv(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQV,len);
	r->u.hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->u.pair->car->u.pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->u.pair->car->u.pair->cdr);
	}
	return r;
}

static FklNastNode* prod_action_hashequal(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQUAL,len);
	r->u.hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->u.pair->car->u.pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->u.pair->car->u.pair->cdr);
	}
	return r;
}

static FklNastNode* prod_action_bytevector(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_BYTEVECTOR,line);
	size_t len=fklNastListLength(list);
	FklBytevector* bv=fklCreateBytevector(len,NULL);
	r->u.bvec=bv;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr,i++)
	{
		FklNastNode* cur=list->u.pair->car;
		if(cur->type==FKL_NAST_FIX)
			bv->ptr[i]=cur->u.fix>UINT8_MAX?UINT8_MAX:(cur->u.fix<0?0:cur->u.fix);
		else
			bv->ptr[i]=cur->u.bigInt->neg?0:UINT8_MAX;
	}
	return r;
}
static const FklGrammerCstrAction example_grammer_action[]=
{
	{"s-exp &list",                   prod_action_return_first,  },
	{"s-exp &vector",                 prod_action_return_first,  },
	{"s-exp &bytevector",             prod_action_return_first,  },
	{"s-exp &box",                    prod_action_return_first,  },
	{"s-exp &hasheq",                 prod_action_return_first,  },
	{"s-exp &hasheqv",                prod_action_return_first,  },
	{"s-exp &hashequal",              prod_action_return_first,  },
	{"s-exp &string",                 prod_action_return_first,  },
	{"s-exp &symbol",                 prod_action_return_first,  },
	{"s-exp &integer",                prod_action_return_first,  },
	{"s-exp &float",                  prod_action_return_first,  },
	{"s-exp &char",                   prod_action_return_first,  },
	{"s-exp &quote",                  prod_action_return_first,  },
	{"s-exp &qsquote",                prod_action_return_first,  },
	{"s-exp &unquote",                prod_action_return_first,  },
	{"s-exp &unqtesp",                prod_action_return_first,  },

	{"quote #' &s-exp",               prod_action_quote,         },
	{"qsquote #` &s-exp",             prod_action_qsquote,       },
	{"unquote #~ &s-exp",             prod_action_unquote,       },
	{"unqtesp #~@ &s-exp",            prod_action_unqtesp,       },

	{"symbol &%symbol + #|",          prod_action_symbol,        },
	{"string #\" + &%qstr #\"",       prod_action_string,        },

	{"integer &%s-dint + #|",         prod_action_dec_integer,   },
	{"integer &%s-xint + #|",         prod_action_hex_integer,   },
	{"integer &%s-oint + #|",         prod_action_oct_integer,   },

	{"float &%s-dfloat + #|",         prod_action_float,         },
	{"float &%s-xfloat + #|",         prod_action_float,         },

	{"char ##\\\\",                   prod_action_backslash,     },
	{"char ##\\ + &%any",             prod_action_any_char,      },
	{"char ##\\\\ + &%dec3",          prod_action_dec_char,      },
	{"char ##\\\\0 + &%oct3",         prod_action_oct_char,      },
	{"char ##\\\\x + &%hex2",         prod_action_hex_char,      },

	{"box ##& &s-exp",                prod_action_box,           },

	{"list #( &l #)",                 prod_action_return_second, },
	{"list #[ &l #]",                 prod_action_return_second, },

	{"l ",                            prod_action_nil,           },
	{"l &s-exp &l",                   prod_action_list,          },
	{"l &s-exp #, &s-exp",            prod_action_pair,          },

	{"vector ##( &v #)",              prod_action_vector,        },
	{"vector ##[ &v #]",              prod_action_vector,        },

	{"v ",                            prod_action_nil,           },
	{"v &s-exp &v",                   prod_action_list,          },

	{"hasheq ##hash( &kv #)",         prod_action_hasheq,        },
	{"hasheq ##hash[ &kv #]",         prod_action_hasheq,        },

	{"hasheqv ##hasheqv( &kv #)",     prod_action_hasheqv,       },
	{"hasheqv ##hasheqv[ &kv #]",     prod_action_hasheqv,       },

	{"hashequal ##hashequal( &kv #)", prod_action_hashequal,     },
	{"hashequal ##hashequal[ &kv #]", prod_action_hashequal,     },

	{"kv ",                           prod_action_nil,           },
	{"kv #( &s-exp #, &s-exp #) &kv", prod_action_kv_list,       },
	{"kv #[ &s-exp #, &s-exp #] &kv", prod_action_kv_list,       },

	{"bytevector ##vu8( &v8 #)",      prod_action_bytevector,    },
	{"bytevector ##vu8[ &v8 #]",      prod_action_bytevector,    },

	{"v8 ",                           prod_action_nil,           },
	{"v8 &integer &v8",               prod_action_list,          },

	{"+ &%space",                     NULL,                      },
	{"+ #; &%eol",                    NULL,                      },
	{"+ ##! &%eol",                   NULL,                      },
	{NULL,                            NULL,                      },
};

