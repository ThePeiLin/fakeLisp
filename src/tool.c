#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include"tool.h"

char* getListFromFile(FILE* file)
{
	char* tmp=NULL;
	char* before;
	char ch;
	int i=0;
	int j;
	int mark=0;
	int braketsNum=0;
	while((ch=getc(file))!=EOF)
	{
		if(ch==';'&&!mark)
		{
			while(getc(file)!='\n');
			continue;
		}
		if(ch=='\n'&&braketsNum<=0)break;
		if(isspace(ch)&&!braketsNum)continue;
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*i)))
			errors(OUTOFMEMORY);
		memcpy(tmp,before,j);
		*(tmp+j)=ch;
		mark^=(ch=='\"'&&*(tmp+j-1)!='\\');
		if(before!=NULL)free(before);
		if(ch=='(')braketsNum++;
		else if(ch==')')
		{
			braketsNum--;
			if(braketsNum<=0)break;
		}
		else continue;
	}
	if(tmp!=NULL)*(tmp+i)='\0';
	return tmp;
}

char* getStringFromList(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while((*(str+len)!='(')
			&&(*(str+len)!=')')
			&&!isspace(*(str+len))
			&&(*(str+len)!=',')
			&&(*(str+len)!=0))len++;
	if(!(tmp=(char*)malloc(sizeof(char)*len+1)))errors(OUTOFMEMORY);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* floatToString(double num)
{
	int i;
	char numString[sizeof(double)*2+3];
	sprintf(numString,"%lf",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	for(i=0;i<lenOfNum;i++)*(tmp+i)=numString[i];
	return tmp;
}


double stringToFloat(const char* str)
{
	double tmp;
	sscanf(str,"%lf",&tmp);
	return tmp;
}



char* intToString(long num)
{
	int i;
	char numString[sizeof(long)*2+3];
	sprintf(numString,"%ld",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	for(i=0;i<lenOfNum;i++)*(tmp+i)=numString[i];
	return tmp;
}


long stringToInt(const char* str)
{
	long tmp;
	sscanf(str,"%ld",&tmp);
	return tmp;
}


int power(int first,int second)
{
	int i;
	int result=1;
	for(i=0;i<second;i++)result=result*first;
	return result;
}


rawString getStringBetweenMarks(const char* str)
{
	rawString obj;
	char ch=0;
	char* tmp=NULL;
	char* before;
	int len=0;
	int i=1;
	int j;
	while(*(str+i)!='\"')
	{
		if(*(str+i)=='\\')
		{
			if(isdigit(*(str+i+1)))
			{
				if(*(str+i+1)=='0')
				{
					if(isdigit(*(str+i+2)))
					{
						ch=0;
						int k=0;
						int len;
						while((isdigit(*(str+i+2+k))&&*(str+i+2+k)<'8')&&k<4)k++;
						len=k;
						while(k>0)ch=ch+(*(str+i+2+len-k)-'0')*power(8,--k);
						i+=len+2;
					}
					else if((*(str+i+2))=='x'||(*(str+i+2))=='X')
					{
						ch=0;
						int k=0;
						int len;
						while(isxdigit(*(str+i+3)))k++;
						len=k;
						while(k>0)
						{
							int result;
							switch(*(str+i+3+len-k))
							{
								case 'a':
								case 'A':result=10;break;
								case 'b':
								case 'B':result=11;break;
								case 'c':
								case 'C':result=12;break;
								case 'd':
								case 'D':result=13;break;
								case 'e':
								case 'E':result=14;break;
								case 'f':
								case 'F':result=15;break;
								default:if(isdigit(*(str+i+3+len-k)))result=*(str+i+3+len-k)-'0';
							}
							ch=ch+result*power(16,--k);
						}
						i+=len+3;
					}
				}
				else if(isdigit(*(str+i+1))&&*(str+i+1)!='0')
				{
					ch=0;
					int k=0;
					int len;
					while(isdigit(*(str+i+1+k))&&k<4)k++;
					len=k;
					while(k>0)ch=ch+(*(str+i+1+len-k)-'0')*power(10,--k);
					i+=len+1;
				}
			}
			else
			{
				switch(*(str+i+1))
				{
					case 'A':
					case 'a':ch=0x07;break;
					case 'b':
					case 'B':ch=0x08;break;
					case 't':
					case 'T':ch=0x09;break;
					case 'n':
					case 'N':ch=0x0a;break;
					case 'v':
					case 'V':ch=0x0b;break;
					case 'f':
					case 'F':ch=0x0c;break;
					case 'r':
					case 'R':ch=0x0d;break;
					case '\\':ch=0x5c;break;
					default:ch=*(str+i+1);break;
				}
				i+=2;
			}
		}
		else ch=*(str+(i++));
		len++;
		j=len-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(len+1))))
		{
			fprintf(stderr,"Out of memory!\n");
			exit(1);
		}
		if(before!=NULL)memcpy(tmp,before,j);
		*(tmp+j)=ch;
		if(before!=NULL)free(before);
	}
	if(tmp!=NULL)*(tmp+len)='\0';
	obj.len=i+1;
	obj.str=tmp;
	return obj;
}
void printRawString(const char* objStr,FILE* out)
{
	const char* tmpStr=objStr;
	int len=strlen(objStr);
	putc('\"',out);
	for(;tmpStr<objStr+len;tmpStr++)
	{
		if(isgraph(*tmpStr))
			putc(*tmpStr,out);
		else
			fprintf(out,"\\0x%0x",*tmpStr);
	}
	putc('\"',out);
}

void errors(int types)
{
	static char* inform[]=
	{
		"dummy",
		"Out of memory!\n",
		"Wrong environment!\n",
	};
	fprintf(stderr,"error:%s",inform[types]);
	exit(1);
}

cell* createCell(cell* prev)
{
	cell* tmp;
	if((tmp=(cell*)malloc(sizeof(cell))))
	{
		tmp->car.outer=tmp;
		tmp->car.type=NIL;
		tmp->car.value=NULL;
		tmp->cdr.outer=tmp;
		tmp->cdr.type=NIL;
		tmp->cdr.value=NULL;
		tmp->prev=prev;
	}
	else errors(OUTOFMEMORY);
}

cptr* createCptr(cell* outer)
{
	cptr* tmp=NULL;
	if(!(tmp=(cptr*)malloc(sizeof(cptr))))errors(OUTOFMEMORY);
	tmp->outer=outer;
	tmp->type=NIL;
	tmp->value=NULL;
	return tmp;
}

atom* createAtom(int type,const char* value,cell* prev)
{
	atom* tmp=NULL;
	if(!(tmp=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
	if(!(tmp->value=(char*)malloc(strlen(value)+1)))errors(OUTOFMEMORY);
	tmp->prev=prev;
	tmp->type=type;
	memcpy(tmp->value,value,strlen(value)+1);
	return tmp;
}

int copyCptr(cptr* objCptr,const cptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	cell* objCell=NULL;
	cell* copiedCell=NULL;
	cell* tmpCell=(copiedCptr->type==CEL)?copiedCptr->value:NULL;
	copiedCell=tmpCell;
	while(1)
	{
		objCptr->type=copiedCptr->type;
		if(copiedCptr->type==CEL)
		{
			objCell=createCell(objCell);
			objCptr->value=objCell;
			copiedCell=copiedCptr->value;
			copiedCptr=&copiedCell->car;
			copiedCptr=&copiedCell->car;
			objCptr=&objCell->car;
			continue;
		}
		else if(copiedCptr->type==ATM)
		{
			atom* coAtm=copiedCptr->value;
			atom* objAtm=NULL;
			if(!(objAtm=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
			objCptr->value=objAtm;
			if(!(objAtm->value=(char*)malloc(strlen(coAtm->value)+1)))errors(OUTOFMEMORY);
			objAtm->type=coAtm->type;
			memcpy(objAtm->value,coAtm->value,strlen(coAtm->value)+1);
			if(copiedCptr==&copiedCell->car)
			{
				copiedCptr=&copiedCell->cdr;
				objCptr=&objCell->cdr;
				continue;
			}
			
		}
		else if(copiedCptr->type==NIL)
		{
			objCptr->value=NULL;
			if(copiedCptr==&copiedCell->car)
			{
				objCptr=&objCell->cdr;
				copiedCptr=&copiedCell->cdr;
				continue;
			}
		}
		if(copiedCell!=NULL&&copiedCptr==&copiedCell->cdr)
		{
			cell* objPrev=NULL;
			cell* coPrev=NULL;
			if(copiedCell->prev==NULL)break;
			while(objCell->prev!=NULL&&copiedCell!=NULL&&copiedCell!=tmpCell)
			{
				coPrev=copiedCell;
				copiedCell=copiedCell->prev;
				objPrev=objCell;
				objCell=objCell->prev;
				if(coPrev==copiedCell->car.value)break;
			}
			if(copiedCell!=NULL)
			{
				copiedCptr=&copiedCell->cdr;
				objCptr=&objCell->cdr;
			}
			if(copiedCell==tmpCell&&copiedCptr->type==objCptr->type)break;
		}
		if(copiedCell==NULL)break;
	}
	return 1;
}
void replace(cptr* fir,const cptr* sec)
{
	cell* tmp=fir->outer;
	cptr tmpCptr={NULL,NIL,NULL};
	tmpCptr.type=fir->type;
	tmpCptr.value=fir->value;
	copyCptr(fir,sec);
	deleteCptr(&tmpCptr);
	if(fir->type==CEL)((cell*)fir->value)->prev=tmp;
	else if(fir->type==ATM)((atom*)fir->value)->prev=tmp;
}

cptr* destroyCptr(cptr* objCptr)
{
	cell* objCell=NULL;
	if(objCptr->type==CEL)objCell=((cell*)objCptr->value)->prev;
	if(objCptr->type==ATM)objCell=((atom*)objCptr->value)->prev;
	if(objCptr->type==NIL)return objCptr;
	while(objCell!=NULL&&objCell->prev!=NULL)objCell=objCell->prev;
	if(objCell!=NULL)
	{
		deleteCptr(&objCell->car);
		deleteCptr(&objCell->cdr);
	}
	free(objCell);
}
int deleteCptr(cptr* objCptr)
{
	if(objCptr==NULL)return 0;
	cell* tmpCell=(objCptr->type==CEL)?objCptr->value:NULL;
	cell* objCell=tmpCell;
	cptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==CEL)
		{
			if(objCell!=NULL&&tmpCptr==&objCell->cdr)
			{
				objCell=objCell->cdr.value;
				tmpCptr=&objCell->car;
			}
			else
			{
				objCell=tmpCptr->value;
				tmpCptr=&objCell->car;
				continue;
			}
		}
		else if(tmpCptr->type==ATM)
		{
			atom* tmpAtm=(atom*)tmpCptr->value;
			//printf("%s\n",tmpAtm->value);
			free(tmpAtm->value);
			free(tmpAtm);
			tmpCptr->type=NIL;
			tmpCptr->value=NULL;
			continue;
		}
		else if(tmpCptr->type==NIL)
		{
			if(objCell!=NULL&&tmpCptr==&objCell->car)
			{
				tmpCptr=&objCell->cdr;
				continue;
			}
			else if(objCell!=NULL&&tmpCptr==&objCell->cdr)
			{
				cell* prev=objCell;
				objCell=objCell->prev;
				//printf("free CELL\n");
				free(prev);
				if(objCell==NULL||prev==tmpCell)break;
				if(prev==objCell->car.value)
				{
					objCell->car.type=NIL;
					objCell->car.value=NULL;
				}
				else if(prev==objCell->cdr.value)
				{
					objCell->cdr.type=NIL;
					objCell->cdr.value=NULL;
				}
				tmpCptr=&objCell->cdr;
			}
		}
		if(objCell==NULL)break;
	}
	objCptr->type=NIL;
	objCptr->value=NULL;
	return 0;
}

int cptrcmp(const cptr* first,const cptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	cell* firCell=NULL;
	cell* secCell=NULL;
	cell* tmpCell=(first->type==CEL)?first->value:NULL;
	while(1)
	{
		if(first->type!=second->type)return 0;
		else if(first->type==CEL)
		{
			firCell=first->value;
			secCell=second->value;
			first=&firCell->car;
			second=&secCell->car;
			continue;
		}
		else if(first->type==ATM||first->type==NIL)
		{
			if(first->type==ATM)
			{
				atom* firAtm=first->value;
				atom* secAtm=second->value;
				if(firAtm->type!=secAtm->type)return 0;
				if(strcmp(firAtm->value,secAtm->value)!=0)return 0;
			}
			if(firCell!=NULL&&first==&firCell->car)
			{
				first=&firCell->cdr;
				second=&secCell->cdr;
				continue;
			}
		}
		if(firCell!=NULL&&first==&firCell->car)
		{
			first=&firCell->cdr;
			second=&secCell->cdr;
			continue;
		}
		else if(firCell!=NULL&&first==&firCell->cdr)
		{
			cell* firPrev=NULL;
			cell* secPrev=NULL;
			if(firCell->prev==NULL)break;
			while(firCell->prev!=NULL&&firCell!=tmpCell)
			{
				firPrev=firCell;
				secPrev=secCell;
				firCell=firCell->prev;
				secCell=secCell->prev;
				if(firPrev==firCell->car.value)break;
			}
			if(firCell!=NULL)
			{
				first=&firCell->cdr;
				second=&secCell->cdr;
			}
			if(firCell==tmpCell&&first==&firCell->cdr)break;
		}
		if(firCell==NULL&&secCell==NULL)break;
	}
	return 1;
}
