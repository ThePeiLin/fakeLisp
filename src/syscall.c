#include"common.h"
#include"syscall.h"
#include"VMtool.h"
#include"reader.h"
#include"fakeVM.h"
#include<string.h>
#include<math.h>
#include<setjmp.h>
extern const char* builtInSymbolList[NUMOFBUILTINSYMBOL];
extern const char* builtInErrorType[NUMOFBUILTINERRORTYPE];
extern FakeVMlist GlobFakeVMs;
extern void* ThreadVMFunc(void* p);
extern void* ThreadVMDlprocFunc(void* p);
void SYS_car(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(obj->type!=PAIR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	SET_RETURN("SYS_car",getVMpairCar(obj),stack);
}

void SYS_cdr(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(obj->type!=PAIR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	SET_RETURN("SYS_cdr",getVMpairCdr(obj),stack);
}

void SYS_cons(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* car=getArg(stack);
	VMvalue* cdr=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!car||!cdr)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	VMvalue* pair=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
	copyRef(getVMpairCar(pair),car);
	copyRef(getVMpairCdr(pair),cdr);
	SET_RETURN("SYS_cons",pair,stack);
}

void SYS_append(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* retval=newNilValue(exe->heap);
	VMvalue* cur=getArg(stack);
	ValueType prevType=retval->type;
	VMvalue* prev=retval;
	for(;cur;cur=getArg(stack))
	{
		switch(prevType)
		{
			case NIL:
				copyRef(prev,copyVMvalue(cur,exe->heap));
				break;
			case PAIR:
				{
					for(;prev->type;prev=getVMpairCdr(prev));
					copyRef(prev,copyVMvalue(cur,exe->heap));
				}
				break;
			case STR:
				if(cur->type!=STR)
					RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
				prev->u.str->str=strCat(prev->u.str->str,cur->u.str->str);
				break;
			case BYTS:
				if(cur->type!=BYTS)
					RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
				VMBytsCat(prev->u.byts,cur->u.byts);
				break;
			default:
				RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
				break;
		}
		prevType=prev->type;
	}
	resBp(stack);
	SET_RETURN("SYS_append",retval,stack);
}

void SYS_atom(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* arg=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(arg->type!=PAIR)
		SET_RETURN("SYS_atom",newTrueValue(exe->heap),stack)
	else
		SET_RETURN("SYS_atom",newNilValue(exe->heap),stack);
}

void SYS_null(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* arg=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(arg->type==NIL)
		SET_RETURN("SYS_null",newTrueValue(exe->heap),stack)
	else
		SET_RETURN("SYS_null",newNilValue(exe->heap),stack);
}

void SYS_not(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* arg=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(arg->type==NIL)
		SET_RETURN("SYS_not",newTrueValue(exe->heap),stack)
	else
		SET_RETURN("SYS_not",newNilValue(exe->heap),stack);
}

void SYS_eq(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* fir=getArg(stack);
	VMvalue* sec=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	SET_RETURN("SYS_eq",subVMvaluecmp(fir,sec)
			?newTrueValue(exe->heap)
			:newNilValue(exe->heap)
			,stack);
}

void SYS_eqn(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* fir=getArg(stack);
	VMvalue* sec=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(fir->type==DBL||sec->type==DBL)
		SET_RETURN("SYS_eqn",((((fir->type==DBL)?*fir->u.dbl:*fir->u.in32)-((sec->type==DBL)?*sec->u.dbl:*sec->u.in32))==0.0)
				?newTrueValue(exe->heap)
				:newNilValue(exe->heap)
				,stack)
	else if(fir->type==IN32&&sec->type==IN32)
		SET_RETURN("SYS_eqn",(*fir->u.in32==*sec->u.in32)
				?newTrueValue(exe->heap)
				:newNilValue(exe->heap)
				,stack)
	else if(fir->type==sec->type&&(fir->type==SYM||fir->type==STR))
		SET_RETURN("SYS_eqn",(!strcmp(fir->u.str->str,sec->u.str->str))
				?newTrueValue(exe->heap)
				:newNilValue(exe->heap)
				,stack);
}

void SYS_equal(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* fir=getArg(stack);
	VMvalue* sec=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	SET_RETURN("SYS_equal",(VMvaluecmp(fir,sec))
			?newTrueValue(exe->heap)
			:newNilValue(exe->heap)
			,stack);
}

void SYS_add(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* cur=getArg(stack);
	int32_t ri=0;
	double rd=0.0;
	for(;cur;cur=getArg(stack))
		switch(cur->type)
		{
			case IN32:
				ri+=*cur->u.in32;
				break;
			case DBL:
				rd+=*cur->u.dbl;
				break;
			default:
				RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
		}
	resBp(stack);
	if(rd!=0.0)
	{
		rd+=ri;
		SET_RETURN("SYS_add",newVMvalue(DBL,&rd,exe->heap,1),stack);
	}
	else
		SET_RETURN("SYS_add",newVMvalue(IN32,&ri,exe->heap,1),stack)
}

void SYS_sub(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* prev=getArg(stack);
	VMvalue* cur=getArg(stack);
	int32_t ri=0;
	double rd=0.0;
	if(!prev)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(prev->type!=DBL&&prev->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(!cur)
	{
		resBp(stack);
		if(prev->type==DBL)
		{
			rd=-(*prev->u.dbl);
			SET_RETURN("SYS_sub",newVMvalue(DBL,&rd,exe->heap,1),stack);
		}
		else
		{
			ri=-(*prev->u.in32);
			SET_RETURN("SYS_sub",newVMvalue(IN32,&ri,exe->heap,1),stack);
		}
	}
	else
	{
		for(;cur;cur=getArg(stack))
			switch(cur->type)
			{
				case IN32:
					ri+=*cur->u.in32;
					break;
				case DBL:
					rd+=*cur->u.dbl;
					break;
				default:
					RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
					break;
			}
		resBp(stack);
		if(prev->type==DBL||rd!=0.0)
		{
			rd=((prev->type==DBL)?*prev->u.dbl:*prev->u.in32)-rd-ri;
			SET_RETURN("SYS_sub",newVMvalue(DBL,&rd,exe->heap,1),stack);
		}
		else
		{
			ri=*prev->u.in32-ri;
			SET_RETURN("SYS_sub",newVMvalue(IN32,&ri,exe->heap,1),stack);
		}
	}
}

void SYS_mul(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* cur=getArg(stack);
	int32_t ri=1;
	double rd=1.0;
	int haveDbl=0;
	for(;cur;cur=getArg(stack))
		switch(cur->type)
		{
			case IN32:
				ri*=*cur->u.in32;
				break;
			case DBL:
				haveDbl=1;
				rd*=*cur->u.dbl;
				break;
			default:
				RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
		}
	resBp(stack);
	if(haveDbl)
	{
		rd*=ri;
		SET_RETURN("SYS_add",newVMvalue(DBL,&rd,exe->heap,1),stack);
	}
	else
		SET_RETURN("SYS_add",newVMvalue(IN32,&ri,exe->heap,1),stack)
}

void SYS_div(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* prev=getArg(stack);
	VMvalue* cur=getArg(stack);
	int32_t ri=1;
	double rd=1.0;
	if(!prev)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(prev->type!=DBL&&prev->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	if(!cur)
	{
		resBp(stack);
		if(prev->type==DBL)
		{
			if(*prev->u.dbl==0.0)
				RAISE_BUILTIN_ERROR(DIVZERROERROR,runnable,exe);
			rd=1/(*prev->u.dbl);
			SET_RETURN("SYS_sub",newVMvalue(DBL,&rd,exe->heap,1),stack);
		}
		else
		{
			if(*prev->u.in32==0)
				RAISE_BUILTIN_ERROR(DIVZERROERROR,runnable,exe);
			if(1%(*prev->u.in32))
			{
				rd=1.0/(*prev->u.in32);
				SET_RETURN("SYS_sub",newVMvalue(DBL,&rd,exe->heap,1),stack);
			}
			else
			{
				ri=1/(*prev->u.in32);
				SET_RETURN("SYS_sub",newVMvalue(IN32,&ri,exe->heap,1),stack);
			}
		}
	}
	else
	{
		for(;cur;cur=getArg(stack))
			switch(cur->type)
			{
				case IN32:
					ri*=*cur->u.in32;
					break;
				case DBL:
					rd*=*cur->u.dbl;
					break;
				default:
					RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
					break;
			}
		if(ri==0)
			RAISE_BUILTIN_ERROR(DIVZERROERROR,runnable,exe);
		resBp(stack);
		if(prev->type==DBL||rd!=1.0||(prev->type==IN32&&(*prev->u.in32%ri)))
		{
			if(rd==0.0||ri==0)
				RAISE_BUILTIN_ERROR(DIVZERROERROR,runnable,exe);
			rd=((double)((prev->type==DBL)?*prev->u.dbl:*prev->u.in32))/rd/ri;
			SET_RETURN("SYS_div",newVMvalue(DBL,&rd,exe->heap,1),stack);
		}
		else
		{
			if(ri==0)
				RAISE_BUILTIN_ERROR(DIVZERROERROR,runnable,exe);
			ri=*prev->u.in32/ri;
			SET_RETURN("SYS_div",newVMvalue(IN32,&ri,exe->heap,1),stack);
		}
	}
}

void SYS_rem(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* fir=getArg(stack);
	VMvalue* sec=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(fir->type==DBL||sec->type==DBL)
	{
		double af=(fir->type==DBL)?*fir->u.dbl:*fir->u.in32;
		double as=(sec->type==DBL)?*sec->u.dbl:*sec->u.in32;
		if(as==0.0)
			RAISE_BUILTIN_ERROR(DIVZERROERROR,runnable,exe);
		double r=fmod(af,as);
		SET_RETURN("SYS_rem",newVMvalue(DBL,&r,exe->heap,1),stack);
	}
	else if(fir->type==IN32&&sec->type==IN32)
	{
		if(!(*sec->u.in32))
			RAISE_BUILTIN_ERROR(DIVZERROERROR,runnable,exe);
		int32_t r=*fir->u.in32%*sec->u.in32;
		SET_RETURN("SYS_rem",newVMvalue(IN32,&r,exe->heap,1),stack);
	}
	else
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
}

void SYS_gt(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int r=1;
	VMvalue* cur=getArg(stack);
	VMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	for(;cur;cur=getArg(stack))
	{
		if(prev)
		{
			if(prev->type==DBL||cur->type==DBL)
				r=((((prev->type==DBL)?*prev->u.dbl:*prev->u.in32)-((cur->type==DBL)?*cur->u.dbl:*cur->u.in32))>0.0);
			else if(prev->type==IN32&&cur->type==IN32)
				r=(*prev->u.in32>*cur->u.in32);
			else if(prev->type==cur->type&&(prev->type==STR))
				r=(strcmp(prev->u.str->str,cur->u.str->str)>0);
			else
				RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=getArg(stack));
	resBp(stack);
	SET_RETURN("SYS_gt",r
			?newTrueValue(exe->heap)
			:newNilValue(exe->heap)
			,stack);
}

void SYS_ge(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int r=1;
	VMvalue* cur=getArg(stack);
	VMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	for(;cur;cur=getArg(stack))
	{
		if(prev)
		{
			if(prev->type==DBL||cur->type==DBL)
				r=((((prev->type==DBL)?*prev->u.dbl:*prev->u.in32)-((cur->type==DBL)?*cur->u.dbl:*cur->u.in32))>=0.0);
			else if(prev->type==IN32&&cur->type==IN32)
				r=(*prev->u.in32>=*cur->u.in32);
			else if(prev->type==cur->type&&(prev->type==STR))
				r=(strcmp(prev->u.str->str,cur->u.str->str)>=0);
			else
				RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=getArg(stack));
	resBp(stack);
	SET_RETURN("SYS_ge",r
			?newTrueValue(exe->heap)
			:newNilValue(exe->heap)
			,stack);
}

void SYS_lt(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int r=1;
	VMvalue* cur=getArg(stack);
	VMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	for(;cur;cur=getArg(stack))
	{
		if(prev)
		{
			if(prev->type==DBL||cur->type==DBL)
				r=((((prev->type==DBL)?*prev->u.dbl:*prev->u.in32)-((cur->type==DBL)?*cur->u.dbl:*cur->u.in32))<0.0);
			else if(prev->type==IN32&&cur->type==IN32)
				r=(*prev->u.in32<*cur->u.in32);
			else if(prev->type==cur->type&&(prev->type==STR))
				r=(strcmp(prev->u.str->str,cur->u.str->str)<0);
			else
				RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=getArg(stack));
	resBp(stack);
	SET_RETURN("SYS_lt",r
			?newTrueValue(exe->heap)
			:newNilValue(exe->heap)
			,stack);
}

void SYS_le(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int r=1;
	VMvalue* cur=getArg(stack);
	VMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	for(;cur;cur=getArg(stack))
	{
		if(prev)
		{
			if(prev->type==DBL||cur->type==DBL)
				r=((((prev->type==DBL)?*prev->u.dbl:*prev->u.in32)-((cur->type==DBL)?*cur->u.dbl:*cur->u.in32))<=0.0);
			else if(prev->type==IN32&&cur->type==IN32)
				r=(*prev->u.in32<=*cur->u.in32);
			else if(prev->type==cur->type&&(prev->type==STR))
				r=(strcmp(prev->u.str->str,cur->u.str->str)<=0);
			else
				RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=getArg(stack));
	resBp(stack);
	SET_RETURN("SYS_le",r
			?newTrueValue(exe->heap)
			:newNilValue(exe->heap)
			,stack);
}

void SYS_chr(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	VMvalue* retval=newVMvalue(CHR,NULL,exe->heap,1);
	switch(obj->type)
	{
		case IN32:
			*retval->u.chr=*obj->u.in32;
			break;
		case DBL:
			*retval->u.chr=(int32_t)*obj->u.dbl;
			break;
		case CHR:
			*retval->u.chr=*obj->u.chr;
			break;
		case STR:
			*retval->u.chr=obj->u.str->str[0];
			break;
		case BYTS:
			*retval->u.chr=obj->u.byts->str[0];
			break;
		default:
			RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
			break;
	}
	SET_RETURN("SYS_chr",retval,stack);
}

void SYS_int(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	VMvalue* retval=newVMvalue(IN32,NULL,exe->heap,1);
	switch(obj->type)
	{
		case IN32:
			*retval->u.in32=*obj->u.in32;
			break;
		case DBL:
			*retval->u.in32=(int32_t)*obj->u.dbl;
			break;
		case CHR:
			*retval->u.in32=*obj->u.chr;
			break;
		case STR:
			*retval->u.in32=stringToInt(obj->u.str->str);
			break;
		case BYTS:
			*retval->u.in32=0;
			memcpy(retval->u.in32,obj->u.byts->str,obj->u.byts->size);
			break;
		default:
			RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
			break;
	}
	SET_RETURN("SYS_int",retval,stack);
}

void SYS_dbl(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	VMvalue* retval=newVMvalue(DBL,NULL,exe->heap,1);
	switch(obj->type)
	{
		case IN32:
			*retval->u.dbl=(double)*obj->u.in32;
			break;
		case DBL:
			*retval->u.dbl=*obj->u.dbl;
			break;
		case CHR:
			*retval->u.dbl=(double)(int32_t)*obj->u.chr;
			break;
		case STR:
			*retval->u.dbl=stringToDouble(obj->u.str->str);
			break;
		case BYTS:
			*retval->u.dbl=0.0;
			memcpy(retval->u.dbl,obj->u.byts->str,obj->u.byts->size);
			break;
		default:
			RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
			break;
	}
	SET_RETURN("SYS_dbl",retval,stack);
}

void SYS_str(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	VMvalue* retval=newVMvalue(STR,newVMstr(NULL),exe->heap,1);
	switch(obj->type)
	{
		case IN32:
			retval->u.str->str=intToString(*obj->u.in32);
			break;
		case DBL:
			retval->u.str->str=doubleToString(*obj->u.dbl);
			break;
		case CHR:
			retval->u.str->str=copyStr((char[]){*obj->u.chr,'\0'});
			break;
		case SYM:
			retval->u.str->str=copyStr(getGlobSymbolWithId(*obj->u.sid)->symbol);
			break;
		case STR:
			retval->u.str->str=copyStr(obj->u.str->str);
			break;
		case BYTS:
			retval->u.str->str=copyStr((char*)obj->u.byts->str);
			break;
		default:
			RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	}
	SET_RETURN("SYS_str",retval,stack);
}

void SYS_sym(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	VMvalue* retval=newVMvalue(SYM,NULL,exe->heap,1);
	switch(obj->type)
	{
		case CHR:
			*retval->u.sid=addSymbolToGlob(((char[]){*obj->u.chr,'\0'}))->id;
			break;
		case SYM:
			*retval->u.sid=*obj->u.sid;
			break;
		case STR:
			*retval->u.sid=addSymbolToGlob(obj->u.str->str)->id;
			break;
		default:
			RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	}
	SET_RETURN("SYS_sym",retval,stack);
}

void SYS_byts(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	VMvalue* retval=newVMvalue(BYTS,newEmptyVMByts(),exe->heap,1);
	switch(obj->type)
	{
		case IN32:
			retval->u.byts->size=sizeof(int32_t);
			retval->u.byts->str=createByteArry(sizeof(int32_t));
			*(int32_t*)retval->u.byts->str=*obj->u.in32;
			break;
		case DBL:
			retval->u.byts->size=sizeof(double);
			retval->u.byts->str=createByteArry(sizeof(double));
			*(double*)retval->u.byts->str=*obj->u.dbl;
			break;
		case CHR:
			retval->u.byts->size=sizeof(char);
			retval->u.byts->str=createByteArry(sizeof(char));
			*(char*)retval->u.byts->str=*obj->u.chr;
			break;
		case SYM:
			retval->u.byts->size=strlen(getGlobSymbolWithId(*obj->u.sid)->symbol);
			retval->u.byts->str=(uint8_t*)copyStr(getGlobSymbolWithId(*obj->u.sid)->symbol);
			break;
		case STR:
			retval->u.byts->size=strlen(obj->u.str->str)+1;
			retval->u.byts->str=(uint8_t*)copyStr(obj->u.str->str);
			break;
		case BYTS:
			retval->u.byts->size=obj->u.byts->size;
			retval->u.byts->str=createByteArry(obj->u.byts->size);
			memcpy(retval->u.byts->str,obj->u.byts->str,obj->u.byts->size);
			break;
		default:
			RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	}
	SET_RETURN("SYS_byts",retval,stack);
}

void SYS_type(FakeVM* exe,pthread_rwlock_t* gclock)
{
	static int32_t a[15];
	static int hasInit=0;
	if(!hasInit)
	{
		hasInit=1;
		char* b[]=
		{
			"nil",
			"int",
			"chr",
			"dbl",
			"sym",
			"str",
			"byts",
			"proc",
			"cont",
			"chan",
			"fp",
			"dll",
			"dlproc",
			"err",
			"pair"
		};
		int i=0;
		for(;i<ATM;i++)
			a[i]=addSymbolToGlob(b[i])->id;
	}
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	int32_t type=obj->type;
	SET_RETURN("SYS_type",newVMvalue(SYM,a+type,exe->heap,1),stack);
}

void SYS_nth(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* place=getArg(stack);
	VMvalue* objlist=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(place->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMvalue* retval=NULL;
	int32_t offset=*place->u.in32;
	switch(objlist->type)
	{
		case PAIR:
			{
				VMvalue* objPair=objlist;
				int i=0;
				for(;i<offset&&objPair->type==PAIR;i++,objPair=getVMpairCdr(objPair));
				retval=(objPair->type==PAIR)?getVMpairCar(objPair):newNilValue(exe->heap);
			}
			break;
		case STR:
			retval=newVMvalue(CHR,objlist->u.str->str+offset,exe->heap,0);
			break;
		case BYTS:
			retval=newVMvalue(BYTS,copyRefVMByts(1,objlist->u.byts->str+offset),exe->heap,0);
			break;
		default:
			RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	}
	SET_RETURN("SYS_nth",retval,stack);
}

void SYS_length(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	int32_t len=0;
	VMvalue* retval=newVMvalue(IN32,NULL,exe->heap,1);
	switch(obj->type)
	{
		case NIL:
			break;
		case PAIR:
			for(;obj->type==PAIR;obj=getVMpairCdr(obj))len++;
			break;
		case STR:
			len=strlen(obj->u.str->str);
			break;
		case BYTS:
			len=obj->u.byts->size;
			break;
		case CHAN:
			len=getNumVMChanl(obj->u.chan);
			break;
		default:
			RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	}
	*retval->u.in32=len;
	SET_RETURN("SYS_length",retval,stack);
}

void SYS_file(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* filename=getArg(stack);
	VMvalue* mode=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!mode||!filename)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(filename->type!=STR||mode->type!=STR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* file=fopen(filename->u.str->str,mode->u.str->str);
	VMvalue* obj=NULL;
	if(!file)
	{
		SET_RETURN("SYS_file",filename,stack);
		RAISE_BUILTIN_ERROR(FILEFAILURE,runnable,exe);
	}
	else
		obj=newVMvalue(FP,newVMfp(file),heap,1);
	SET_RETURN("SYS_file",obj,stack);
}

void SYS_read(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* tmpFile=file->u.fp->fp;
	int unexpectEOF=0;
	char* prev=NULL;
	char* tmpString=readInPattern(tmpFile,&prev,&unexpectEOF);
	if(prev)
		free(prev);
	if(unexpectEOF)
	{
		free(tmpString);
		RAISE_BUILTIN_ERROR(UNEXPECTEOF,runnable,exe);
	}
	Intpr* tmpIntpr=newTmpIntpr(NULL,tmpFile);
	AST_cptr* tmpCptr=baseCreateTree(tmpString,tmpIntpr);
	VMvalue* tmp=NULL;
	if(tmpCptr==NULL)
		tmp=newNilValue(exe->heap);
	else
		tmp=castCptrVMvalue(tmpCptr,exe->heap);
	SET_RETURN("SYS_read",tmp,stack);
	free(tmpIntpr);
	free(tmpString);
	deleteCptr(tmpCptr);
	free(tmpCptr);
}

void SYS_getb(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* size=getArg(stack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file||!size)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP||size->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* fp=file->u.fp->fp;
	uint8_t* str=(uint8_t*)malloc(sizeof(uint8_t)*(*size->u.in32));
	FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
	int32_t realRead=0;
	realRead=fread(str,sizeof(uint8_t),*size->u.in32,fp);
	if(!realRead)
	{
		free(str);
		SET_RETURN("SYS_getb",newNilValue(exe->heap),stack);
	}
	else
	{
		str=(uint8_t*)realloc(str,sizeof(uint8_t)*realRead);
		FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
		VMvalue* tmpByts=newVMvalue(BYTS,NULL,exe->heap,1);
		tmpByts->u.byts=newEmptyVMByts();
		tmpByts->u.byts->size=*size->u.in32;
		tmpByts->u.byts->str=str;
		SET_RETURN("SYS_getb",tmpByts,stack);
	}
}

void SYS_write(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file||!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	CRL* head=NULL;
	writeVMvalue(obj,objFile,&head);
	SET_RETURN("SYS_write",obj,stack);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
	}
}

void SYS_putb(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* bt=getArg(stack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file||!bt)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP||bt->type!=BYTS)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	fwrite(bt->u.byts->str,sizeof(uint8_t),bt->u.byts->size,objFile);
	SET_RETURN("SYS_putb",bt,stack);
}

void SYS_princ(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=getArg(stack);
	VMvalue* file=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!file||!obj)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(file->type!=FP)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	CRL* head=NULL;
	princVMvalue(obj,objFile,&head);
	while(head)
	{
		CRL* prev=head;
		head=head->next;
		free(prev);
	}
	SET_RETURN("SYS_princ",obj,stack);
}

void SYS_dll(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* dllName=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!dllName)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(dllName->type!=STR&&dllName->type!=SYM)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	VMDll* dll=newVMDll(dllName->u.str->str);
	if(!dll)
		RAISE_BUILTIN_ERROR(LOADDLLFAILD,runnable,exe);
	SET_RETURN("SYS_dll",newVMvalue(DLL,dll,exe->heap,1),stack);
}

void SYS_dlsym(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* dll=getArg(stack);
	VMvalue* symbol=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!dll||!symbol)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if((symbol->type!=SYM&&symbol->type!=STR)||dll->type!=DLL)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	char prefix[]="FAKE_";
	size_t len=strlen(prefix)+strlen(symbol->u.str->str)+1;
	char* realDlFuncName=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(realDlFuncName,"B_dlsym",__FILE__,__LINE__);
	sprintf(realDlFuncName,"%s%s",prefix,symbol->u.str->str);
	DllFunc funcAddress=getAddress(realDlFuncName,dll->u.dll->handle);
	if(!funcAddress)
	{
		free(realDlFuncName);
		RAISE_BUILTIN_ERROR(INVALIDSYMBOL,runnable,exe);
	}
	free(realDlFuncName);
	VMDlproc* dlproc=newVMDlproc(funcAddress,dll->u.dll);
	SET_RETURN("SYS_dlsym",newVMvalue(DLPROC,dlproc,heap,1),stack);
}

void SYS_argv(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* retval=NULL;
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,topComStack(exe->rstack),exe);
	retval=newNilValue(exe->heap);
	VMvalue* tmp=retval;
	int32_t i=0;
	for(;i<exe->argc;i++,tmp=getVMpairCdr(tmp))
	{
		VMvalue* cur=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
		copyRef(tmp,cur);
		cur->u.pair->car=newVMvalue(STR,newVMstr(exe->argv[i]),exe->heap,1);
	}
	SET_RETURN("FAKE_argv",retval,stack);
}

void SYS_go(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(exe->VMid==-1)
		RAISE_BUILTIN_ERROR(CANTCREATETHREAD,runnable,exe);
	VMvalue* threadProc=getArg(stack);
	if(!threadProc)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(threadProc->type!=PRC&&threadProc->type!=DLPROC)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	FakeVM* threadVM=(threadProc->type==PRC)?newThreadVM(threadProc->u.prc,exe->heap):newThreadDlprocVM(runnable,exe->heap);
	threadVM->lnt=exe->lnt;
	threadVM->code=exe->code;
	threadVM->size=exe->size;
	VMstack* threadVMstack=threadVM->stack;
	VMvalue* prevBp=newVMvalue(IN32,&threadVMstack->bp,exe->heap,1);
	SET_RETURN("SYS_go",prevBp,threadVMstack);
	threadVMstack->bp=threadVMstack->tp;
	VMvalue* cur=getArg(stack);
	ComStack* comStack=newComStack(32);
	for(;cur;cur=getArg(stack))
	{
		VMvalue* tmp=newNilValue(exe->heap);
		copyRef(tmp,cur);
		pushComStack(tmp,comStack);
	}
	resBp(stack);
	while(!isComStackEmpty(comStack))
	{
		VMvalue* tmp=popComStack(comStack);
		SET_RETURN("SYS_go",tmp,threadVMstack);
	}
	freeComStack(comStack);
	increaseVMChanlRefcount(threadVM->chan);
	VMChanl* chan=threadVM->chan;
	int32_t faildCode=0;
	if(threadProc->type==PRC)
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMFunc,threadVM);
	else
	{
		void* a[2]={threadVM,threadProc->u.dlproc->func};
		void** p=(void**)copyMemory(a,sizeof(a));
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMDlprocFunc,p);
	}
	if(faildCode)
	{
		decreaseVMChanlRefcount(chan);
		freeVMChanl(chan);
		deleteCallChain(threadVM);
		freeVMstack(threadVM->stack);
		threadVM->stack=NULL;
		threadVM->lnt=NULL;
		SET_RETURN("SYS_go",newVMvalue(IN32,&faildCode,exe->heap,1),stack);
	}
	else
		SET_RETURN("SYS_go",newVMvalue(CHAN,chan,exe->heap,1),stack);
}

void SYS_chanl(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* maxSize=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!maxSize)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(maxSize->type!=IN32)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	SET_RETURN("SYS_chanl",newVMvalue(CHAN,newVMChanl(*maxSize->u.in32),exe->heap,1),stack);
}

void SYS_send(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* message=getArg(stack);
	VMvalue* ch=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!message||!ch)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(ch->type!=CHAN)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	SendT* t=newSendT(message);
	chanlSend(t,ch->u.chan);
	SET_RETURN("SYS_send",message,stack);
}

void SYS_recv(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* ch=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!ch)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(ch->type!=CHAN)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	RecvT* t=newRecvT(exe);
	chanlRecv(t,ch->u.chan);
}

void SYS_error(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* type=getArg(stack);
	VMvalue* message=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!type||!message)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(type->type!=SYM||message->type!=STR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	SET_RETURN("SYS_error",newVMvalue(ERR,newVMerrorWithSid(*type->u.sid,message->u.str->str),exe->heap,1),stack);
}

void SYS_raise(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* err=getArg(stack);
	if(resBp(stack))
		RAISE_BUILTIN_ERROR(TOOMANYARG,runnable,exe);
	if(!err)
		RAISE_BUILTIN_ERROR(TOOFEWARG,runnable,exe);
	if(err->type!=ERR)
		RAISE_BUILTIN_ERROR(WRONGARG,runnable,exe);
	increaseVMerrorRefcount(err->u.err);
	raiseVMerror(err->u.err,exe);
}
