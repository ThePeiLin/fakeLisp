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
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.car",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.car",TOOFEWARG,runnable,exe);
	if(!IS_PAIR(obj))
		RAISE_BUILTIN_ERROR("sys.car",WRONGARG,runnable,exe);
	SET_RETURN("SYS_car",MAKE_VM_REF(&obj->u.pair->car),stack);
}

void SYS_cdr(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.cdr",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.cdr",TOOFEWARG,runnable,exe);
	if(!IS_PAIR(obj))
		RAISE_BUILTIN_ERROR("sys.cdr",WRONGARG,runnable,exe);
	SET_RETURN("SYS_cdr",MAKE_VM_REF(&obj->u.pair->cdr),stack);
}

void SYS_cons(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* car=GET_VAL(popVMstack(stack));
	VMvalue* cdr=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.cons",TOOMANYARG,runnable,exe);
	if(!car||!cdr)
		RAISE_BUILTIN_ERROR("sys.cons",TOOFEWARG,runnable,exe);
	VMvalue* pair=newVMvalue(PAIR,newVMpair(),exe->heap);
	pair->u.pair->car=car;
	pair->u.pair->cdr=cdr;
	SET_RETURN("SYS_cons",pair,stack);
}

void SYS_append(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* retval=VM_NIL;
	VMvalue* cur=GET_VAL(popVMstack(stack));
	VMvalue** prev=&retval;
	for(;cur;cur=GET_VAL(popVMstack(stack)))
	{
		if(*prev==VM_NIL)
			*prev=copyVMvalue(cur,exe->heap);
		else if(IS_PAIR(*prev))
		{
			for(;IS_PAIR(*prev);prev=&(*prev)->u.pair->cdr);
			*prev=copyVMvalue(cur,exe->heap);
		}
		else if(IS_STR(*prev))
		{
			if(!IS_PTR(cur))
				RAISE_BUILTIN_ERROR("sys.append",WRONGARG,runnable,exe);
			(*prev)->u.str=strCat((*prev)->u.str,cur->u.str);
		}
		else if(IS_BYTS(*prev))
		{
			if(!IS_BYTS(*prev))
				RAISE_BUILTIN_ERROR("sys.append",WRONGARG,runnable,exe);
			VMBytsCat(&(*prev)->u.byts,cur->u.byts);
		}
		else
			RAISE_BUILTIN_ERROR("sys.append",WRONGARG,runnable,exe);
	}
	resBp(stack);
	SET_RETURN("SYS_append",retval,stack);
}

void SYS_atom(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* arg=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.atom",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.atom",TOOFEWARG,runnable,exe);
	if(!IS_PAIR(arg))
		SET_RETURN("SYS_atom",VM_TRUE,stack);
	else
		SET_RETURN("SYS_atom",VM_NIL,stack);
}

void SYS_null(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* arg=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.null",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.null",TOOFEWARG,runnable,exe);
	if(arg==VM_NIL)
		SET_RETURN("SYS_null",VM_TRUE,stack);
	else
		SET_RETURN("SYS_null",VM_NIL,stack);
}

void SYS_not(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* arg=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.not",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.not",TOOFEWARG,runnable,exe);
	if(arg==VM_NIL)
		SET_RETURN("SYS_not",VM_TRUE,stack);
	else
		SET_RETURN("SYS_not",VM_NIL,stack);
}

void SYS_eq(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* fir=GET_VAL(popVMstack(stack));
	VMvalue* sec=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.eq",TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR("sys.eq",TOOFEWARG,runnable,exe);
	SET_RETURN("SYS_eq",fir==sec
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_eqn(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* fir=GET_VAL(popVMstack(stack));
	VMvalue* sec=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.eqn",TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR("sys.eqn",TOOFEWARG,runnable,exe);
	if((IS_DBL(fir)||IS_IN32(fir))&&(IS_DBL(sec)||IS_IN32(sec)))
		SET_RETURN("SYS_eqn",((((IS_DBL(fir))?*fir->u.dbl:GET_IN32(fir))-((IS_DBL(sec))?*sec->u.dbl:GET_IN32(sec)))==0.0)
				?VM_TRUE
				:VM_NIL
				,stack);
	else if(IS_SYM(fir)&&IS_SYM(sec))
		SET_RETURN("SYS_eqn",fir==sec
				?VM_TRUE
				:VM_NIL
				,stack);
	else if(IS_STR(fir)&&IS_STR(sec))
		SET_RETURN("SYS_eqn",(!strcmp(fir->u.str,sec->u.str))
				?VM_TRUE
				:VM_NIL
				,stack);
	else if(IS_BYTS(fir)&&IS_BYTS(sec))
		SET_RETURN("SYS_eqn",eqVMByts(fir->u.byts,sec->u.byts)
				?VM_TRUE
				:VM_NIL
				,stack);
	else
		RAISE_BUILTIN_ERROR("sys.eqn",WRONGARG,runnable,exe);

}

void SYS_equal(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* fir=GET_VAL(popVMstack(stack));
	VMvalue* sec=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.equal",TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR("sys.equal",TOOFEWARG,runnable,exe);
	SET_RETURN("SYS_equal",(VMvaluecmp(fir,sec))
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_add(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* cur=GET_VAL(popVMstack(stack));
	int32_t ri=0;
	double rd=0.0;
	for(;cur;cur=GET_VAL(popVMstack(stack)))
	{
		if(IS_IN32(cur))
			ri+=GET_IN32(cur);
		else if(IS_DBL(cur))
			rd+=*cur->u.dbl;
		else
			RAISE_BUILTIN_ERROR("sys.add",WRONGARG,runnable,exe);
	}
	resBp(stack);
	if(rd!=0.0)
	{
		rd+=ri;
		SET_RETURN("SYS_add",newVMvalue(DBL,&rd,exe->heap),stack);
	}
	else
		SET_RETURN("SYS_add",MAKE_VM_IN32(ri),stack);
}

void SYS_add_1(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* arg=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.add_1",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.add_1",TOOFEWARG,runnable,exe);
	if(!IS_DBL(arg)&&!IS_IN32(arg))
		RAISE_BUILTIN_ERROR("sys.add_1",WRONGARG,runnable,exe);
	if(IS_DBL(arg))
	{
		double r=*arg->u.dbl+1.0;
		SET_RETURN("SYS_add_1",newVMvalue(DBL,&r,exe->heap),stack);
	}
	else
		SET_RETURN("SYS_add_1",MAKE_VM_IN32(GET_IN32(arg)+1),stack);
}

void SYS_sub(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* prev=GET_VAL(popVMstack(stack));
	VMvalue* cur=GET_VAL(popVMstack(stack));
	int32_t ri=0;
	double rd=0.0;
	if(!prev)
		RAISE_BUILTIN_ERROR("sys.sub",TOOFEWARG,runnable,exe);
	if(!IS_DBL(prev)&&!IS_IN32(prev))
		RAISE_BUILTIN_ERROR("sys.sub",WRONGARG,runnable,exe);
	if(!cur)
	{
		resBp(stack);
		if(IS_DBL(prev))
		{
			rd=-(*prev->u.dbl);
			SET_RETURN("SYS_sub",newVMvalue(DBL,&rd,exe->heap),stack);
		}
		else
		{
			ri=-GET_IN32(prev);
			SET_RETURN("SYS_sub",MAKE_VM_IN32(ri),stack);
		}
	}
	else
	{
		for(;cur;cur=GET_VAL(popVMstack(stack)))
		{
			if(IS_IN32(cur))
				ri+=GET_IN32(cur);
			else if(IS_DBL(cur))
				rd+=*cur->u.dbl;
			else
				RAISE_BUILTIN_ERROR("sys.sub",WRONGARG,runnable,exe);
		}
		resBp(stack);
		if(IS_DBL(prev)||rd!=0.0)
		{
			rd=((IS_DBL(prev))?*prev->u.dbl:GET_IN32(prev))-rd-ri;
			SET_RETURN("SYS_sub",newVMvalue(DBL,&rd,exe->heap),stack);
		}
		else
		{
			ri=GET_IN32(prev)-ri;
			SET_RETURN("SYS_sub",MAKE_VM_IN32(ri),stack);
		}
	}
}

void SYS_sub_1(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* arg=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.sub_1",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.add_1",TOOFEWARG,runnable,exe);
	if(!IS_DBL(arg)&&!IS_IN32(arg))
		RAISE_BUILTIN_ERROR("sys.add_1",WRONGARG,runnable,exe);
	if(IS_DBL(arg))
	{
		double r=*arg->u.dbl-1.0;
		SET_RETURN("SYS_sub_1",newVMvalue(DBL,&r,exe->heap),stack);
	}
	else
		SET_RETURN("SYS_sub_1",MAKE_VM_IN32(GET_IN32(arg)-1),stack);
}

void SYS_mul(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* cur=GET_VAL(popVMstack(stack));
	int32_t ri=1;
	double rd=1.0;
	for(;cur;cur=GET_VAL(popVMstack(stack)))
	{
		if(IS_IN32(cur))
			ri*=GET_IN32(cur);
		else if(IS_DBL(cur))
			rd*=*cur->u.dbl;
		else
			RAISE_BUILTIN_ERROR("sys.mul",WRONGARG,runnable,exe);
	}
	resBp(stack);
	if(rd!=1.0)
	{
		rd*=ri;
		SET_RETURN("SYS_mul",newVMvalue(DBL,&rd,exe->heap),stack);
	}
	else
		SET_RETURN("SYS_mul",MAKE_VM_IN32(ri),stack);
}

void SYS_div(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* prev=GET_VAL(popVMstack(stack));
	VMvalue* cur=GET_VAL(popVMstack(stack));
	int32_t ri=1;
	double rd=1.0;
	if(!prev)
		RAISE_BUILTIN_ERROR("sys.div",TOOFEWARG,runnable,exe);
	if(!IS_DBL(prev)&&!IS_IN32(prev))
		RAISE_BUILTIN_ERROR("sys.div",WRONGARG,runnable,exe);
	if(!cur)
	{
		resBp(stack);
		if(IS_DBL(prev))
		{
			if(*prev->u.dbl==0.0)
				RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
			rd=1/(*prev->u.dbl);
			SET_RETURN("SYS_sub",newVMvalue(DBL,&rd,exe->heap),stack);
		}
		else
		{
			if(!GET_IN32(prev))
				RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
			if(1%GET_IN32(prev))
			{
				rd=1.0/GET_IN32(prev);
				SET_RETURN("SYS_sub",newVMvalue(DBL,&rd,exe->heap),stack);
			}
			else
			{
				ri=1/GET_IN32(prev);
				SET_RETURN("SYS_sub",MAKE_VM_IN32(ri),stack);
			}
		}
	}
	else
	{
		for(;cur;cur=GET_VAL(popVMstack(stack)))
		{
			if(IS_IN32(cur))
				ri*=GET_IN32(cur);
			else if(IS_DBL(cur))
				rd*=*cur->u.dbl;
			else
				RAISE_BUILTIN_ERROR("sys.div",WRONGARG,runnable,exe);
		}
		if(ri==0)
			RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
		resBp(stack);
		if(IS_DBL(prev)||rd!=1.0||(IS_IN32(prev)&&GET_IN32(prev)%ri))
		{
			if(rd==0.0||ri==0)
				RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
			rd=((double)((IS_DBL(prev))?*prev->u.dbl:GET_IN32(prev)))/rd/ri;
			SET_RETURN("SYS_div",newVMvalue(DBL,&rd,exe->heap),stack);
		}
		else
		{
			if(ri==0)
				RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
			ri=GET_IN32(prev)/ri;
			SET_RETURN("SYS_div",MAKE_VM_IN32(ri),stack);
		}
	}
}

void SYS_rem(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* fir=GET_VAL(popVMstack(stack));
	VMvalue* sec=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.rem",TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR("sys.rem",TOOFEWARG,runnable,exe);
	if(IS_DBL(fir)||IS_DBL(sec))
	{
		double af=(IS_DBL(fir))?*fir->u.dbl:GET_IN32(fir);
		double as=(IS_DBL(sec))?*sec->u.dbl:GET_IN32(sec);
		if(as==0.0)
			RAISE_BUILTIN_ERROR("sys.rem",DIVZERROERROR,runnable,exe);
		double r=fmod(af,as);
		SET_RETURN("SYS_rem",newVMvalue(DBL,&r,exe->heap),stack);
	}
	else if(IS_IN32(fir)&&IS_IN32(sec))
	{
		if(!GET_IN32(sec))
			RAISE_BUILTIN_ERROR("sys.rem",DIVZERROERROR,runnable,exe);
		int32_t r=GET_IN32(fir)%GET_IN32(sec);
		SET_RETURN("SYS_rem",MAKE_VM_IN32(r),stack);
	}
	else
		RAISE_BUILTIN_ERROR("sys.rem",WRONGARG,runnable,exe);
}

void SYS_gt(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int r=1;
	VMvalue* cur=GET_VAL(popVMstack(stack));
	VMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR("sys.gt",TOOFEWARG,runnable,exe);
	for(;cur;cur=GET_VAL(popVMstack(stack)))
	{
		if(prev)
		{
			if((IS_DBL(prev)||IS_IN32(prev))&&(IS_DBL(cur)||IS_IN32(cur)))
				r=((((IS_DBL(prev))?*prev->u.dbl:GET_IN32(prev))-((IS_DBL(cur))?*cur->u.dbl:GET_IN32(cur)))>0.0);
			else if(IS_STR(prev)&&IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)>0);
			else
				RAISE_BUILTIN_ERROR("sys.gt",WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=GET_VAL(popVMstack(stack)));
	resBp(stack);
	SET_RETURN("SYS_gt",r
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_ge(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int r=1;
	VMvalue* cur=GET_VAL(popVMstack(stack));
	VMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR("sys.ge",TOOFEWARG,runnable,exe);
	for(;cur;cur=GET_VAL(popVMstack(stack)))
	{
		if(prev)
		{
			if((IS_DBL(prev)||IS_IN32(prev))&&(IS_DBL(cur)||IS_IN32(cur)))
				r=((((IS_DBL(prev))?*prev->u.dbl:GET_IN32(prev))-((IS_DBL(cur))?*cur->u.dbl:GET_IN32(cur)))>=0.0);
			else if(IS_STR(prev)&&IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)>=0);
			else
				RAISE_BUILTIN_ERROR("sys.ge",WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=GET_VAL(popVMstack(stack)));
	resBp(stack);
	SET_RETURN("SYS_ge",r
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_lt(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int r=1;
	VMvalue* cur=GET_VAL(popVMstack(stack));
	VMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR("sys.lt",TOOFEWARG,runnable,exe);
	for(;cur;cur=GET_VAL(popVMstack(stack)))
	{
		if(prev)
		{
			if((IS_DBL(prev)||IS_IN32(prev))&&(IS_DBL(cur)||IS_IN32(cur)))
				r=((((IS_DBL(prev))?*prev->u.dbl:GET_IN32(prev))-((IS_DBL(cur))?*cur->u.dbl:GET_IN32(cur)))<0.0);
			else if(IS_STR(prev)&&IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)<0);
			else
				RAISE_BUILTIN_ERROR("sys.lt",WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=GET_VAL(popVMstack(stack)));
	resBp(stack);
	SET_RETURN("SYS_lt",r
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_le(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int r=1;
	VMvalue* cur=GET_VAL(popVMstack(stack));
	VMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR("sys.le",TOOFEWARG,runnable,exe);
	for(;cur;cur=GET_VAL(popVMstack(stack)))
	{
		if(prev)
		{
			if((IS_DBL(prev)||IS_IN32(prev))&&(IS_DBL(cur)||IS_IN32(cur)))
				r=((((IS_DBL(prev))?*prev->u.dbl:GET_IN32(prev))-((IS_DBL(cur))?*cur->u.dbl:GET_IN32(cur)))<=0.0);
			else if(IS_STR(prev)&&IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)<=0);
			else
				RAISE_BUILTIN_ERROR("sys.le",WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=GET_VAL(popVMstack(stack)));
	resBp(stack);
	SET_RETURN("SYS_le",r
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_chr(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.chr",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.chr",TOOFEWARG,runnable,exe);
	if(IS_IN32(obj))
		SET_RETURN("SYS_chr",MAKE_VM_CHR(GET_IN32(obj)),stack);
	else if(IS_CHR(obj))
		SET_RETURN("SYS_chr",obj,stack);
	else if(IS_DBL(obj))
		SET_RETURN("SYS_chr",MAKE_VM_CHR((int32_t)*obj->u.dbl),stack);
	else if(IS_STR(obj))
		SET_RETURN("SYS_chr",MAKE_VM_CHR(obj->u.str[0]),stack);
	else if(IS_BYTS(obj))
		SET_RETURN("SYS_chr",MAKE_VM_CHR(obj->u.byts->str[0]),stack);
	else
		RAISE_BUILTIN_ERROR("sys.chr",WRONGARG,runnable,exe);
}

void SYS_int(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.int",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.int",TOOFEWARG,runnable,exe);
	if(IS_CHR(obj))
		SET_RETURN("SYS_int",MAKE_VM_IN32(GET_CHR(obj)),stack);
	else if(IS_IN32(obj))
		SET_RETURN("SYS_int",obj,stack);
	else if(IS_DBL(obj))
		SET_RETURN("SYS_int",MAKE_VM_IN32((int32_t)*obj->u.dbl),stack);
	else if(IS_STR(obj))
		SET_RETURN("SYS_int",MAKE_VM_IN32(stringToInt(obj->u.str)),stack);
	else if(IS_BYTS(obj))
	{
		uint8_t r[4]={0};
		size_t s=obj->u.byts->size;
		switch(s>4?4:s)
		{
			case 4:r[3]=obj->u.byts->str[3];
			case 3:r[2]=obj->u.byts->str[2];
			case 2:r[1]=obj->u.byts->str[1];
			case 1:r[0]=obj->u.byts->str[0];
		}
		SET_RETURN("SYS_int",MAKE_VM_IN32(*(int32_t*)r),stack);
	}
	else
		RAISE_BUILTIN_ERROR("sys.int",WRONGARG,runnable,exe);
}

void SYS_dbl(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.dbl",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.dbl",TOOFEWARG,runnable,exe);
	VMvalue* retval=newVMvalue(DBL,NULL,exe->heap);
	if(IS_CHR(obj))
		*retval->u.dbl=(double)GET_CHR(obj);
	else if(IS_IN32(obj))
		*retval->u.dbl=(double)GET_IN32(obj);
	else if(IS_DBL(obj))
		*retval->u.dbl=*obj->u.dbl;
	else if(IS_STR(obj))
		*retval->u.dbl=stringToDouble(obj->u.str);
	else if(IS_BYTS(obj))
	{
		uint8_t r[8]={0};
		size_t s=obj->u.byts->size;
		switch(s>8?8:s)
		{
			case 8:r[7]=obj->u.byts->str[7];
			case 7:r[6]=obj->u.byts->str[6];
			case 6:r[5]=obj->u.byts->str[5];
			case 5:r[4]=obj->u.byts->str[4];
			case 4:r[3]=obj->u.byts->str[3];
			case 3:r[2]=obj->u.byts->str[2];
			case 2:r[1]=obj->u.byts->str[1];
			case 1:r[0]=obj->u.byts->str[0];
		}
		memcpy(retval->u.dbl,r,8);
	}
	else
		RAISE_BUILTIN_ERROR("sys.dbl",WRONGARG,runnable,exe);
	SET_RETURN("SYS_dbl",retval,stack);
}

void SYS_str(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.str",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.str",TOOFEWARG,runnable,exe);
	VMvalue* retval=newVMvalue(STR,NULL,exe->heap);
	if(IS_IN32(obj))
		retval->u.str=intToString(GET_IN32(obj));
	else if(IS_DBL(obj))
		retval->u.str=doubleToString(*obj->u.dbl);
	else if(IS_CHR(obj))
		retval->u.str=copyStr((char[]){GET_CHR(obj),'\0'});
	else if(IS_SYM(obj))
		retval->u.str=copyStr(getGlobSymbolWithId(GET_SYM(obj))->symbol);
	else if(IS_STR(obj))
		retval->u.str=copyStr(obj->u.str);
	else if(IS_BYTS(obj))
	{
		size_t s=obj->u.byts->size;
		size_t l=(obj->u.byts->str[s-1])?s+1:s;
		char* t=(char*)malloc(sizeof(char)*l);
		FAKE_ASSERT(t,"SYS_str",__FILE__,__LINE__);
		memcpy(t,obj->u.byts->str,s);
		t[l-1]='\0';
		retval->u.str=t;
	}
	else
		RAISE_BUILTIN_ERROR("sys.str",WRONGARG,runnable,exe);
	SET_RETURN("SYS_str",retval,stack);
}

void SYS_sym(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.sym",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.sym",TOOFEWARG,runnable,exe);
	if(IS_CHR(obj))
		SET_RETURN("SYS_sym",MAKE_VM_SYM(addSymbolToGlob(((char[]){GET_CHR(obj),'\0'}))->id),stack);
	else if(IS_SYM(obj))
		SET_RETURN("SYS_sym",obj,stack);
	else if(IS_STR(obj))
		SET_RETURN("SYS_sym",MAKE_VM_SYM(addSymbolToGlob(obj->u.str)->id),stack);
	else
		RAISE_BUILTIN_ERROR("sys.sym",WRONGARG,runnable,exe);
}

void SYS_byts(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.byts",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.byts",TOOFEWARG,runnable,exe);
	VMvalue* retval=newVMvalue(BYTS,newEmptyVMByts(),exe->heap);
	if(IS_IN32(obj))
	{
		retval->u.byts=(VMByts*)realloc(retval->u.byts,sizeof(VMByts)+sizeof(int32_t));
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=sizeof(int32_t);
		*(int32_t*)retval->u.byts->str=GET_IN32(obj);
	}
	else if(IS_DBL(obj))
	{
		retval->u.byts=(VMByts*)realloc(retval->u.byts,sizeof(VMByts)+sizeof(double));
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=sizeof(double);
		*(double*)retval->u.byts->str=*obj->u.dbl;
	}
	else if(IS_CHR(obj))
	{
		retval->u.byts=(VMByts*)realloc(retval->u.byts,sizeof(VMByts)+sizeof(char));
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=sizeof(char);
		*(char*)retval->u.byts->str=GET_CHR(obj);
	}
	else if(IS_SYM(obj))
	{
		SymTabNode* n=getGlobSymbolWithId(GET_SYM(obj));
		retval->u.byts=(VMByts*)realloc(retval->u.byts,sizeof(VMByts)+strlen(n->symbol));
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=strlen(n->symbol);
		memcpy(retval->u.byts->str,n->symbol,retval->u.byts->size);
	}
	else if(IS_STR(obj))
	{
		retval->u.byts=(VMByts*)realloc(retval->u.byts,sizeof(VMByts)+strlen(obj->u.str)+1);
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=strlen(obj->u.str);
		memcpy(retval->u.byts->str,obj->u.str,retval->u.byts->size);
	}
	else if(IS_BYTS(obj))
		VMBytsCat(&retval->u.byts,obj->u.byts);
	else
		RAISE_BUILTIN_ERROR("sys.byts",WRONGARG,runnable,exe);
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
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.type",TOOMANYARG,runnable,exe);
	size_t type=0;
	switch(GET_TAG(obj))
	{
		case NIL_TAG:type=NIL;break;
		case IN32_TAG:type=IN32;break;
		case SYM_TAG:type=SYM;break;
		case CHR_TAG:type=CHR;break;
		case PTR_TAG:type=obj->type;break;
		default:
			RAISE_BUILTIN_ERROR("sys.type",WRONGARG,runnable,exe);
			break;
	}
	SET_RETURN("SYS_type",MAKE_VM_SYM(a[type]),stack);
}

void SYS_nth(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* place=GET_VAL(popVMstack(stack));
	VMvalue* objlist=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.nth",TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		RAISE_BUILTIN_ERROR("sys.nth",TOOFEWARG,runnable,exe);
	if(!IS_IN32(place))
		RAISE_BUILTIN_ERROR("sys.nth",WRONGARG,runnable,exe);
	VMvalue* retval=NULL;
	int32_t offset=GET_IN32(place);
	if(IS_PAIR(objlist))
	{
		VMvalue* objPair=objlist;
		int i=0;
		for(;i<offset&&IS_PAIR(objPair);i++,objPair=getVMpairCdr(objPair));
		retval=(IS_PAIR(objPair))?MAKE_VM_REF(&objPair->u.pair->car):VM_NIL;
	}
	else if(IS_STR(objlist))
		retval=offset>=strlen(objlist->u.str)?VM_NIL:newVMChref(objlist,objlist->u.str+offset);
	else if(IS_BYTS(objlist))
		retval=offset>=objlist->u.byts->size?VM_NIL:newVMChref(objlist,(char*)objlist->u.byts->str+offset);
	else
		RAISE_BUILTIN_ERROR("sys.nth",WRONGARG,runnable,exe);
	SET_RETURN("SYS_nth",retval,stack);
}

void SYS_length(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.length",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.length",TOOFEWARG,runnable,exe);
	int32_t len=0;
	if(obj==VM_NIL||IS_PAIR(obj))
		for(;IS_PAIR(obj);obj=getVMpairCdr(obj))len++;
	else if(IS_STR(obj))
		len=strlen(obj->u.str);
	else if(IS_BYTS(obj))
		len=obj->u.byts->size;
	else if(IS_CHAN(obj))
		len=getNumVMChanl(obj->u.chan);
	else
		RAISE_BUILTIN_ERROR("sys.length",WRONGARG,runnable,exe);
	SET_RETURN("SYS_length",MAKE_VM_IN32(len),stack);
}

void SYS_file(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* filename=GET_VAL(popVMstack(stack));
	VMvalue* mode=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.file",TOOMANYARG,runnable,exe);
	if(!mode||!filename)
		RAISE_BUILTIN_ERROR("sys.file",TOOFEWARG,runnable,exe);
	if(!IS_STR(filename)||!IS_STR(mode))
		RAISE_BUILTIN_ERROR("sys.file",WRONGARG,runnable,exe);
	FILE* file=fopen(filename->u.str,mode->u.str);
	VMvalue* obj=NULL;
	if(!file)
	{
		SET_RETURN("SYS_file",filename,stack);
		RAISE_BUILTIN_ERROR("sys.file",FILEFAILURE,runnable,exe);
	}
	else
		obj=newVMvalue(FP,file,heap);
	SET_RETURN("SYS_file",obj,stack);
}

void SYS_read(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* stream=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.read",TOOMANYARG,runnable,exe);
	if(!stream)
		RAISE_BUILTIN_ERROR("sys.read",TOOFEWARG,runnable,exe);
	if(!IS_FP(stream)&&!IS_STR(stream))
		RAISE_BUILTIN_ERROR("sys.read",WRONGARG,runnable,exe);
	char* tmpString=NULL;
	FILE* tmpFile=NULL;
	if(IS_FP(stream))
	{
		tmpFile=stream->u.fp;
		int unexpectEOF=0;
		char* prev=NULL;
		tmpString=readInPattern(tmpFile,&prev,&unexpectEOF);
		if(prev)
			free(prev);
		if(unexpectEOF)
		{
			free(tmpString);
			RAISE_BUILTIN_ERROR("sys.read",UNEXPECTEOF,runnable,exe);
		}
	}
	else
		tmpString=copyStr(stream->u.str);
	Intpr* tmpIntpr=newTmpIntpr(NULL,tmpFile);
	AST_cptr* tmpCptr=baseCreateTree(tmpString,tmpIntpr);
	VMvalue* tmp=NULL;
	if(tmpCptr==NULL)
		tmp=VM_NIL;
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
	VMvalue* size=GET_VAL(popVMstack(stack));
	VMvalue* file=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.getb",TOOMANYARG,runnable,exe);
	if(!file||!size)
		RAISE_BUILTIN_ERROR("sys.getb",TOOFEWARG,runnable,exe);
	if(!IS_FP(file)||!IS_IN32(size))
		RAISE_BUILTIN_ERROR("sys.getb",WRONGARG,runnable,exe);
	FILE* fp=file->u.fp;
	uint8_t* str=(uint8_t*)malloc(sizeof(uint8_t)*GET_IN32(size));
	FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
	int32_t realRead=0;
	realRead=fread(str,sizeof(uint8_t),GET_IN32(size),fp);
	if(!realRead)
	{
		free(str);
		SET_RETURN("SYS_getb",VM_NIL,stack);
	}
	else
	{
		str=(uint8_t*)realloc(str,sizeof(uint8_t)*realRead);
		FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
		VMvalue* tmpByts=newVMvalue(BYTS,NULL,exe->heap);
		tmpByts->u.byts=(VMByts*)malloc(sizeof(VMByts)+GET_IN32(size));
		FAKE_ASSERT(tmpByts->u.byts,"SYS_getb",__FILE__,__LINE__);
		tmpByts->u.byts->size=GET_IN32(size);
		memcpy(tmpByts->u.byts->str,str,GET_IN32(size));
		free(str);
		SET_RETURN("SYS_getb",tmpByts,stack);
	}
}

void SYS_write(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	VMvalue* file=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.write",TOOMANYARG,runnable,exe);
	if(!file||!obj)
		RAISE_BUILTIN_ERROR("sys.write",TOOFEWARG,runnable,exe);
	if(!IS_FP(file))
		RAISE_BUILTIN_ERROR("sys.write",WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp;
	writeVMvalue(obj,objFile,NULL);
	SET_RETURN("SYS_write",obj,stack);
}

void SYS_putb(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* bt=GET_VAL(popVMstack(stack));
	VMvalue* file=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.putb",TOOMANYARG,runnable,exe);
	if(!file||!bt)
		RAISE_BUILTIN_ERROR("sys.putb",TOOFEWARG,runnable,exe);
	if(!IS_FP(file)||!IS_BYTS(bt))
		RAISE_BUILTIN_ERROR("sys.putb",WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp;
	fwrite(bt->u.byts->str,sizeof(uint8_t),bt->u.byts->size,objFile);
	SET_RETURN("SYS_putb",bt,stack);
}

void SYS_princ(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* obj=GET_VAL(popVMstack(stack));
	VMvalue* file=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.princ",TOOMANYARG,runnable,exe);
	if(!file||!obj)
		RAISE_BUILTIN_ERROR("sys.princ",TOOFEWARG,runnable,exe);
	if(!IS_FP(file))
		RAISE_BUILTIN_ERROR("sys.princ",WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp;
	princVMvalue(obj,objFile,NULL);
	SET_RETURN("SYS_princ",obj,stack);
}

void SYS_dll(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* dllName=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.dll",TOOMANYARG,runnable,exe);
	if(!dllName)
		RAISE_BUILTIN_ERROR("sys.dll",TOOFEWARG,runnable,exe);
	if(!IS_STR(dllName))
		RAISE_BUILTIN_ERROR("sys.dll",WRONGARG,runnable,exe);
	DllHandle* dll=newVMDll(dllName->u.str);
	if(!dll)
		RAISE_BUILTIN_ERROR("sys.dll",LOADDLLFAILD,runnable,exe);
	SET_RETURN("SYS_dll",newVMvalue(DLL,dll,exe->heap),stack);
}

void SYS_dlsym(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* dll=GET_VAL(popVMstack(stack));
	VMvalue* symbol=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.dlsym",TOOMANYARG,runnable,exe);
	if(!dll||!symbol)
		RAISE_BUILTIN_ERROR("sys.dlsym",TOOFEWARG,runnable,exe);
	if(!IS_STR(symbol)||!IS_DLL(dll))
		RAISE_BUILTIN_ERROR("sys.dlsym",WRONGARG,runnable,exe);
	char prefix[]="FAKE_";
	size_t len=strlen(prefix)+strlen(symbol->u.str)+1;
	char* realDlFuncName=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(realDlFuncName,"B_dlsym",__FILE__,__LINE__);
	sprintf(realDlFuncName,"%s%s",prefix,symbol->u.str);
	DllFunc funcAddress=getAddress(realDlFuncName,dll->u.dll);
	if(!funcAddress)
	{
		free(realDlFuncName);
		RAISE_BUILTIN_ERROR("sys.dlsym",INVALIDSYMBOL,runnable,exe);
	}
	free(realDlFuncName);
	VMDlproc* dlproc=newVMDlproc(funcAddress,dll);
	SET_RETURN("SYS_dlsym",newVMvalue(DLPROC,dlproc,heap),stack);
}

void SYS_argv(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* retval=NULL;
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.argv",TOOMANYARG,topComStack(exe->rstack),exe);
	retval=VM_NIL;
	VMvalue** tmp=&retval;
	int32_t i=0;
	for(;i<exe->argc;i++,tmp=&(*tmp)->u.pair->cdr)
	{
		VMvalue* cur=newVMvalue(PAIR,newVMpair(),exe->heap);
		*tmp=cur;
		cur->u.pair->car=newVMvalue(STR,copyStr(exe->argv[i]),exe->heap);
	}
	SET_RETURN("FAKE_argv",retval,stack);
}

void SYS_go(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(exe->VMid==-1)
		RAISE_BUILTIN_ERROR("sys.go",CANTCREATETHREAD,runnable,exe);
	VMvalue* threadProc=GET_VAL(popVMstack(stack));
	if(!threadProc)
		RAISE_BUILTIN_ERROR("sys.go",TOOFEWARG,runnable,exe);
	if(!IS_PRC(threadProc)&&!IS_DLPROC(threadProc)&&!IS_CONT(threadProc))
		RAISE_BUILTIN_ERROR("sys.go",WRONGARG,runnable,exe);
	FakeVM* threadVM=(IS_PRC(threadProc)||IS_CONT(threadProc))?newThreadVM(threadProc->u.prc,exe->heap):newThreadDlprocVM(runnable,exe->heap);
	threadVM->lnt=exe->lnt;
	threadVM->code=exe->code;
	threadVM->size=exe->size;
	VMstack* threadVMstack=threadVM->stack;
	VMvalue* prevBp=MAKE_VM_IN32(threadVMstack->bp);
	SET_RETURN("SYS_go",prevBp,threadVMstack);
	threadVMstack->bp=threadVMstack->tp;
	VMvalue* cur=GET_VAL(popVMstack(stack));
	ComStack* comStack=newComStack(32);
	for(;cur;cur=GET_VAL(popVMstack(stack)))
		pushComStack(cur,comStack);
	resBp(stack);
	while(!isComStackEmpty(comStack))
	{
		VMvalue* tmp=popComStack(comStack);
		SET_RETURN("SYS_go",tmp,threadVMstack);
	}
	freeComStack(comStack);
	VMvalue* chan=threadVM->chan;
	int32_t faildCode=0;
	if(IS_PRC(threadProc))
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMFunc,threadVM);
	else if(IS_CONT(threadProc))
	{
		createCallChainWithContinuation(threadVM,threadProc->u.cont);
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMFunc,threadVM);
	}
	else
	{
		void* a[2]={threadVM,threadProc->u.dlproc->func};
		void** p=(void**)copyMemory(a,sizeof(a));
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMDlprocFunc,p);
	}
	if(faildCode)
	{
		deleteCallChain(threadVM);
		freeVMstack(threadVM->stack);
		threadVM->stack=NULL;
		threadVM->lnt=NULL;
		SET_RETURN("SYS_go",MAKE_VM_IN32(faildCode),stack);
	}
	else
		SET_RETURN("SYS_go",chan,stack);
}

void SYS_chanl(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* maxSize=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.chanl",TOOMANYARG,runnable,exe);
	if(!maxSize)
		RAISE_BUILTIN_ERROR("sys.chanl",TOOFEWARG,runnable,exe);
	if(!IS_IN32(maxSize))
		RAISE_BUILTIN_ERROR("sys.chanl",WRONGARG,runnable,exe);
	SET_RETURN("SYS_chanl",newVMvalue(CHAN,newVMChanl(GET_IN32(maxSize)),exe->heap),stack);
}

void SYS_send(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* message=GET_VAL(popVMstack(stack));
	VMvalue* ch=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.send",TOOMANYARG,runnable,exe);
	if(!message||!ch)
		RAISE_BUILTIN_ERROR("sys.send",TOOFEWARG,runnable,exe);
	if(!IS_CHAN(ch))
		RAISE_BUILTIN_ERROR("sys.send",WRONGARG,runnable,exe);
	SendT* t=newSendT(message);
	chanlSend(t,ch->u.chan);
	SET_RETURN("SYS_send",message,stack);
}

void SYS_recv(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* ch=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.recv",TOOMANYARG,runnable,exe);
	if(!ch)
		RAISE_BUILTIN_ERROR("sys.recv",TOOFEWARG,runnable,exe);
	if(!IS_CHAN(ch))
		RAISE_BUILTIN_ERROR("sys.recv",WRONGARG,runnable,exe);
	RecvT* t=newRecvT(exe);
	chanlRecv(t,ch->u.chan);
}

void SYS_error(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* who=GET_VAL(popVMstack(stack));
	VMvalue* type=GET_VAL(popVMstack(stack));
	VMvalue* message=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.error",TOOMANYARG,runnable,exe);
	if(!type||!message)
		RAISE_BUILTIN_ERROR("sys.error",TOOFEWARG,runnable,exe);
	if(!IS_SYM(type)||!IS_STR(message)||(!IS_SYM(who)&&!IS_STR(who)))
		RAISE_BUILTIN_ERROR("sys.error",WRONGARG,runnable,exe);
	SET_RETURN("SYS_error",newVMvalue(ERR,newVMerrorWithSid((IS_SYM(who))?getGlobSymbolWithId(GET_SYM(who))->symbol:who->u.str,GET_SYM(type),message->u.str),exe->heap),stack);
}

void SYS_raise(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* err=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.raise",TOOMANYARG,runnable,exe);
	if(!err)
		RAISE_BUILTIN_ERROR("sys.raise",TOOFEWARG,runnable,exe);
	if(!IS_ERR(err))
		RAISE_BUILTIN_ERROR("sys.raise",WRONGARG,runnable,exe);
	raiseVMerror(err,exe);
}

void SYS_clcc(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* proc=GET_VAL(popVMstack(stack));
	if(resBp(stack))
		RAISE_BUILTIN_ERROR("sys.clcc",TOOMANYARG,runnable,exe);
	if(!proc)
		RAISE_BUILTIN_ERROR("sys.clcc",TOOFEWARG,runnable,exe);
	if(!IS_PTR(proc)||(proc->type!=PRC&&proc->type!=CONT&&proc->type!=DLPROC))
		RAISE_BUILTIN_ERROR("sys.clcc",INVOKEERROR,runnable,exe);
	VMvalue* cc=newVMvalue(CONT,newVMcontinuation(stack,exe->rstack,exe->tstack),exe->heap);
	SET_RETURN("SYS_clcc",MAKE_VM_IN32(stack->bp),stack);
	stack->bp=stack->tp;
	SET_RETURN("SYS_clcc",cc,stack);
	if(proc->type==PRC)
	{
		VMproc* tmpProc=proc->u.prc;
		VMrunnable* prevProc=hasSameProc(tmpProc->scp,exe->rstack);
		if(isTheLastExpress(runnable,prevProc,exe)&&prevProc)
			prevProc->mark=1;
		else
		{
			VMrunnable* tmpRunnable=newVMrunnable(tmpProc);
			tmpRunnable->localenv=newVMenv(tmpProc->prevEnv);
			pushComStack(tmpRunnable,exe->rstack);
		}
	}
	else if(proc->type==CONT)
	{
		VMcontinuation* cc=proc->u.cont;
		createCallChainWithContinuation(exe,cc);
	}
	else
	{
		DllFunc dllfunc=proc->u.dlproc->func;
		dllfunc(exe,gclock);
	}
}

void SYS_apply(FakeVM* exe,pthread_rwlock_t* gclock)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* proc=GET_VAL(popVMstack(stack));
	if(!proc)
		RAISE_BUILTIN_ERROR("sys.apply",TOOFEWARG,runnable,exe);
	if(!IS_PTR(proc)||(proc->type!=PRC&&proc->type!=CONT&&proc->type!=DLPROC))
		RAISE_BUILTIN_ERROR("b.invoke",INVOKEERROR,runnable,exe);
	ComStack* stack1=newComStack(32);
	VMvalue* value=NULL;
	VMvalue* lastList=NULL;
	while((value=GET_VAL(popVMstack(stack))))
	{
		if(!resBp(stack))
		{
			lastList=value;
			break;
		}
		pushComStack(value,stack1);
	}
	ComStack* stack2=newComStack(32);
	if(!IS_PAIR(lastList)&&lastList!=VM_NIL)
	{
		freeComStack(stack1);
		freeComStack(stack2);
		RAISE_BUILTIN_ERROR("sys.apply",WRONGARG,runnable,exe);
	}
	for(;IS_PAIR(lastList);lastList=getVMpairCdr(lastList))
		pushComStack(getVMpairCar(lastList),stack2);
	if(lastList!=VM_NIL)
	{
		freeComStack(stack1);
		freeComStack(stack2);
		RAISE_BUILTIN_ERROR("sys.apply",WRONGARG,runnable,exe);
	}
	SET_RETURN("SYS_apply",MAKE_VM_IN32(stack->bp),stack);
	stack->bp=stack->tp;
	while(!isComStackEmpty(stack2))
	{
		VMvalue* t=popComStack(stack2);
		SET_RETURN("SYS_apply",t,stack);
	}
	freeComStack(stack2);
	while(!isComStackEmpty(stack1))
	{
		VMvalue* t=popComStack(stack1);
		SET_RETURN("SYS_apply",t,stack);
	}
	freeComStack(stack1);
	if(proc->type==PRC)
	{
		VMproc* tmpProc=proc->u.prc;
		VMrunnable* prevProc=hasSameProc(tmpProc->scp,exe->rstack);
		if(isTheLastExpress(runnable,prevProc,exe)&&prevProc)
			prevProc->mark=1;
		else
		{
			VMrunnable* tmpRunnable=newVMrunnable(tmpProc);
			tmpRunnable->localenv=newVMenv(tmpProc->prevEnv);
			pushComStack(tmpRunnable,exe->rstack);
		}
	}
	else if(proc->type==CONT)
	{
		VMcontinuation* cc=proc->u.cont;
		createCallChainWithContinuation(exe,cc);
	}
	else
	{
		DllFunc dllfunc=proc->u.dlproc->func;
		dllfunc(exe,gclock);
	}
}
