#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<stdint.h>
#include<math.h>
#include"tool.h"

char* getListFromFile(FILE* file)
{
	char* tmp=NULL;
	char* before;
	int ch;
	int i=0;
	int j;
	int mark=0;
	int braketsNum=0;
	while((ch=getc(file))!=EOF)
	{
		if(ch==';'&&!mark)
		{
			while(getc(file)!='\n');
			ungetc('\n',file);
			continue;
		}
		else if(ch=='\n'&&braketsNum<=0&&!mark){ungetc('\n',file);break;}
		else if(ch=='\'')
		{
			while(isspace((ch=getc(file))));
			ungetc(ch,file);
			char beQuote[]="(quote ";
			char* tmpList=subGetList(file);
			char* other=NULL;
			int len=strlen(tmpList)+strlen(beQuote)+1;
			if(!(other=(char*)malloc(sizeof(char)*len)))errors(OUTOFMEMORY);
			strcpy(other,beQuote);
			strcat(other,tmpList);
			other[len-1]=')';
			i+=len;
			j=i-len;
			before=tmp;
			if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))errors(OUTOFMEMORY);
			memcpy(tmp,before,j);
			memcpy(tmp+j,other,len);
			if(before!=NULL)free(before);
			free(other);
			free(tmpList);
			continue;
		}
		else if(isspace(ch)&&!braketsNum)continue;
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))
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
	if(ch==EOF&&braketsNum!=0&&file!=stdin)
	{
		free(tmp);
		return NULL;
	}
	if(tmp!=NULL)*(tmp+i)='\0';
	return tmp;
}

char* subGetList(FILE* file)
{
	char* tmp=NULL;
	char* before;
	int ch;
	int i=0;
	int j;
	int mark=0;
	int braketsNum=0;
	while((ch=getc(file))!=EOF)
	{
		if(ch==';'&&!mark)
		{
			while(getc(file)!='\n');
			ungetc('\n',file);
			continue;
		}
		if(isspace(ch)&&!braketsNum)
		{
			ungetc(ch,file);
			break;
		}
		else if(ch==')')
		{
			braketsNum--;
			if(braketsNum<0)
			{
				ungetc(ch,file);
				break;
			}
		}
		else if(ch==','&&braketsNum<=0)
		{
			ungetc(ch,file);
			break;
		}
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))
			errors(OUTOFMEMORY);
		memcpy(tmp,before,j);
		*(tmp+j)=ch;
		mark^=(ch=='\"'&&*(tmp+j-1)!='\\');
		if(before!=NULL)free(before);
		if(ch=='(')braketsNum++;
		if(ch==')'&&braketsNum<=0)break;
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

char* DoubleToString(double num)
{
	int i;
	char numString[sizeof(double)*2+3];
	sprintf(numString,"%lf",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=(char*)malloc(lenOfNum*sizeof(char));
	for(i=0;i<lenOfNum;i++)*(tmp+i)=numString[i];
	return tmp;
}


double stringToDouble(const char* str)
{
	double tmp;
	sscanf(str,"%lf",&tmp);
	return tmp;
}



char* intToString(long num)
{
	int i;
	char numString[sizeof(long)*2+3];
	for(i=0;i<sizeof(long)*2+3;i++)numString[i]=0;
	sprintf(numString,"%ld",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=NULL;
	if(!(tmp=(char*)malloc(lenOfNum*sizeof(char))))errors(OUTOFMEMORY);
	memcpy(tmp,numString,lenOfNum);;
	return tmp;
}


int64_t stringToInt(const char* str)
{
	int64_t tmp;
	char* format=NULL;
	if(isHexNum(str))format="%lx";
	else if(isOctNum(str))format="%lo";
	else format="%ld";
	sscanf(str,format,&tmp);
	return tmp;
}


int power(int first,int second)
{
	int i;
	int result=1;
	for(i=0;i<second;i++)result=result*first;
	return result;
}


rawString getStringBetweenMarks(const char* str,intpr* inter)
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
		if(*(str+i)=='\n')inter->curline+=1;
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
			else if(*(str+i+1)=='\n')
			{
				i+=2;
				continue;
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

pair* newPair(int curline,pair* prev)
{
	pair* tmp;
	if((tmp=(pair*)malloc(sizeof(pair))))
	{
		tmp->car.outer=tmp;
		tmp->car.type=NIL;
		tmp->car.value=NULL;
		tmp->cdr.outer=tmp;
		tmp->cdr.type=NIL;
		tmp->cdr.value=NULL;
		tmp->prev=prev;
		tmp->car.curline=curline;
		tmp->cdr.curline=curline;
	}
	else errors(OUTOFMEMORY);
}

cptr* newCptr(int curline,pair* outer)
{
	cptr* tmp=NULL;
	if(!(tmp=(cptr*)malloc(sizeof(cptr))))errors(OUTOFMEMORY);
	tmp->outer=outer;
	tmp->curline=curline;
	tmp->type=NIL;
	tmp->value=NULL;
	return tmp;
}

atom* newAtom(int type,const char* value,pair* prev)
{
	atom* tmp=NULL;
	if(!(tmp=(atom*)malloc(sizeof(atom))))errors(OUTOFMEMORY);
	switch(type)
	{
		case SYM:
		case STR:
			if(!(tmp->value.str=(char*)malloc(strlen(value)+1)))errors(OUTOFMEMORY);
			strcpy(tmp->value.str,value);
			break;
		case CHR:
		case INT:
		case DOU:
			*(int64_t*)(&tmp->value)=0;break;
	}		
	tmp->prev=prev;
	tmp->type=type;
	return tmp;
}

int copyCptr(cptr* objCptr,const cptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	pair* objPair=NULL;
	pair* copiedPair=NULL;
	pair* tmpPair=(copiedCptr->type==PAR)?copiedCptr->value:NULL;
	copiedPair=tmpPair;
	while(1)
	{
		objCptr->type=copiedCptr->type;
		if(copiedCptr->type==PAR)
		{
			objPair=newPair(0,objPair);
			objCptr->value=objPair;
			copiedPair=copiedCptr->value;
			copiedCptr=&copiedPair->car;
			copiedCptr=&copiedPair->car;
			objCptr=&objPair->car;
			continue;
		}
		else if(copiedCptr->type==ATM)
		{
			atom* coAtm=copiedCptr->value;
			atom* objAtm=NULL;
			if(coAtm->type==SYM||coAtm->type==STR)
				objAtm=newAtom(coAtm->type,coAtm->value.str,objPair);
			else
			{
				objAtm=newAtom(coAtm->type,NULL,objPair);
				if(objAtm->type==DOU)objAtm->value.dou=coAtm->value.dou;
				else if(objAtm->type==INT)objAtm->value.num=coAtm->value.num;
				else if(objAtm->type==CHR)objAtm->value.chr=coAtm->value.chr;
			}
			objCptr->value=objAtm;
			if(copiedCptr==&copiedPair->car)
			{
				copiedCptr=&copiedPair->cdr;
				objCptr=&objPair->cdr;
				continue;
			}
			
		}
		else if(copiedCptr->type==NIL)
		{
			objCptr->value=NULL;
			if(copiedCptr==&copiedPair->car)
			{
				objCptr=&objPair->cdr;
				copiedCptr=&copiedPair->cdr;
				continue;
			}
		}
		if(copiedPair!=NULL&&copiedCptr==&copiedPair->cdr)
		{
			pair* objPrev=NULL;
			pair* coPrev=NULL;
			if(copiedPair->prev==NULL)break;
			while(objPair->prev!=NULL&&copiedPair!=NULL&&copiedPair!=tmpPair)
			{
				coPrev=copiedPair;
				copiedPair=copiedPair->prev;
				objPrev=objPair;
				objPair=objPair->prev;
				if(coPrev==copiedPair->car.value)break;
			}
			if(copiedPair!=NULL)
			{
				copiedCptr=&copiedPair->cdr;
				objCptr=&objPair->cdr;
			}
			if(copiedPair==tmpPair&&copiedCptr->type==objCptr->type)break;
		}
		if(copiedPair==NULL)break;
	}
	return 1;
}
void replace(cptr* fir,const cptr* sec)
{
	pair* tmp=fir->outer;
	cptr tmpCptr={NULL,0,NIL,NULL};
	tmpCptr.type=fir->type;
	tmpCptr.value=fir->value;
	copyCptr(fir,sec);
	deleteCptr(&tmpCptr);
	if(fir->type==PAR)((pair*)fir->value)->prev=tmp;
	else if(fir->type==ATM)((atom*)fir->value)->prev=tmp;
}

cptr* destroyCptr(cptr* objCptr)
{
	pair* objPair=NULL;
	if(objCptr->type==PAR)objPair=((pair*)objCptr->value)->prev;
	if(objCptr->type==ATM)objPair=((atom*)objCptr->value)->prev;
	if(objCptr->type==NIL)return objCptr;
	while(objPair!=NULL&&objPair->prev!=NULL)objPair=objPair->prev;
	if(objPair!=NULL)
	{
		deleteCptr(&objPair->car);
		deleteCptr(&objPair->cdr);
	}
	free(objPair);
}
int deleteCptr(cptr* objCptr)
{
	if(objCptr==NULL)return 0;
	pair* tmpPair=(objCptr->type==PAR)?objCptr->value:NULL;
	pair* objPair=tmpPair;
	cptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==PAR)
		{
			if(objPair!=NULL&&tmpCptr==&objPair->cdr)
			{
				objPair=objPair->cdr.value;
				tmpCptr=&objPair->car;
			}
			else
			{
				objPair=tmpCptr->value;
				tmpCptr=&objPair->car;
				continue;
			}
		}
		else if(tmpCptr->type==ATM)
		{
			freeAtom(tmpCptr->value);
			tmpCptr->type=NIL;
			tmpCptr->value=NULL;
			continue;
		}
		else if(tmpCptr->type==NIL)
		{
			if(objPair!=NULL&&tmpCptr==&objPair->car)
			{
				tmpCptr=&objPair->cdr;
				continue;
			}
			else if(objPair!=NULL&&tmpCptr==&objPair->cdr)
			{
				pair* prev=objPair;
				objPair=objPair->prev;
				//printf("free PAR\n");
				free(prev);
				if(objPair==NULL||prev==tmpPair)break;
				if(prev==objPair->car.value)
				{
					objPair->car.type=NIL;
					objPair->car.value=NULL;
				}
				else if(prev==objPair->cdr.value)
				{
					objPair->cdr.type=NIL;
					objPair->cdr.value=NULL;
				}
				tmpCptr=&objPair->cdr;
			}
		}
		if(objPair==NULL)break;
	}
	objCptr->type=NIL;
	objCptr->value=NULL;
	return 0;
}

int cptrcmp(const cptr* first,const cptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	pair* firPair=NULL;
	pair* secPair=NULL;
	pair* tmpPair=(first->type==PAR)?first->value:NULL;
	while(1)
	{
		if(first->type!=second->type)return 0;
		else if(first->type==PAR)
		{
			firPair=first->value;
			secPair=second->value;
			first=&firPair->car;
			second=&secPair->car;
			continue;
		}
		else if(first->type==ATM||first->type==NIL)
		{
			if(first->type==ATM)
			{
				atom* firAtm=first->value;
				atom* secAtm=second->value;
				if(firAtm->type!=secAtm->type)return 0;
				if((firAtm->type==SYM||firAtm->type==STR)&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==DOU&&fabs(firAtm->value.dou-secAtm->value.dou)!=0)return 0;
				else if(!memcmp(&firAtm->type,&secAtm->type,sizeof(atom)-sizeof(pair*)))return 0;
			}
			if(firPair!=NULL&&first==&firPair->car)
			{ first=&firPair->cdr;
				second=&secPair->cdr;
				continue;
			}
		}
		if(firPair!=NULL&&first==&firPair->car)
		{
			first=&firPair->cdr;
			second=&secPair->cdr;
			continue;
		}
		else if(firPair!=NULL&&first==&firPair->cdr)
		{
			pair* firPrev=NULL;
			pair* secPrev=NULL;
			if(firPair->prev==NULL)break;
			while(firPair->prev!=NULL&&firPair!=tmpPair)
			{
				firPrev=firPair;
				secPrev=secPair;
				firPair=firPair->prev;
				secPair=secPair->prev;
				if(firPrev==firPair->car.value)break;
			}
			if(firPair!=NULL)
			{
				first=&firPair->cdr;
				second=&secPair->cdr;
			}
			if(firPair==tmpPair&&first==&firPair->cdr)break;
		}
		if(firPair==NULL&&secPair==NULL)break;
	}
	return 1;
}

cptr* nextCptr(const cptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->cdr.type==PAR)
		return &((pair*)objCptr->outer->cdr.value)->car;
	return NULL;
}

cptr* prevCptr(const cptr* objCptr)
{
	if(objCptr->outer->prev!=NULL)
		return &objCptr->outer->prev->car;
	return NULL;
}

int isHexNum(const char* objStr)
{
	if(objStr!=NULL&&strlen(objStr)>3&&objStr[0]=='-'&&objStr[1]=='0'&&(objStr[2]=='x'||objStr[2]=='X'))return 1;
	if(objStr!=NULL&&strlen(objStr)>2&&objStr[0]=='0'&&(objStr[1]=='x'||objStr[1]=='X'))return 1;
	return 0;
}

int isOctNum(const char* objStr)
{
	if(objStr!=NULL&&strlen(objStr)>2&&objStr[0]=='-'&&objStr[1]=='0'&&!isalpha(objStr[2]))return 1;
	if(objStr!=NULL&&strlen(objStr)>1&&objStr[0]=='0'&&!isalpha(objStr[1]))return 1;
	return 0;
}

int isDouble(const char* objStr)
{
	int i=(objStr[0]=='-')?1:0;
	int len=strlen(objStr);
	for(;i<len;i++)
		if(objStr[i]=='.')return 1;
	return 0;
}

char stringToChar(const char* objStr)
{
	char* format=NULL;
	char ch=0;
	if(isNum(objStr))
	{
		if(isHexNum(objStr))format="%x";
		else if(isOctNum(objStr))format="%o";
		else format="%d";
		sscanf(objStr,format,&ch);
	}
	else
	{
		switch(*(objStr))
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
			default:ch=*(objStr);break;
		}
	}
	return ch;
}

int isNum(const char* objStr)
{
	if(!isxdigit(*objStr)&&(*objStr!='-'||*objStr!='.'))return 0;
	int len=strlen(objStr);
	int i=(*objStr=='-')?1:0;
	int hasDot=0;
	for(;i<len;i++)
	{
		hasDot+=(objStr[i]=='.')?1:0;
		if(objStr[i]=='.'&&hasDot>1)return 0;
		if(!isxdigit(objStr[i])&&objStr[i]!='.')return 0;
	}
	return 1;
}

void freeAtom(atom* objAtm)
{
	if(objAtm->type==SYM||objAtm->type==STR)free(objAtm->value.str);
	free(objAtm);
}

void printRawChar(char chr,FILE* out)
{
	if(isgraph(chr))
		fprintf(out,"#\\%c",chr);
	else
		fprintf(out,"#\\\\0x%x",(int)chr);
}

env* newEnv(env* prev)
{
	env* curEnv=NULL;
	if(!(curEnv=(env*)malloc(sizeof(env))))errors(OUTOFMEMORY);
	if(prev!=NULL)prev->next=curEnv;
	curEnv->prev=prev;
	curEnv->next=NULL;
	curEnv->symbols=NULL;
	return curEnv;
}

void destroyEnv(env* objEnv)
{
	if(objEnv==NULL)errors(WRONGENV);
	while(objEnv!=NULL)
	{
		defines* delsym=objEnv->symbols;
		while(delsym!=NULL)
		{
			free(delsym->symName);
			deleteCptr(&delsym->obj);
			defines* prev=delsym;
			delsym=delsym->next;
			free(prev);
		}
		env* prev=objEnv;
		objEnv=objEnv->next;
		free(prev);
	}
}

intpr* newIntpr(const char* filename,FILE* file)
{
	intpr* tmp=NULL;
	if(!(tmp=(intpr*)malloc(sizeof(intpr))))errors(OUTOFMEMORY);
	if(!(tmp->filename=(char*)malloc(sizeof(char)*(strlen(filename)+1))))errors(OUTOFMEMORY);
	strcpy(tmp->filename,filename);
	tmp->file=file;
	tmp->glob=newEnv(NULL);
	tmp->curline=1;
	return tmp;
}

void freeIntpr(intpr* inter)
{
	free(inter->filename);
	fclose(inter->file);
	destroyEnv(inter->glob);
	free(inter);
}

char getci(intpr* inter)
{
	char ch=getc(inter->file);
	inter->curline+=(ch=='\n')?1:0;
	return ch;
}
