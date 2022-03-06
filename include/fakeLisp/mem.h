#ifndef FKL_MEM_H
#define FKL_MEM_H
#include<stddef.h>
typedef void (*FklGenDestructor)(void*);
typedef struct
{
	void (*destructor)(void*);
	void* block;
}FklMem;

typedef struct
{
	struct FklPtrStack* s;
}FklMemMenager;

FklMem* fklNewMem(void* mem,void (*destructor)(void*));
void fklFreeMem(FklMem*);

FklMemMenager* fklNewMemMenager(size_t size);
void* fklReallocMem(void* o_block,void* n_block,FklMemMenager*);
void fklFreeMemMenager(FklMemMenager*);
void fklPushMem(void* block,void (*destructor)(void*),FklMemMenager* memMenager);
void fklDeleteMem(void* block,FklMemMenager* memMenager);

#endif
