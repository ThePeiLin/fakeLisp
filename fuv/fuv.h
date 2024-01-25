#ifndef FKL_FUV_FUV_H
#define FKL_FUV_FUV_H

#include<fakeLisp/vm.h>
#include<uv.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct
{
	uv_loop_t loop;
	FklHashTable gc_values;
}FuvLoop;

int isFuvLoop(FklVMvalue* v);
FklVMvalue* createFuvLoop(FklVM*);

#ifdef __cplusplus
}
#endif

#endif
