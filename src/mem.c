#include<fakeLisp/mem.h>
#include<fakeLisp/basicADT.h>
#include<fakeLisp/utils.h>
FklMem* fklNewMem(void* block,void (*destructor)(void*))
{
	FklMem* tmp=(FklMem*)malloc(sizeof(FklMem));
	FKL_ASSERT(tmp,"fklNewMem",__FILE__,__LINE__);
	tmp->block=block;
	tmp->destructor=destructor;
	return tmp;
}

void fklFreeMem(FklMem* mem)
{
	void (*f)(void*)=mem->destructor;
	f(mem->block);
	free(mem);
}

FklMemMenager* fklNewMemMenager(size_t size)
{
	FklMemMenager* tmp=(FklMemMenager*)malloc(sizeof(FklMemMenager));
	FKL_ASSERT(tmp,"fklNewMemMenager",__FILE__,__LINE__);
	tmp->s=fklNewPtrStack(size);
	return tmp;
}

void fklFreeMemMenager(FklMemMenager* memMenager)
{
	size_t i=0;
	FklPtrStack* s=memMenager->s;
	for(;i<s->top;i++)
		fklFreeMem(s->data[i]);
	fklFreePtrStack(s);
	free(memMenager);
}

void fklPushMem(void* block,void (*destructor)(void*),FklMemMenager* memMenager)
{
	FklMem* mem=fklNewMem(block,destructor);
	fklPushPtrStack(mem,memMenager->s);
}

void* fklPopMem(FklMemMenager* memMenager)
{
	return fklPopPtrStack(memMenager->s);
}

void fklDeleteMem(void* block,FklMemMenager* memMenager)
{
	FklPtrStack* s=memMenager->s;
	s->top-=1;
	uint32_t i=0;
	uint32_t j=s->top;
	for(;i<s->top;i++)
		if(((FklMem*)s->data[i])->block==block)
			break;
	free(s->data[i]);
	for(;i<j;i++)
		s->data[i]=s->data[i+1];
}

void* fklReallocMem(void* o_block,void* n_block,FklMemMenager* memMenager)
{
	FklPtrStack* s=memMenager->s;
	uint32_t i=0;
	FklMem* m=NULL;
	for(;i<s->top;i++)
		if(((FklMem*)s->data[i])->block==o_block)
		{
			m=s->data[i];
			break;
		}
	m->block=n_block;
	return n_block;
}


