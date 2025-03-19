#include<fakeLisp/grammer.h>
#include<fakeLisp/vm.h>

static inline void* prod_action_symbol(void* outerCtx
		,void* ast[]
		,size_t num
		,size_t line)
{
	FklVMvalue** values=(FklVMvalue**)ast;
	const char* start="|";
	size_t start_size=1;
	const char* end="|";
	size_t end_size=1;

	const FklString* str=FKL_VM_STR(values[0]);
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
	FklSid_t id=fklVMaddSymbolCharBuf(((FklVM*)outerCtx)->gc,buffer.buf,buffer.index)->id;
	fklUninitStringBuffer(&buffer);
	FklVMvalue* retval=FKL_MAKE_VM_SYM(id);
	return retval;
}

static inline void* prod_action_return_first(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	return nodes[0];
}

static inline void* prod_action_return_second(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	return nodes[1];
}

static inline void* prod_action_string(void* outerCtx
		,void* ast[]
		,size_t num
		,size_t line)
{
	FklVMvalue** values=(FklVMvalue**)ast;
	size_t start_size=1;
	size_t end_size=1;

	const FklString* str=FKL_VM_STR(values[0]);
	const char* cstr=str->str;

	size_t size=0;
	char* s=fklCastEscapeCharBuf(&cstr[start_size],str->size-end_size-start_size,&size);

	FklVM* exe=(FklVM*)outerCtx;
	FklVMvalue* retval=fklCreateVMvalueStr2(exe,size,s);

	free(s);
	return retval;
}


static inline void* prod_action_nil(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	return FKL_VM_NIL;
}

static inline void* prod_action_pair(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklVMvalue* car=nodes[0];
	FklVMvalue* cdr=nodes[2];
	FklVMvalue* pair=fklCreateVMvaluePair(exe,car,cdr);
	return pair;
}

static inline void* prod_action_list(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklVMvalue* car=nodes[0];
	FklVMvalue* cdr=nodes[1];
	FklVMvalue* pair=fklCreateVMvaluePair(exe,car,cdr);
	return pair;
}

static inline void* prod_action_dec_integer(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	const FklString* str=FKL_VM_STR((FklVMvalue*)nodes[0]);
	int64_t i=strtoll(str->str,NULL,10);
	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
		return fklCreateVMvalueBigIntWithDecString(exe,str);
	else
		return FKL_MAKE_VM_FIX(i);
}

static inline void* prod_action_hex_integer(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	const FklString* str=FKL_VM_STR((FklVMvalue*)nodes[0]);
	int64_t i=strtoll(str->str,NULL,16);
	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
		return fklCreateVMvalueBigIntWithHexString(exe,str);
	else
		return FKL_MAKE_VM_FIX(i);
}

static inline void* prod_action_oct_integer(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	const FklString* str=FKL_VM_STR((FklVMvalue*)nodes[0]);
	int64_t i=strtoll(str->str,NULL,8);
	if(i>FKL_FIX_INT_MAX||i<FKL_FIX_INT_MIN)
		return fklCreateVMvalueBigIntWithOctString(exe,str);
	else
		return FKL_MAKE_VM_FIX(i);
}

static inline void* prod_action_float(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	const FklString* str=FKL_VM_STR((FklVMvalue*)nodes[0]);
	return fklCreateVMvalueF64(exe,strtod(str->str,NULL));
}

static inline void* prod_action_char(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	const FklString* str=FKL_VM_STR((FklVMvalue*)nodes[0]);
	if(!fklIsValidCharBuf(str->str+2,str->size-2))
		return NULL;
	return FKL_MAKE_VM_CHR(fklCharBufToChar(str->str+2,str->size-2));
}

static inline void* prod_action_box(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	return fklCreateVMvalueBox(exe,nodes[1]);
}

static inline void* prod_action_vector(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklVMvalue* list=nodes[1];
	size_t len=fklVMlistLength(list);
	FklVMvalue* r=fklCreateVMvalueVec(exe,len);
	FklVMvec* vec=FKL_VM_VEC(r);
	for(size_t i=0;FKL_IS_PAIR(list);list=FKL_VM_CDR(list),i++)
		vec->base[i]=FKL_VM_CAR(list);
	return r;
}

// static inline void* prod_action_dvector(void* outerCtx
// 		,void* nodes[]
// 		,size_t num
// 		,size_t line)
// {
// 	FklVM* exe=(FklVM*)outerCtx;
// 	FklVMvalue* list=nodes[1];
// 	size_t len=fklVMlistLength(list);
// 	FklVMvalue* r=fklCreateVMvalueDvec(exe,len);
// 	FklVMdvec* vec=FKL_VM_DVEC(r);
// 	for(size_t i=0;FKL_IS_PAIR(list);list=FKL_VM_CDR(list),i++)
// 		vec->base[i]=FKL_VM_CAR(list);
// 	return r;
// }

static inline FklVMvalue* create_vmvalue_list(FklVM* exe,FklVMvalue** a,size_t num)
{
	FklVMvalue* r=NULL;
	FklVMvalue** cur=&r;
	for(size_t i=0;i<num;i++)
	{
		(*cur)=fklCreateVMvaluePairWithCar(exe,a[i]);
		cur=&FKL_VM_CDR(*cur);
	}
	(*cur)=FKL_VM_NIL;
	return r;
}

static inline void* prod_action_quote(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklSid_t id=fklVMaddSymbolCstr(exe->gc,"quote")->id;
	FklVMvalue* rest=nodes[1];
	FklVMvalue* head=FKL_MAKE_VM_SYM(id);

	FklVMvalue* items[]={head,rest};

	return create_vmvalue_list(exe,items,2);
}

static inline void* prod_action_unquote(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklSid_t id=fklVMaddSymbolCstr(exe->gc,"unquote")->id;
	FklVMvalue* rest=nodes[1];
	FklVMvalue* head=FKL_MAKE_VM_SYM(id);

	FklVMvalue* items[]={head,rest};

	return create_vmvalue_list(exe,items,2);
}

static inline void* prod_action_qsquote(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklSid_t id=fklVMaddSymbolCstr(exe->gc,"qsquote")->id;
	FklVMvalue* rest=nodes[1];
	FklVMvalue* head=FKL_MAKE_VM_SYM(id);

	FklVMvalue* items[]={head,rest};

	return create_vmvalue_list(exe,items,2);
}

static inline void* prod_action_unqtesp(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklSid_t id=fklVMaddSymbolCstr(exe->gc,"unqtesp")->id;
	FklVMvalue* rest=nodes[1];
	FklVMvalue* head=FKL_MAKE_VM_SYM(id);

	FklVMvalue* items[]={head,rest};

	return create_vmvalue_list(exe,items,2);
}

static inline void* prod_action_kv_list(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;

	FklVMvalue* car=nodes[1];
	FklVMvalue* cdr=nodes[3];
	FklVMvalue* rest=nodes[5];

	FklVMvalue* pair=fklCreateVMvaluePair(exe,car,cdr);

	FklVMvalue* r=fklCreateVMvaluePair(exe,pair,rest);
	return r;
}

static inline void* prod_action_hasheq(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklVMvalue* list=nodes[1];
	FklVMvalue* r=fklCreateVMvalueHashEq(exe);
	for(;FKL_IS_PAIR(list);list=FKL_VM_CDR(list))
	{
		FklVMvalue* key_value=FKL_VM_CAR(list);
		fklVMhashTableSet(FKL_VM_CAR(key_value),FKL_VM_CDR(key_value),FKL_VM_HASH(r));
	}
	return r;
}

static inline void* prod_action_hasheqv(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklVMvalue* list=nodes[1];
	FklVMvalue* r=fklCreateVMvalueHashEqv(exe);
	for(;FKL_IS_PAIR(list);list=FKL_VM_CDR(list))
	{
		FklVMvalue* key_value=FKL_VM_CAR(list);
		fklVMhashTableSet(FKL_VM_CAR(key_value),FKL_VM_CDR(key_value),FKL_VM_HASH(r));
	}
	return r;
}

static inline void* prod_action_hashequal(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;
	FklVMvalue* list=nodes[1];
	FklVMvalue* r=fklCreateVMvalueHashEqual(exe);
	for(;FKL_IS_PAIR(list);list=FKL_VM_CDR(list))
	{
		FklVMvalue* key_value=FKL_VM_CAR(list);
		fklVMhashTableSet(FKL_VM_CAR(key_value),FKL_VM_CDR(key_value),FKL_VM_HASH(r));
	}
	return r;
}

static inline void* prod_action_bytevector(void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklVM* exe=(FklVM*)outerCtx;

	FklVMvalue* list=nodes[1];
	size_t len=fklVMlistLength(list);
	FklBytevector* bv=fklCreateBytevector(len,NULL);

	for(size_t i=0;FKL_IS_PAIR(list);list=FKL_VM_CDR(list),i++)
	{
		FklVMvalue* cur=FKL_VM_CAR(list);
		if(FKL_IS_FIX(cur))
		{
			int64_t fix=FKL_GET_FIX(cur);
			bv->ptr[i]=fix>UINT8_MAX?UINT8_MAX:(fix<0?0:fix);
		}
		else
			bv->ptr[i]=FKL_VM_BI(cur)->num<0?0:UINT8_MAX;
	}

	FklVMvalue* r=fklCreateVMvalueBvec(exe,bv);
	return r;
}
