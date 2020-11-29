#include<stdio.h>
#include<pthread.h>
#ifdef _WIN32
#include<conio.h>
#include<windows.h>
#else
#include<termios.h>
#endif
#include<unistd.h>
#include"tool.h"
#include"VMtool.h"
#ifdef __cplusplus
extern "C"{
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
	int resBp(fakeVM* exe)
	{
		VMstack* stack=exe->stack;
		if(stack->tp>stack->bp)return 4;
		VMvalue* prevBp=getTopValue(stack);
		stack->bp=*prevBp->u.num;
		stack->tp-=1;
		return 0;
	}

	int FAKE_getch(fakeVM* exe,pthread_rwlock_t* pGClock)
	{
		VMstack* stack=exe->stack;
		VMprocess* proc=exe->curproc;
		if(resBp(exe))return 4;
		if(stack->tp>=stack->size)
		{
			stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
			if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
			stack->size+=64;
		}
		char ch=getch();
		stack->values[stack->tp]=newVMvalue(CHR,&ch,exe->heap,1);
		stack->tp+=1;
		return 0;
	}

	int FAKE_sleep(fakeVM* exe,pthread_rwlock_t* pGClock)
	{
		VMstack* stack=exe->stack;
		VMprocess* proc=exe->curproc;
		VMvalue* second=getArg(stack);
		if(!second)return 5;
		if(resBp(exe))return 4;
		if(second->type!=IN32)return 1;
		int32_t s=*second->u.num;
		releaseSource(pGClock);
		s=sleep(s);
		if(stack->tp>=stack->size)
		{
			stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size+64));
			if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
			stack->size+=64;
		}
		stack->values[stack->tp]=newVMvalue(IN32,&s,exe->heap,1);
		stack->tp+=1;
		return 0;
	}

	int FAKE_usleep(fakeVM* exe,pthread_rwlock_t* pGClock)
	{
		VMstack* stack=exe->stack;
		VMprocess* proc=exe->curproc;
		VMvalue* second=getArg(stack);
		if(!second)return 5;
		if(resBp(exe))return 4;
		if(second->type!=IN32)return 1;
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
			if(stack->values==NULL)errors(OUTOFMEMORY,__FILE__,__LINE__);
			stack->size+=64;
		}
		stack->values[stack->tp]=newVMvalue(IN32,&s,exe->heap,1);
		stack->tp+=1;
		return 0;
	}

#ifdef __cplusplus
}
#endif
