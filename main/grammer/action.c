#include<fakeLisp/grammer.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>

static inline void* prod_action_symbol(void* outerCtx
		,void* ast[]
		,size_t num
		,size_t line)
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
	FklSid_t id=fklAddSymbolCharBuf(buffer.buf,buffer.index,outerCtx)->id;
	FklNastNode* node=fklCreateNastNode(FKL_NAST_SYM,nodes[0]->curline);
	node->sym=id;
	fklUninitStringBuffer(&buffer);
	return node;
}

static inline void* prod_action_return_first(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	return fklMakeNastNodeRef(nodes[0]);
}

static inline void* prod_action_return_second(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	return fklMakeNastNodeRef(nodes[1]);
}

static inline void* prod_action_string(void* outerCtx
		,void* ast[]
		,size_t num
		,size_t line)
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


static inline void* prod_action_nil(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	return fklCreateNastNode(FKL_NAST_NIL,line);
}

static inline void* prod_action_pair(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklNastNode* car=nodes[0];
	FklNastNode* cdr=nodes[2];
	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
	pair->pair=fklCreateNastPair();
	pair->pair->car=fklMakeNastNodeRef(car);
	pair->pair->cdr=fklMakeNastNodeRef(cdr);
	return pair;
}

static inline void* prod_action_list(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklNastNode* car=nodes[0];
	FklNastNode* cdr=nodes[1];
	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
	pair->pair=fklCreateNastPair();
	pair->pair->car=fklMakeNastNodeRef(car);
	pair->pair->cdr=fklMakeNastNodeRef(cdr);
	return pair;
}

static inline void* prod_action_dec_integer(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
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

static inline void* prod_action_hex_integer(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
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

static inline void* prod_action_oct_integer(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
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

static inline void* prod_action_float(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	const FklString* str=((FklNastNode*)nodes[0])->str;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_F64,line);
	double i=strtod(str->str,NULL);
	r->f64=i;
	return r;
}

static inline void* prod_action_char(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	const FklString* str=((FklNastNode*)nodes[0])->str;
	if(!fklIsValidCharBuf(str->str+2,str->size-2))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_CHR,line);
	r->chr=fklCharBufToChar(str->str+2,str->size-2);
	return r;
}

static inline void* prod_action_box(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);
	box->box=fklMakeNastNodeRef(nodes[1]);
	return box;
}

static inline void* prod_action_vector(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
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

static inline void* prod_action_dvector(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_DVECTOR,line);
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

static inline void* prod_action_quote(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklSid_t id=fklAddSymbolCstr("quote",outerCtx)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* prod_action_unquote(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklSid_t id=fklAddSymbolCstr("unquote",outerCtx)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* prod_action_qsquote(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklSid_t id=fklAddSymbolCstr("qsquote",outerCtx)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* prod_action_unqtesp(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklSid_t id=fklAddSymbolCstr("unqtesp",outerCtx)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* prod_action_kv_list(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
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

static inline void* prod_action_hasheq(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
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

static inline void* prod_action_hasheqv(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
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

static inline void* prod_action_hashequal(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
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

static inline void* prod_action_bytevector(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
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
