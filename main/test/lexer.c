#include<fakeLisp/lexer.h>
#include<fakeLisp/base.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>

// const char* test_grammer[]={
// 	"s #( &s #)",
// 	"s #( #)",
// 	"s &d",
// 	"d #0",
// 	"d #1",
// 	"d #2",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"string #“ + &%qstr + #”",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"ss #( &b #)",
// 	"ss &s",
// 	"b &ss &b",
// 	"b ",
//
// 	"s #d",
// 	"s #c",
// 	"s #d + &s",
// 	"s #c + &s",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"s #foobar",
// 	"s #| + &%qstr + #|",
// 	"s &s + #foobar",
// 	"s &s + #| + &%qstr + #|",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"s #( &l #)",
// 	"s #[ &l #]",
//
// 	// "s ##( &v #)",
// 	// "s ##[ &v #]",
// 	//
// 	// "s ##hash( &kv #)",
// 	// "s ##hash[ &kv #]",
// 	//
// 	// "s ##hasheqv( &kv #)",
// 	// "s ##hasheqv[ &kv #]",
// 	//
// 	// "s ##hashequal( &kv #)",
// 	// "s ##hashequal[ &kv #]",
// 	//
// 	// "s ##vu8( &v8 #)",
// 	// "s ##vu8[ &v8 #]",
// 	//
// 	// "s #' &s",
// 	// "s #` &s",
// 	// "s #~ &s",
// 	// "s #~@ &s",
// 	// "s ##& &s",
// 	//
// 	// "s &%dec-int",
// 	// "s &%hex-int",
// 	// "s &%oct-int",
// 	//
// 	// "s &%dec-float",
// 	// "s &%hex-float",
//
// 	"s &%identifier + #|",
// 	// "s #\" + &%qstr #\"",
//
// 	"l ",
// 	"l &s &l",
// 	"l &s #, &s",
// 	//
// 	// "v ",
// 	// "v &s &v",
// 	//
// 	// "v8 ",
// 	// "v8 &%dec-int &v8",
// 	// "v8 &%hex-int &v8",
// 	// "v8 &%oct-int &v8",
// 	//
// 	// "kv ",
// 	// "kv #( &s #, &s #) &kv",
// 	// "kv #[ &s #, &s #] &kv",
//
// 	"+ &%space",
// 	"+ #; &%eol",
// 	"+ ##! &%eol",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"s #foo",
// 	"s #foo",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"s &f &b",
// 	"f #f &f ",
// 	"f ",
// 	"b #b",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"comment #; + &%eol",
// 	"comment ##! + &%eol",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"E #a &A",
// 	"E #b &B",
// 	"A #c &A",
// 	"A #d",
// 	"B #c &B",
// 	"B #d",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"S &B &B",
// 	"B #a &B",
// 	"B #b",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"E &E #+ &T",
// 	"E &T",
// 	"T &T #* &F",
// 	"T &F",
// 	"F #0",
// 	"F #1",
// 	"F #( &E #)",
// 	NULL,
// };


// const char* example_grammer[]=
// {
// 	"S #a &A #d",
// 	"S #b &B #d",
// 	"S #a &B #e",
// 	"S #b &A #e",
// 	"A #c",
// 	"B #c",
// 	NULL,
// };

// const char* example_grammer[]=
// {
// 	"S &L #= &R",
// 	"S &R",
// 	"L #* &R",
// 	"L #id",
// 	"R &L",
// 	NULL,
// };

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
	const FklString* s=nodes[1]->str;
	size_t size=0;
	char* cstr=fklCastEscapeCharBuf(s->str,s->size,&size);
	FklString* str=fklCreateString(size,cstr);
	FklNastNode* node=fklCreateNastNode(FKL_NAST_STR,nodes[1]->curline);
	node->str=str;
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
	pair->pair=fklCreateNastPair();
	pair->pair->car=fklMakeNastNodeRef(car);
	pair->pair->cdr=fklMakeNastNodeRef(cdr);
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
	pair->pair=fklCreateNastPair();
	pair->pair->car=fklMakeNastNodeRef(car);
	pair->pair->cdr=fklMakeNastNodeRef(cdr);
	return pair;
}

static FklNastNode* prod_action_dec_integer(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=nodes[0]->str;
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

static FklNastNode* prod_action_hex_integer(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=nodes[0]->str;
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

static FklNastNode* prod_action_oct_integer(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=nodes[0]->str;
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

static FklNastNode* prod_action_float(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=nodes[0]->str;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_F64,line);
	double i=strtod(str->str,NULL);
	r->f64=i;
	return r;
}

static FklNastNode* prod_action_any_char(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	c->chr=nodes[1]->str->str[0];
	return c;
}

static FklNastNode* prod_action_esc_char(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	static const char* escapeChars=FKL_ESCAPE_CHARS;
	static const char* escapeCharsTo=FKL_ESCAPE_CHARS_TO;

	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	FklString* str=nodes[1]->str;
	if(str->size)
	{
		char ch=toupper(nodes[1]->str->str[0]);
		for(size_t i=0;escapeChars[i];i++)
			if(ch==escapeChars[i])
			{
				c->chr=escapeCharsTo[i];
				return c;
			}
		c->chr=nodes[1]->str->str[0];
	}
	else
		c->chr='\\';
	return c;
}

static FklNastNode* prod_action_dec_char(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	c->chr=strtol(nodes[1]->str->str,NULL,10);
	return c;
}

static FklNastNode* prod_action_hex_char(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	c->chr=strtol(nodes[1]->str->str,NULL,16);
	return c;
}

static FklNastNode* prod_action_oct_char(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* c=fklCreateNastNode(FKL_NAST_CHR,line);
	const FklString* s=nodes[1]->str;
	if(s->size)
		c->chr=strtol(s->str,NULL,8);
	else
		c->chr=0;
	return c;
}

static FklNastNode* prod_action_box(const FklGrammerProduction* prod
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);
	box->box=fklMakeNastNodeRef(nodes[1]);
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
	r->vec=vec;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
		vec->base[i]=fklMakeNastNodeRef(list->pair->car);
	return r;
}

static inline FklNastNode* create_nast_list(FklNastNode** a,size_t num,uint64_t line)
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

static FklNastNode* prod_action_quote(const FklGrammerProduction* prod
		,FklNastNode** nodes
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

static FklNastNode* prod_action_unquote(const FklGrammerProduction* prod
		,FklNastNode** nodes
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

static FklNastNode* prod_action_qsquote(const FklGrammerProduction* prod
		,FklNastNode** nodes
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

static FklNastNode* prod_action_unqtesp(const FklGrammerProduction* prod
		,FklNastNode** nodes
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
	pair->pair=fklCreateNastPair();
	pair->pair->car=fklMakeNastNodeRef(car);
	pair->pair->cdr=fklMakeNastNodeRef(cdr);

	FklNastNode* r=fklCreateNastNode(FKL_NAST_PAIR,line);
	r->pair=fklCreateNastPair();
	r->pair->car=pair;
	r->pair->cdr=fklMakeNastNodeRef(rest);
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
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
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
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
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
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
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

	{"char ##\\ + &%any",             prod_action_any_char,     },
	{"char ##\\\\ + &%esc",           prod_action_esc_char,     },
	{"char ##\\\\ + &%dec3",          prod_action_dec_char,      },
	{"char ##\\\\0 + &%oct3",         prod_action_oct_char,      },
	{"char ##\\\\x + &%hex2",         prod_action_hex_char,      },
	{"char ##\\\\X + &%hex2",         prod_action_hex_char,      },

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

// {"symbol #| + &%qstr + #|",          prod_action_string,        },
// {"symbol &%identifier",          prod_action_return_first,        },
// {"symbol &symbol + &%identifier",          prod_action_return_first,        },
// {"symbol &symbol + #| + &%qstr + #|",          prod_action_return_first,        },

// static const FklGrammerCstrAction example_grammer[]=
// {
// 	{"E &E #+ &T", prod_action_return_first, },
// 	{"E &T",       prod_action_return_first, },
// 	{"T &T #* &F", prod_action_return_first, },
// 	{"T &F",       prod_action_return_first, },
// 	{"F #0",       prod_action_return_first, },
// 	{"F #1",       prod_action_return_first, },
// 	{"F #( &E #)", prod_action_return_first, },
// 	{"+ &%space",  NULL,                     },
// 	{NULL,         NULL,                     },
// };

// static const FklGrammerCstrAction example_grammer[]=
// {
// 	{"ss &s &b #c", prod_action_return_first, },
// 	{"s #d",        prod_action_return_first, },
// 	{"b &b #a",     prod_action_return_first, },
// 	{"b ",          prod_action_return_first, },
// 	{"+ &%space",   NULL,                     },
// 	{NULL,          NULL,                     },
// };

int main()
{
	FklSymbolTable* st=fklCreateSymbolTable();
	// FklGrammer* g=fklCreateGrammerFromCstr(example_grammer,st);
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
	// const char* exp="1*(1+1)";
	// const char* exp=" 1 * (1 + 1) ";
	// const char* exp="foobar|foo|foobar|bar|";
	// const char* exp=" a c d ";
	// const char* exp=" a c dd";
	// const char* exp="fffb";
	// const char* exp="(\"foo\" \"bar\" \"foobar\",;abcd\n\"i\")";
	// const char* exp="[(foobar;comments\nfoo bar),abcd]";
	// const char* exp="(foo bar abcd|foo \\|bar|efgh foo \"foo\\\"\",bar)";
	
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
		"'#&#(foo 0x114514 \"foobar\" .1 0x1p1 114514|foo|bar #\\a #\\\\0 #\\\\x11 #\\\\0123 #\\\\0177 #\\\\0777)",
		"#hash((a,1) (b,2))",
		"#vu8(114 514 114514)",
		"114514",
		"#\\ ",
		// "#\\",
		NULL,
	};

	// const char* exp="'#&#(foo 0x114514 \"foobar\" .1 0x1p1 114514|foo|bar #\\a #\\\\0 #\\\\x11 #\\\\0123 #\\\\0177 #\\\\0777)";
	// const char* exp="“114514”";
	FklPtrStack tokenStack=FKL_STACK_INIT;
	int retval=0;
	for(const char** exp=&exps[0];*exp;exp++)
	{
		fklInitPtrStack(&tokenStack,16,8);

		FklGrammerMatchOuterCtx outerCtx=FKL_GRAMMER_MATCH_OUTER_CTX_INIT;

		retval=fklParseForCstrDbg(&g->aTable,*exp,&tokenStack,&outerCtx,st);

		fputc('\n',stdout);
		for(size_t i=0;i<tokenStack.top;i++)
		{
			FklString* s=tokenStack.base[i];
			fprintf(stdout,"%s\n",s->str);
			free(s);
		}
		fputc('\n',stdout);

		fklUninitPtrStack(&tokenStack);
		if(retval)
			break;
	}
	FILE* c_parser_file=fopen("c_parser_func.c","w");
	fklPrintAnalysisTableAsCfunc(g,st,c_parser_file);
	fclose(c_parser_file);
	fklDestroyGrammer(g);
	fklDestroySymbolTable(st);
	return retval;
}
