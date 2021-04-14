#include"common.h"
#include"VMtool.h"
#include<stdio.h>
#include<pthread.h>
#ifdef _WIN32
#include<conio.h>
#include<windows.h>
#else
#include<termios.h>
#endif
#include<time.h>
#include<string.h>
#include<unistd.h>
#include<limits.h>
#ifdef __cplusplus
extern "C"
{
#endif
#ifndef _WIN32

int getch()
{
	struct termios oldt,newt;
	int ch;
	tcgetattr(STDIN_FILENO,&oldt);
	newt=oldt;
	newt.c_lflag &=~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO,TCSANOW,&newt);
	ch=getchar();
	tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
	return ch;
}
#endif
int resBp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	if(stack->tp>stack->bp)return TOOMANYARG;
	VMvalue* prevBp=getTopValue(stack);
	stack->bp=*prevBp->u.num;
	stack->tp-=1;
	return 0;
}

int FAKE_getch(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	if(resBp(exe))return TOOMANYARG;
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("FAKE_getch",__FILE__,__LINE__);
		stack->size+=64;
	}
	char ch=getch();
	SET_RETURN("FAKE_getch",newVMvalue(CHR,&ch,exe->heap,1),stack);
	return 0;
}

int FAKE_sleep(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* second=getArg(stack);
	if(!second)return TOOFEWARG;
	if(resBp(exe))return TOOMANYARG;
	if(second->type!=IN32)return WRONGARG;
	int32_t s=*second->u.num;
	releaseSource(pGClock);
	s=sleep(s);
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("FAKE_sleep",__FILE__,__LINE__);
		stack->size+=64;
	}
	lockSource(pGClock);
	stack->values[stack->tp]=newVMvalue(IN32,&s,exe->heap,1);
	stack->tp+=1;
	return 0;
}

int FAKE_usleep(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* second=getArg(stack);
	if(!second)return TOOFEWARG;
	if(resBp(exe))return TOOMANYARG;
	if(second->type!=IN32)return WRONGARG;
	int32_t s=*second->u.num;
	releaseSource(pGClock);
#ifdef _WIN32
		Sleep(s);
#else
		usleep(s);
#endif
	if(stack->tp>=stack->size)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
		if(stack->values==NULL)errors("FAKE_usleep",__FILE__,__LINE__);
		stack->size+=64;
	}
	lockSource(pGClock);
	stack->values[stack->tp]=newVMvalue(IN32,&s,exe->heap,1);
	stack->tp+=1;
	return 0;
}

int FAKE_exit(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* exitCode=getArg(stack);
	if(exitCode==NULL)
		exit(0);
	int32_t num=*exitCode->u.num;
	exit(num);
	return 0;
}

int FAKE_rand(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	static int hasSrand=0;
	if(!hasSrand)
	{
		srand((unsigned)time(NULL));
		hasSrand=1;
	}
	VMstack* stack=exe->stack;
	VMvalue*  lim=getArg(stack);
	if(resBp(exe))return TOOMANYARG;
	if(lim&&lim->type!=IN32)return WRONGARG;
	int32_t limit=(lim==NULL||*lim->u.num==0)?RAND_MAX:*lim->u.num;
	int32_t result=rand()%limit;
	VMvalue* toReturn=newVMvalue(IN32,&result,exe->heap,1);
	SET_RETURN("FAKE_rand",toReturn,stack);
	return 0;
}

int FAKE_getTime(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	if(resBp(exe))return TOOMANYARG;
	time_t timer=time(NULL);
	struct tm* tblock=NULL;
	tblock=localtime(&timer);
	char* sec=intToString(tblock->tm_sec);
	char* min=intToString(tblock->tm_min);
	char* hour=intToString(tblock->tm_hour);
	char* day=intToString(tblock->tm_mday);
	char* mon=intToString(tblock->tm_mon+1);
	char* year=intToString(tblock->tm_year+1900);
	int32_t timeLen=strlen(year)+strlen(mon)+strlen(day)+strlen(hour)+strlen(min)+strlen(sec)+5;
	char* trueTime=(char*)malloc(sizeof(char)*(timeLen+1));
	if(!trueTime)
		errors("FAKE_getTime",__FILE__,__LINE__);
	sprintf(trueTime,"%s-%s-%s_%s_%s_%s",year,mon,day,hour,min,sec);
	free(sec);
	free(min);
	free(hour);
	free(day);
	free(mon);
	free(year);
	VMstr* tmpStr=newVMstr(trueTime);
	VMvalue* tmpVMvalue=newVMvalue(STR,tmpStr,exe->heap,1);
	SET_RETURN("FAKE_getTime",tmpVMvalue,stack);
	free(trueTime);
	return 0;
}

int FAKE_removeFile(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* name=getArg(stack);
	if(!name)
		return TOOFEWARG;
	if(resBp(exe))
		return TOOMANYARG;
	if(name->type!=STR)
		return WRONGARG;
	int32_t i=remove(name->u.str->str);
	VMvalue* toReturn=newVMvalue(IN32,&i,exe->heap,1);
	SET_RETURN("FAKE_removeFile",toReturn,stack);
	return 0;
}

int FAKE_argv(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* retval=NULL;
	if(resBp(exe))
		return TOOMANYARG;
	else
	{
		retval=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
		VMvalue* tmp=retval;
		int32_t i=0;
		for(;i<exe->argc;i++,tmp=getCdr(tmp))
		{
			tmp->u.pair->car=newVMvalue(STR,newVMstr(exe->argv[i]),exe->heap,1);
			tmp->u.pair->cdr=newVMvalue(PAIR,newVMpair(exe->heap),exe->heap,1);
		}
	}
	SET_RETURN("FAKE_argv",retval,stack);
	return 0;
}

int FAKE_setChanlBufferSize(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMvalue* chan=getArg(stack);
	VMvalue* size=getArg(stack);
	if(resBp(exe))
		return TOOMANYARG;
	if(size==NULL||chan==NULL)
		return TOOFEWARG;
	if(size->type!=IN32||chan->type!=CHAN)
		return WRONGARG;
	chan->u.chan->max=*size->u.num;
	SET_RETURN("FAKE_setChanlBufferSize",chan,stack);
	return 0;
}

int FAKE_symbolToValue(FakeVM* exe,pthread_rwlock_t* pGClock)
{
	VMstack* stack=exe->stack;
	VMprocess* proc=exe->curproc;
	VMvalue* symbol=getArg(stack);
	if(!symbol)
		return TOOFEWARG;
	if(resBp(exe))
		return TOOFEWARG;
	if(symbol->type!=SYM)
		return WRONGARG;
	int32_t id=findSymbol(symbol->u.str->str,exe->table)->id;
	VMenv* curEnv=proc->prev->localenv;
	VMvalue* tmpValue=NULL;
	VMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
	{
		tmp=findVMenvNode(id,curEnv);
		curEnv=curEnv->prev;
	}
	if(tmp==NULL)
		return 0;
	tmpValue=tmp->value;
	SET_RETURN("FAKE_symbolToValue",tmpValue,stack);
	return 0;
}
#ifdef __cplusplus
}
#endif
