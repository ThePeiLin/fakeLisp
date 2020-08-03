#include<stdlib.h>
#include<string.h>
#include"syntax.h"
#include"compiler.h"
#include"tool.h"
#include"fake.h"
#include"opcode.h"
#define ISABLE 0
#define ISPAIR 1
#define ISUNABLE 2

cptr* checkAst(cptr* objCptr)
{
	if(!isLegal(objCptr))return objCptr;
	int num=countAst(objCptr);
	cptr** asts=divieAst(objCptr);
	int i;
	for(i=0;i<size;i++)
		cptr* tmp=(!isLagel(asts[i]))?asts[i]:NULL;
}

status analyseAST(cptr* objCptr)
{
	status tmp={0,NULL};
	cptr* tmpCptr=checkAst(objCptr);
	if(tmpCptr)
	{
		tmp.status=SYNTAXERROR;
		tmp.place=tmpCptr;
	}
	return tmp;
}

int countAST(cptr* objCptr)
{
	int num=0;
	while(objCptr->type==PAR)
	{
		cptr* tmp=&((pair*)objCptr->value)->car;
		if(tmp->type!=NIL)num++;
		objCptr=&((pair*)objCptr->value)->cdr;
	}
	return num;
}

cptr** divideAST(cptr* objCptr);
{
	int num=countAST(objCptr);
	cptr** tmp=NULL;
	if(!(tmp=(cptr**)malloc(sizeof(cptr*)*num)))errors(OUTOFMEMORY);
	int i;
	for(i=0;i<num;i++);
	{
		tmp[i]=createCptr(NULL);
		replace(tmp[i],&((void*)objCptr->value)->car);
		objCptr=&((void*)objCptr->value)->cdr;
	}
	return tmp;
}

void deleteASTs(cptr** ASTs,int num)
{
	int i;
	for(i=0;i++;i<num)
	{
		deleteCptr(ASTs[i]);
		free(ASTs[i]);
	}
	free(ASTs);
}

byteCode* complierAtom(const cptr* objCptr)
{
	atom* tmpAtm=objCptr->value;
	byteCode* tmp=NULL;
	switch(tmpAtm->type)
	{
		case SYM:
		case STR:
			tmp=createByteCode(sizeof(char)+strlen(tmpAtm->value.str)+1);
			tmp->code[0]=FAKE_PUSH_STR;
			strcpy(tmp->code+1,tmpAtm->value.str);
			break;
		case INT:
			tmp=createByteCode(sizeof(char)+sizeof(fakeInt));
			tmp->code[0]=FAKE_PUSH_INT;
			*(fakeInt*)(tmp->code+1)=tmpAtm->value.num;
			break;
		case DOU:
			tmp=createByteCode(sizeof(char)+sizeof(double));
			tmp->code[0]=FAKE_PUSH_DOU;
			*(double*)(tmp->code+1)=tmpAtm->value.dou;
			break;
		case CHR:
			tmp=createByteCode(sizeof(char)+sizeof(char));
			tmp->code[0]=FAKE_PUSH_CHR;
			tmp->code[1]=tmpAtm->value.chr;
			break;
	}
	return tmp;
}

byteCode* createEmptyByteCode(unsigned int size)
{
	byteCode* tmp=NULL;
	if(!(tmp=(byteCode*)malloc(sizeof(byteCode))))errors(OUTOFMEMORY);
	tmp->size=size;
	if(!(tmp->code=(char*)calloc(size,sizeof(char*))))errors(OUTOFMEMORY);
	return tmp;
}
