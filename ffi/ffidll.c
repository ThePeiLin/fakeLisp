#include"ffidll.h"
#include<fakeLisp/utils.h>
pthread_mutex_t GlobSharedObjsMutex=PTHREAD_MUTEX_INITIALIZER;

typedef struct FklSharedObjNode
{
	FklVMdllHandle dll;
	struct FklSharedObjNode* next;
}FklSharedObjNode;

FklSharedObjNode* GlobSharedObjs=NULL;
void fklAddSharedObj(FklVMdllHandle handle)
{
	FklSharedObjNode* node=(FklSharedObjNode*)malloc(sizeof(FklSharedObjNode));
	FKL_ASSERT(node,__func__);
	node->dll=handle;
	pthread_mutex_lock(&GlobSharedObjsMutex);
	node->next=GlobSharedObjs;
	GlobSharedObjs=node;
	pthread_mutex_unlock(&GlobSharedObjsMutex);
}
