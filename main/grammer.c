#include<fakeLisp/grammer.h>
#include<fakeLisp/base.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<ctype.h>

static inline FklNastNode* prod_action_symbol(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const char* start="|";
	size_t start_size=1;
	const char* end="|";
	size_t end_size=1;

	const FklString* str=nodes[0]->str;
	const char* cstr=str->str;
	size_t cstr_size=str->size;

	FklStringBuffer buffer;
	fklInitStringBuffer(&buffer);
	while(*cstr)
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
		for(;cstr[len];len++)
			if(fklCharBufMatch(start,start_size,cstr+len,cstr_size-len))
				break;
		fklStringBufferBincpy(&buffer,cstr,len);
		cstr+=len;
		cstr_size-=len;
	}
	FklSid_t id=fklAddSymbolCharBuf(buffer.b,buffer.i,st)->id;
	FklNastNode* node=fklCreateNastNode(FKL_NAST_SYM,nodes[0]->curline);
	node->sym=id;
	fklUninitStringBuffer(&buffer);
	return node;
}

static inline FklNastNode* prod_action_return_first(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklMakeNastNodeRef(nodes[0]);
}

static inline FklNastNode* prod_action_return_second(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklMakeNastNodeRef(nodes[1]);
}

static inline FklNastNode* prod_action_string(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
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

static inline FklNastNode* prod_action_nil(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklCreateNastNode(FKL_NAST_NIL,line);
}

static inline FklNastNode* prod_action_pair(void* ctx
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

static inline FklNastNode* prod_action_list(void* ctx
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

static inline FklNastNode* prod_action_dec_integer(void* ctx
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

static inline FklNastNode* prod_action_hex_integer(void* ctx
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

static inline FklNastNode* prod_action_oct_integer(void* ctx
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

static inline FklNastNode* prod_action_float(void* ctx
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

static inline FklNastNode* prod_action_char(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	const FklString* str=nodes[0]->str;
	if(!fklIsValidCharBuf(str->str+2,str->size-2))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_CHR,line);
	r->chr=fklCharBufToChar(str->str+2,str->size-2);
	return r;
}

static inline FklNastNode* prod_action_box(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);
	box->box=fklMakeNastNodeRef(nodes[1]);
	return box;
}

static inline FklNastNode* prod_action_vector(void* ctx
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

static inline FklNastNode* prod_action_quote(void* ctx
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

static inline FklNastNode* prod_action_unquote(void* ctx
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

static inline FklNastNode* prod_action_qsquote(void* ctx
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

static inline FklNastNode* prod_action_unqtesp(void* ctx
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

static inline FklNastNode* prod_action_kv_list(void* ctx
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

static inline FklNastNode* prod_action_hasheq(void* ctx
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

static inline FklNastNode* prod_action_hasheqv(void* ctx
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

static inline FklNastNode* prod_action_hashequal(void* ctx
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

static inline FklNastNode* prod_action_bytevector(void* ctx
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

static const char* prod_actions_src=
"static inline FklNastNode* prod_action_symbol(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	const char* start=\"|\";\n"
"	size_t start_size=1;\n"
"	const char* end=\"|\";\n"
"	size_t end_size=1;\n"
"\n"
"	const FklString* str=nodes[0]->str;\n"
"	const char* cstr=str->str;\n"
"	size_t cstr_size=str->size;\n"
"\n"
"	FklStringBuffer buffer;\n"
"	fklInitStringBuffer(&buffer);\n"
"	while(*cstr)\n"
"	{\n"
"		if(fklCharBufMatch(start,start_size,cstr,cstr_size))\n"
"		{\n"
"			cstr+=start_size;\n"
"			cstr_size-=start_size;\n"
"			size_t len=fklQuotedCharBufMatch(cstr,cstr_size,end,end_size);\n"
"			if(!len)\n"
"				return 0;\n"
"			size_t size=0;\n"
"			char* s=fklCastEscapeCharBuf(cstr,len-end_size,&size);\n"
"			fklStringBufferBincpy(&buffer,s,size);\n"
"			free(s);\n"
"			cstr+=len;\n"
"			cstr_size-=len;\n"
"			continue;\n"
"		}\n"
"		size_t len=0;\n"
"		for(;cstr[len];len++)\n"
"			if(fklCharBufMatch(start,start_size,cstr+len,cstr_size-len))\n"
"				break;\n"
"		fklStringBufferBincpy(&buffer,cstr,len);\n"
"		cstr+=len;\n"
"		cstr_size-=len;\n"
"	}\n"
"	FklSid_t id=fklAddSymbolCharBuf(buffer.b,buffer.i,st)->id;\n"
"	FklNastNode* node=fklCreateNastNode(FKL_NAST_SYM,nodes[0]->curline);\n"
"	node->sym=id;\n"
"	fklUninitStringBuffer(&buffer);\n"
"	return node;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_return_first(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	return fklMakeNastNodeRef(nodes[0]);\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_return_second(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	return fklMakeNastNodeRef(nodes[1]);\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_string(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	size_t start_size=1;\n"
"	size_t end_size=1;\n"
"\n"
"	const FklString* str=nodes[0]->str;\n"
"	const char* cstr=str->str;\n"
"\n"
"	size_t size=0;\n"
"	char* s=fklCastEscapeCharBuf(&cstr[start_size],str->size-end_size-start_size,&size);\n"
"	FklNastNode* node=fklCreateNastNode(FKL_NAST_STR,nodes[0]->curline);\n"
"	node->str=fklCreateString(size,s);\n"
"	free(s);\n"
"	return node;\n"
"}\n"
"\n"
"\n"
"static inline FklNastNode* prod_action_nil(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	return fklCreateNastNode(FKL_NAST_NIL,line);\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_pair(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklNastNode* car=nodes[0];\n"
"	FklNastNode* cdr=nodes[2];\n"
"	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);\n"
"	pair->pair=fklCreateNastPair();\n"
"	pair->pair->car=fklMakeNastNodeRef(car);\n"
"	pair->pair->cdr=fklMakeNastNodeRef(cdr);\n"
"	return pair;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_list(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklNastNode* car=nodes[0];\n"
"	FklNastNode* cdr=nodes[1];\n"
"	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);\n"
"	pair->pair=fklCreateNastPair();\n"
"	pair->pair->car=fklMakeNastNodeRef(car);\n"
"	pair->pair->cdr=fklMakeNastNodeRef(cdr);\n"
"	return pair;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_dec_integer(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	const FklString* str=nodes[0]->str;\n"
"	int64_t i=strtoll(str->str,NULL,10);\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_FIX,line);\n"
"	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)\n"
"	{\n"
"		FklBigInt bInt=FKL_BIG_INT_INIT;\n"
"		fklInitBigIntFromDecString(&bInt,str);\n"
"		FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));\n"
"		FKL_ASSERT(bi);\n"
"		*bi=bInt;\n"
"		r->bigInt=bi;\n"
"		r->type=FKL_NAST_BIG_INT;\n"
"	}\n"
"	else\n"
"	{\n"
"		r->type=FKL_NAST_FIX;\n"
"		r->fix=i;\n"
"	}\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_hex_integer(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	const FklString* str=nodes[0]->str;\n"
"	int64_t i=strtoll(str->str,NULL,16);\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_FIX,line);\n"
"	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)\n"
"	{\n"
"		FklBigInt bInt=FKL_BIG_INT_INIT;\n"
"		fklInitBigIntFromHexString(&bInt,str);\n"
"		FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));\n"
"		FKL_ASSERT(bi);\n"
"		*bi=bInt;\n"
"		r->bigInt=bi;\n"
"		r->type=FKL_NAST_BIG_INT;\n"
"	}\n"
"	else\n"
"	{\n"
"		r->type=FKL_NAST_FIX;\n"
"		r->fix=i;\n"
"	}\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_oct_integer(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	const FklString* str=nodes[0]->str;\n"
"	int64_t i=strtoll(str->str,NULL,8);\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_FIX,line);\n"
"	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)\n"
"	{\n"
"		FklBigInt bInt=FKL_BIG_INT_INIT;\n"
"		fklInitBigIntFromOctString(&bInt,str);\n"
"		FklBigInt* bi=(FklBigInt*)malloc(sizeof(FklBigInt));\n"
"		FKL_ASSERT(bi);\n"
"		*bi=bInt;\n"
"		r->bigInt=bi;\n"
"		r->type=FKL_NAST_BIG_INT;\n"
"	}\n"
"	else\n"
"	{\n"
"		r->type=FKL_NAST_FIX;\n"
"		r->fix=i;\n"
"	}\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_float(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	const FklString* str=nodes[0]->str;\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_F64,line);\n"
"	double i=strtod(str->str,NULL);\n"
"	r->f64=i;\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_char(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	const FklString* str=nodes[0]->str;\n"
"	if(!fklIsValidCharBuf(str->str+2,str->size-2))\n"
"		return NULL;\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_CHR,line);\n"
"	r->chr=fklCharBufToChar(str->str+2,str->size-2);\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_box(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);\n"
"	box->box=fklMakeNastNodeRef(nodes[1]);\n"
"	return box;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_vector(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklNastNode* list=nodes[1];\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_VECTOR,line);\n"
"	size_t len=fklNastListLength(list);\n"
"	FklNastVector* vec=fklCreateNastVector(len);\n"
"	r->vec=vec;\n"
"	size_t i=0;\n"
"	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)\n"
"		vec->base[i]=fklMakeNastNodeRef(list->pair->car);\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* create_nast_list(FklNastNode** a,size_t num,uint64_t line)\n"
"{\n"
"	FklNastNode* r=NULL;\n"
"	FklNastNode** cur=&r;\n"
"	for(size_t i=0;i<num;i++)\n"
"	{\n"
"		(*cur)=fklCreateNastNode(FKL_NAST_PAIR,a[i]->curline);\n"
"		(*cur)->pair=fklCreateNastPair();\n"
"		(*cur)->pair->car=a[i];\n"
"		cur=&(*cur)->pair->cdr;\n"
"	}\n"
"	(*cur)=fklCreateNastNode(FKL_NAST_NIL,line);\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_quote(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklSid_t id=fklAddSymbolCstr(\"quote\",st)->id;\n"
"	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);\n"
"	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);\n"
"	head->sym=id;\n"
"	FklNastNode* s_exps[]={head,s_exp};\n"
"	return create_nast_list(s_exps,2,line);\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_unquote(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklSid_t id=fklAddSymbolCstr(\"unquote\",st)->id;\n"
"	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);\n"
"	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);\n"
"	head->sym=id;\n"
"	FklNastNode* s_exps[]={head,s_exp};\n"
"	return create_nast_list(s_exps,2,line);\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_qsquote(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklSid_t id=fklAddSymbolCstr(\"qsquote\",st)->id;\n"
"	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);\n"
"	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);\n"
"	head->sym=id;\n"
"	FklNastNode* s_exps[]={head,s_exp};\n"
"	return create_nast_list(s_exps,2,line);\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_unqtesp(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklSid_t id=fklAddSymbolCstr(\"unqtesp\",st)->id;\n"
"	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);\n"
"	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);\n"
"	head->sym=id;\n"
"	FklNastNode* s_exps[]={head,s_exp};\n"
"	return create_nast_list(s_exps,2,line);\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_kv_list(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklNastNode* car=nodes[1];\n"
"	FklNastNode* cdr=nodes[3];\n"
"	FklNastNode* rest=nodes[5];\n"
"\n"
"	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);\n"
"	pair->pair=fklCreateNastPair();\n"
"	pair->pair->car=fklMakeNastNodeRef(car);\n"
"	pair->pair->cdr=fklMakeNastNodeRef(cdr);\n"
"\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_PAIR,line);\n"
"	r->pair=fklCreateNastPair();\n"
"	r->pair->car=pair;\n"
"	r->pair->cdr=fklMakeNastNodeRef(rest);\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_hasheq(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklNastNode* list=nodes[1];\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);\n"
"	size_t len=fklNastListLength(list);\n"
"	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQ,len);\n"
"	r->hash=ht;\n"
"	size_t i=0;\n"
"	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)\n"
"	{\n"
"		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);\n"
"		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);\n"
"	}\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_hasheqv(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklNastNode* list=nodes[1];\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);\n"
"	size_t len=fklNastListLength(list);\n"
"	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQV,len);\n"
"	r->hash=ht;\n"
"	size_t i=0;\n"
"	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)\n"
"	{\n"
"		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);\n"
"		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);\n"
"	}\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_hashequal(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklNastNode* list=nodes[1];\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);\n"
"	size_t len=fklNastListLength(list);\n"
"	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQUAL,len);\n"
"	r->hash=ht;\n"
"	size_t i=0;\n"
"	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)\n"
"	{\n"
"		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);\n"
"		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);\n"
"	}\n"
"	return r;\n"
"}\n"
"\n"
"static inline FklNastNode* prod_action_bytevector(FklNastNode** nodes\n"
"		,size_t num\n"
"		,size_t line\n"
"		,FklSymbolTable* st)\n"
"{\n"
"	FklNastNode* list=nodes[1];\n"
"	FklNastNode* r=fklCreateNastNode(FKL_NAST_BYTEVECTOR,line);\n"
"	size_t len=fklNastListLength(list);\n"
"	FklBytevector* bv=fklCreateBytevector(len,NULL);\n"
"	r->bvec=bv;\n"
"	size_t i=0;\n"
"	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)\n"
"	{\n"
"		FklNastNode* cur=list->pair->car;\n"
"		if(cur->type==FKL_NAST_FIX)\n"
"			bv->ptr[i]=cur->fix>UINT8_MAX?UINT8_MAX:(cur->fix<0?0:cur->fix);\n"
"		else\n"
"			bv->ptr[i]=cur->bigInt->neg?0:UINT8_MAX;\n"
"	}\n"
"	return r;\n"
"}\n";

static const FklGrammerCstrAction builtin_grammer_and_action[]=
{
	{"*s-exp* &*list*",                   "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*vector*",                 "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*bytevector*",             "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*box*",                    "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*hasheq*",                 "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*hasheqv*",                "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*hashequal*",              "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*string*",                 "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*symbol*",                 "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*integer*",                "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*float*",                  "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*char*",                   "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*quote*",                  "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*qsquote*",                "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*unquote*",                "prod_action_return_first",  prod_action_return_first,  },
	{"*s-exp* &*unqtesp*",                "prod_action_return_first",  prod_action_return_first,  },

	{"*quote* #' &*s-exp*",               "prod_action_quote",         prod_action_quote,         },
	{"*qsquote* #` &*s-exp*",             "prod_action_qsquote",       prod_action_qsquote,       },
	{"*unquote* #~ &*s-exp*",             "prod_action_unquote",       prod_action_unquote,       },
	{"*unqtesp* #~@ &*s-exp*",            "prod_action_unqtesp",       prod_action_unqtesp,       },

	{"*symbol* &%symbol + #|",            "prod_action_symbol",        prod_action_symbol,        },
	{"*string* &%string + #\"",           "prod_action_string",        prod_action_string,        },

	{"*integer* &%s-dint + #|",           "prod_action_dec_integer",   prod_action_dec_integer,   },
	{"*integer* &%s-xint + #|",           "prod_action_hex_integer",   prod_action_hex_integer,   },
	{"*integer* &%s-oint + #|",           "prod_action_oct_integer",   prod_action_oct_integer,   },

	{"*float* &%s-dfloat + #|",           "prod_action_float",         prod_action_float,         },
	{"*float* &%s-xfloat + #|",           "prod_action_float",         prod_action_float,         },

	{"*char* &%s-char ##\\",              "prod_action_char",          prod_action_char,          },

	{"*box* ##& &*s-exp*",                "prod_action_box",           prod_action_box,           },

	{"*list* #( &l #)",                   "prod_action_return_second", prod_action_return_second, },
	{"*list* #[ &l #]",                   "prod_action_return_second", prod_action_return_second, },

	{"l ",                                "prod_action_nil",           prod_action_nil,           },
	{"l &*s-exp* &l",                     "prod_action_list",          prod_action_list,          },
	{"l &*s-exp* #, &*s-exp*",            "prod_action_pair",          prod_action_pair,          },

	{"*vector* ##( &v #)",                "prod_action_vector",        prod_action_vector,        },
	{"*vector* ##[ &v #]",                "prod_action_vector",        prod_action_vector,        },

	{"v ",                                "prod_action_nil",           prod_action_nil,           },
	{"v &*s-exp* &v",                     "prod_action_list",          prod_action_list,          },

	{"*hasheq* ##hash( &kv #)",           "prod_action_hasheq",        prod_action_hasheq,        },
	{"*hasheq* ##hash[ &kv #]",           "prod_action_hasheq",        prod_action_hasheq,        },

	{"*hasheqv* ##hasheqv( &kv #)",       "prod_action_hasheqv",       prod_action_hasheqv,       },
	{"*hasheqv* ##hasheqv[ &kv #]",       "prod_action_hasheqv",       prod_action_hasheqv,       },

	{"*hashequal* ##hashequal( &kv #)",   "prod_action_hashequal",     prod_action_hashequal,     },
	{"*hashequal* ##hashequal[ &kv #]",   "prod_action_hashequal",     prod_action_hashequal,     },

	{"kv ",                               "prod_action_nil",           prod_action_nil,           },
	{"kv #( &*s-exp* #, &*s-exp* #) &kv", "prod_action_kv_list",       prod_action_kv_list,       },
	{"kv #[ &*s-exp* #, &*s-exp* #] &kv", "prod_action_kv_list",       prod_action_kv_list,       },

	{"*bytevector* ##vu8( &v8 #)",        "prod_action_bytevector",    prod_action_bytevector,    },
	{"*bytevector* ##vu8[ &v8 #]",        "prod_action_bytevector",    prod_action_bytevector,    },

	{"v8 ",                               "prod_action_nil",           prod_action_nil,           },
	{"v8 &*integer* &v8",                 "prod_action_list",          prod_action_list,          },

	{"+ &%space",                         NULL,                        NULL,                      },
	{"+ #; &%eol",                        NULL,                        NULL,                      },
	{"+ ##! &%eol",                       NULL,                        NULL,                      },
	{NULL,                                NULL,                        NULL,                      },
};

int main()
{
	FklSymbolTable* st=fklCreateSymbolTable();
	FklGrammer* g=fklCreateGrammerFromCstrAction(builtin_grammer_and_action,st);
	if(!g)
	{
		fklDestroySymbolTable(st);
		fprintf(stderr,"garmmer create fail\n");
		exit(1);
	}

	FklHashTable* itemSet=fklGenerateLr0Items(g);
	fklLr0ToLalrItems(itemSet,g);

	if(fklGenerateLalrAnalyzeTable(g,itemSet))
	{
		fklDestroySymbolTable(st);
		fprintf(stderr,"not lalr garmmer\n");
		exit(1);
	}
	FILE* parse=fopen("parse.c","w");
	fklPrintAnalysisTableAsCfunc(g,st,&prod_actions_src,1,parse);
	fclose(parse);
	return 0;
}
