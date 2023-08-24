#include<fakeLisp/vm.h>

typedef struct Greylink
{
	FklVMvalue* v;
	struct Greylink* next;
}Greylink;

typedef struct GcCoCtx
{
	FklVMgc* gc;
	uint32_t num;
	uint32_t count;
	volatile FklGCstate* ps;
	struct Greylink* volatile greylink;
}GcCoCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(GcCoCtx);

static inline Greylink* volatile* get_grey_link(FklVMgc* gc)
{
	return &((GcCoCtx*)(gc->GcCo.sf.data))->greylink;
}

static inline Greylink* createGreylink(FklVMvalue* v,struct Greylink* next)
{
	Greylink* g=(Greylink*)malloc(sizeof(Greylink));
	FKL_ASSERT(g);
	g->v=v;
	g->next=next;
	return g;
}

inline void fklGC_toGrey(FklVMvalue* v,FklVMgc* gc)
{
	if(FKL_IS_PTR(v)&&v!=NULL&&v->mark!=FKL_MARK_B)
	{
		v->mark=FKL_MARK_G;
		Greylink* volatile* pgl=get_grey_link(gc);
		*pgl=createGreylink(v,*pgl);
		// gc->grey=createGreylink(v,gc->grey);
		// gc->greyNum++;
	}
}

static inline void fklGC_markRootToGrey(FklVM* exe)
{
	FklVMgc* gc=exe->gc;

	for(FklVMframe* cur=exe->frames;cur;cur=cur->prev)
		fklDoAtomicFrame(cur,gc);

	uint32_t count=exe->ltp;
	FklVMvalue** loc=exe->locv;
	for(uint32_t i=0;i<count;i++)
		fklGC_toGrey(loc[i],gc);

	for(size_t i=1;i<=exe->libNum;i++)
	{
		FklVMlib* lib=&exe->libs[i];
		fklGC_toGrey(lib->proc,gc);
		if(lib->imported)
		{
			for(uint32_t i=0;i<lib->count;i++)
				fklGC_toGrey(lib->loc[i],gc);
		}
	}
	FklVMvalue** base=exe->base;
	for(uint32_t i=0;i<exe->tp;i++)
		fklGC_toGrey(base[i],gc);
	fklGC_toGrey(exe->chan,gc);
}

static inline void fklGC_markAllRootToGrey(FklVM* curVM)
{
	fklGC_markRootToGrey(curVM);

	for(FklVM* cur=curVM->next;cur!=curVM;cur=cur->next)
		fklGC_markRootToGrey(cur);
}

// static inline void fklGC_pause(FklVM* exe)
// {
// 	fklGC_markAllRootToGrey(exe);
// }

static inline void propagateMark(FklVMvalue* root,FklVMgc* gc)
{
	FKL_ASSERT(root->type<=FKL_TYPE_CODE_OBJ);
	static void(* const fkl_atomic_value_method_table[FKL_TYPE_CODE_OBJ+1])(FklVMvalue*,FklVMgc*)=
	{
		NULL,
		NULL,
		NULL,
		fklAtomicVMvec,
		fklAtomicVMpair,
		fklAtomicVMbox,
		NULL,
		fklAtomicVMuserdata,
		fklAtomicVMproc,
		fklAtomicVMchan,
		NULL,
		fklAtomicVMdll,
		fklAtomicVMdlproc,
		NULL,
		fklAtomicVMhashTable,
		NULL,
	};
	void (*atomic_value_func)(FklVMvalue*,FklVMgc*)=fkl_atomic_value_method_table[root->type];
	if(atomic_value_func)
		atomic_value_func(root,gc);
}

static inline int fklGC_propagate(FklVMgc* gc)
{
	Greylink* volatile* pgl=get_grey_link(gc);
	FklVMvalue* v=FKL_VM_NIL;
	Greylink* c=*pgl;
	if(c)
	{
		// gc->greyNum--;
		*pgl=c->next;
		v=c->v;
		free(c);
	}
	if(FKL_IS_PTR(v)&&v->mark==FKL_MARK_G)
	{
		v->mark=FKL_MARK_B;
		propagateMark(v,gc);
	}
	return *pgl==NULL;
}

static inline void fklGC_collect(FklVMgc* gc,FklVMvalue** pw)
{
	size_t count=0;
	FklVMvalue* head=gc->head;
	gc->head=NULL;
	gc->running=FKL_GC_SWEEPING;
	FklVMvalue** phead=&head;
	while(*phead)
	{
		FklVMvalue* cur=*phead;
		if(cur->mark==FKL_MARK_W)
		{
			*phead=cur->next;
			cur->next=*pw;
			*pw=cur;
			count++;
		}
		else
		{
			cur->mark=FKL_MARK_W;
			phead=&cur->next;
		}
	}
	*phead=gc->head;
	gc->head=head;
	gc->num-=count;
}

inline void fklGC_sweep(FklVMvalue* head)
{
	FklVMvalue** phead=&head;
	while(*phead)
	{
		FklVMvalue* cur=*phead;
		*phead=cur->next;
		fklDestroyVMvalue(cur);
	}
}

inline void fklGetGCstateAndGCNum(FklVMgc* gc,FklGCstate* s,int* cr)
{
	*s=gc->running;
	*cr=gc->num>gc->threshold;
}

//#define FKL_GC_GRAY_FACTOR (16)
//#define FKL_GC_NEW_FACTOR (4)

//inline void fklGC_step(FklVM* exe)
//{
//	FklGCstate running=FKL_GC_NONE;
//	int cr=0;
//	fklGetGCstateAndGCNum(exe->gc,&running,&cr);
//	static size_t greyNum=0;
//	cr=1;
//	switch(running)
//	{
//		case FKL_GC_NONE:
//			if(cr)
//			{
//				fklChangeGCstate(FKL_GC_MARK_ROOT,exe->gc);
//				fklGC_pause(exe);
//				fklChangeGCstate(FKL_GC_PROPAGATE,exe->gc);
//			}
//			break;
//		case FKL_GC_MARK_ROOT:
//			break;
//		case FKL_GC_PROPAGATE:
//			{
//				size_t create_n=exe->gc->greyNum-greyNum;
//				size_t timce=exe->gc->greyNum/FKL_GC_GRAY_FACTOR+create_n/FKL_GC_NEW_FACTOR+1;
//				greyNum+=create_n;
//				for(size_t i=0;i<timce;i++)
//					if(fklGC_propagate(exe->gc))
//					{
//						fklChangeGCstate(FKL_GC_SWEEP,exe->gc);
//						break;
//					}
//			}
//			break;
//		case FKL_GC_SWEEP:
//			if(!pthread_mutex_trylock(&GCthreadLock))
//			{
//				pthread_create(&GCthreadId,NULL,fklGC_sweepThreadFunc,exe->gc);
//				fklChangeGCstate(FKL_GC_COLLECT,exe->gc);
//				pthread_mutex_unlock(&GCthreadLock);
//			}
//			break;
//		case FKL_GC_COLLECT:
//		case FKL_GC_SWEEPING:
//			break;
//		case FKL_GC_DONE:
//			if(!pthread_mutex_trylock(&GCthreadLock))
//			{
//				fklChangeGCstate(FKL_GC_NONE,exe->gc);
//				pthread_join(GCthreadId,NULL);
//				pthread_mutex_unlock(&GCthreadLock);
//			}
//			break;
//		default:
//			FKL_ASSERT(0);
//			break;
//	}
//}

//void* fklGC_sweepThreadFunc(void* arg)
//{
//	FklVMgc* gc=arg;
//	FklVMvalue* white=NULL;
//	fklGC_collect(gc,&white);
//	fklChangeGCstate(FKL_GC_SWEEPING,gc);
//	fklGC_sweep(white);
//	fklChangeGCstate(FKL_GC_DONE,gc);
//	return NULL;
//}

void* fklGC_threadFunc(void* arg)
{
	FklVM* exe=arg;
	FklVMgc* gc=exe->gc;
	gc->running=FKL_GC_MARK_ROOT;
	fklGC_markAllRootToGrey(exe);
	gc->running=FKL_GC_PROPAGATE;
	while(!fklGC_propagate(gc));
	FklVMvalue* white=NULL;
	gc->running=FKL_GC_COLLECT;
	fklGC_collect(gc,&white);
	gc->running=FKL_GC_SWEEP;
	gc->running=FKL_GC_SWEEPING;
	fklGC_sweep(white);
	gc->running=FKL_GC_NONE;
	return NULL;
}

static inline void initFrameCtxToGcCoCtx(FklCallObjData data,FklVMgc* gc)
{
	GcCoCtx* c=(GcCoCtx*)data;
	c->num=gc->num;
	c->gc=gc;
	c->ps=&gc->running;
}

static void gc_frame_atomic(FklCallObjData data,FklVMgc* gc)
{
}

static void gc_frame_step(FklCallObjData data,FklVM* exe)
{
	// fprintf(stderr,"gc step\n");
	FklVMgc* gc=exe->gc;
	gc->running=FKL_GC_MARK_ROOT;
	fklGC_markAllRootToGrey(exe);
	gc->running=FKL_GC_PROPAGATE;
	while(!fklGC_propagate(gc));
	FklVMvalue* white=NULL;
	gc->running=FKL_GC_COLLECT;
	fklGC_collect(gc,&white);
	gc->running=FKL_GC_SWEEP;
	gc->running=FKL_GC_SWEEPING;
	fklGC_sweep(white);
	gc->running=FKL_GC_DONE;
}

static int gc_frame_end(FklCallObjData data)
{
	GcCoCtx* ctx=(GcCoCtx*)data;
	FklVMgc* gc=ctx->gc;
	int r=gc->running==FKL_GC_DONE;
	if(r)
		gc->running=FKL_GC_NONE;
	return r;
}

static void gc_frame_finalizer(FklCallObjData data)
{
}

static const FklVMframeContextMethodTable GcCoMethodTable=
{
	.step=gc_frame_step,
	.end=gc_frame_end,
	.atomic=gc_frame_atomic,
	.finalizer=gc_frame_finalizer
};

static inline void initGcCoVM(FklVM* exe,FklVMgc* gc)
{
	FklVMframe* f=&exe->sf;
	f->prev=NULL;
	f->type=FKL_FRAME_OTHEROBJ;
	f->t=&GcCoMethodTable;
	exe->frames=f;
	initFrameCtxToGcCoCtx(f->data,gc);
}

inline FklVMgc* fklCreateVMgc()
{
	FklVMgc* tmp=(FklVMgc*)calloc(1,sizeof(FklVMgc));
	FKL_ASSERT(tmp);
	tmp->threshold=FKL_THRESHOLD_SIZE;
	FklVM* gcvm=&tmp->GcCo;
	gcvm->gc=tmp;
	gcvm->state=FKL_VM_WAITING;
	gcvm->prev=gcvm;
	gcvm->next=gcvm;
	return tmp;
}

static inline void insert_gc_vm(FklVM* prev,FklVMgc* gc)
{
	FklVM* gcvm=&gc->GcCo;
	gcvm->state=FKL_VM_READY;
	FklVM* next=prev->next;
	gcvm->prev=prev;
	gcvm->next=next;
	prev->next=gcvm;
	next->prev=gcvm;
}

void fklTryGC(FklVM* vm)
{
	FklVMgc* gc=vm->gc;
	FklVM* gcvm=&gc->GcCo;
	if(gc->num>gc->threshold&&gcvm->next==gcvm&&gc->running==FKL_GC_NONE)
	{
		gc->running=FKL_GC_MARK_ROOT;
		initGcCoVM(&gc->GcCo,gc);
		insert_gc_vm(vm,gc);
	}
}

void fklAddToGC(FklVMvalue* v,FklVM* vm)
{
	FklVMgc* gc=vm->gc;
	if(FKL_IS_PTR(v))
	{
		FklGCstate running=fklGetGCstate(gc);
		if(running>FKL_GC_NONE&&running<FKL_GC_SWEEPING)
			fklGC_toGrey(v,gc);
		else
			v->mark=FKL_MARK_W;
		gc->num+=1;
		v->next=gc->head;
		gc->head=v;
		fklTryGC(vm);
	}
}

void fklDestroyVMgc(FklVMgc* gc)
{
	fklDestroyAllValues(gc);
	free(gc);
}

