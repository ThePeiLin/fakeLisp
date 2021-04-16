#define USE_CODE_NAME
#include"common.h"
#include"opcode.h"
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<stdint.h>
#include<math.h>
#include<unistd.h>
#include<limits.h>
#ifndef _WIN32
#include<dlfcn.h>
#else
#include<tchar.h>
#endif

char* builtInSymbolList[NUMOFBUILTINSYMBOL]=
{
	"nil",
	"EOF",
	"stdin",
	"stdout",
	"stderr",
	"cons",
	"car",
	"cdr",
	"atom",
	"null",
	"type",
	"aply",
	"eq",
	"eqn",
	"equal",
	"gt",
	"ge",
	"lt",
	"le",
	"not",
	"dbl",
	"str",
	"sym",
	"chr",
	"int",
	"byt",
	"add",
	"sub",
	"mul",
	"div",
	"rem",
	"nth",
	"length",
	"appd",
	"file",
	"read",
	"getb",
	"write",
	"putb",
	"princ",
	"go",
	"chanl",
	"send",
	"recv",
	"clcc"
};

char* getStringFromList(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while((*(str+len)!='(')
			&&(*(str+len)!=')')
			&&!isspace(*(str+len))
			&&(*(str+len)!=',')
			&&(*(str+len)!=0))len++;
	if(!(tmp=(char*)malloc(sizeof(char)*(len+1))))errors("getStringFromList",__FILE__,__LINE__);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* getStringAfterBackslash(const char* str)
{
	char* tmp=NULL;
	int len=0;
	while(!isspace(*(str+len))&&*(str+len)!='\0')
	{
		len++;
		if(!isalnum(str[len])&&str[len-1]!='\\')break;
	}
	if(!(tmp=(char*)malloc(sizeof(char)*(len+1))))errors("getStringAfterBackslash",__FILE__,__LINE__);
	memcpy(tmp,str,len);
	if(tmp!=NULL)*(tmp+len)='\0';
	return tmp;
}

char* doubleToString(double num)
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
	size_t i;
	char numString[sizeof(long)*2+3];
	for(i=0;i<sizeof(long)*2+3;i++)numString[i]=0;
	sprintf(numString,"%ld",num);
	int lenOfNum=strlen(numString)+1;
	char* tmp=NULL;
	if(!(tmp=(char*)malloc(lenOfNum*sizeof(char))))errors("intToString",__FILE__,__LINE__);
	memcpy(tmp,numString,lenOfNum);;
	return tmp;
}


int32_t stringToInt(const char* str)
{
	int32_t tmp;
	char* format=NULL;
	if(isHexNum(str+(str[0]=='0')))format="%lx";
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

void printRawString(const char* objStr,FILE* out)
{
	const char* tmpStr=objStr;
	int len=strlen(objStr);
	putc('\"',out);
	for(;tmpStr<objStr+len;tmpStr++)
	{
		if(*tmpStr=='\"')
			fprintf(out,"\\\"");
		else if(isgraph(*tmpStr)||*tmpStr<0)
			putc(*tmpStr,out);
		else if(*tmpStr=='\x20')
			putc(*tmpStr,out);
		else
		{
			uint8_t j=*tmpStr;
			fprintf(out,"\\0x");
			fprintf(out,"%X",j/16);
			fprintf(out,"%X",j%16);
		}
	}
	putc('\"',out);
}

void errors(const char* str,const char* filename,int line)
{
	fprintf(stderr,"In file \"%s\" line %d\n",filename,line);
	perror(str);
	exit(1);
}

AST_pair* newPair(int curline,AST_pair* prev)
{
	AST_pair* tmp;
	if((tmp=(AST_pair*)malloc(sizeof(AST_pair))))
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
	else errors("newPair",__FILE__,__LINE__);
	return tmp;
}

AST_cptr* newCptr(int curline,AST_pair* outer)
{
	AST_cptr* tmp=NULL;
	if(!(tmp=(AST_cptr*)malloc(sizeof(AST_cptr))))errors("newCptr",__FILE__,__LINE__);
	tmp->outer=outer;
	tmp->curline=curline;
	tmp->type=NIL;
	tmp->value=NULL;
	return tmp;
}

AST_atom* newAtom(int type,const char* value,AST_pair* prev)
{
	AST_atom* tmp=NULL;
	if(!(tmp=(AST_atom*)malloc(sizeof(AST_atom))))errors("newAtom",__FILE__,__LINE__);
	switch(type)
	{
		case SYM:
		case STR:
			if(value!=NULL)
			{
				if(!(tmp->value.str=(char*)malloc(strlen(value)+1)))errors("newAtom",__FILE__,__LINE__);
				strcpy(tmp->value.str,value);
			}
			else
				tmp->value.str=NULL;
			break;
		case CHR:
		case IN32:
		case DBL:
			*(int32_t*)(&tmp->value)=0;
			break;
		case BYTS:
			tmp->value.byts.size=0;
			tmp->value.byts.str=NULL;
			break;
	}
	tmp->prev=prev;
	tmp->type=type;
	return tmp;
}

int copyCptr(AST_cptr* objCptr,const AST_cptr* copiedCptr)
{
	if(copiedCptr==NULL||objCptr==NULL)return 0;
	AST_pair* objPair=NULL;
	AST_pair* copiedPair=NULL;
	AST_pair* tmpPair=(copiedCptr->type==PAIR)?copiedCptr->value:NULL;
	copiedPair=tmpPair;
	while(1)
	{
		objCptr->type=copiedCptr->type;
		objCptr->curline=copiedCptr->curline;
		if(copiedCptr->type==PAIR)
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
			AST_atom* coAtm=copiedCptr->value;
			AST_atom* objAtm=NULL;
			if(coAtm->type==SYM||coAtm->type==STR)
				objAtm=newAtom(coAtm->type,coAtm->value.str,objPair);
			else if(coAtm->type==BYTS)
			{
				objAtm=newAtom(coAtm->type,NULL,objPair);
				objAtm->value.byts.size=coAtm->value.byts.size;
				objAtm->value.byts.str=copyMemory(coAtm->value.byts.str,coAtm->value.byts.size);
			}
			else
			{
				objAtm=newAtom(coAtm->type,NULL,objPair);
				if(objAtm->type==DBL)objAtm->value.dbl=coAtm->value.dbl;
				else if(objAtm->type==IN32)objAtm->value.num=coAtm->value.num;
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
			AST_pair* coPrev=NULL;
			if(copiedPair->prev==NULL)break;
			while(objPair->prev!=NULL&&copiedPair!=NULL&&copiedPair!=tmpPair)
			{
				coPrev=copiedPair;
				copiedPair=copiedPair->prev;
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
void replace(AST_cptr* fir,const AST_cptr* sec)
{
	AST_pair* tmp=fir->outer;
	AST_cptr tmpCptr={NULL,0,NIL,NULL};
	tmpCptr.type=fir->type;
	tmpCptr.value=fir->value;
	copyCptr(fir,sec);
	deleteCptr(&tmpCptr);
	if(fir->type==PAIR)((AST_pair*)fir->value)->prev=tmp;
	else if(fir->type==ATM)((AST_atom*)fir->value)->prev=tmp;
}

int deleteCptr(AST_cptr* objCptr)
{
	if(objCptr==NULL)return 0;
	AST_pair* tmpPair=(objCptr->type==PAIR)?objCptr->value:NULL;
	AST_pair* objPair=tmpPair;
	AST_cptr* tmpCptr=objCptr;
	while(tmpCptr!=NULL)
	{
		if(tmpCptr->type==PAIR)
		{
			if(objPair!=NULL&&tmpCptr==&objPair->cdr)
			{
				objPair=objPair->cdr.value;
				tmpCptr=&objPair->car;
				continue;
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
				AST_pair* prev=objPair;
				objPair=objPair->prev;
			//	printf("free PAIR\n");
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

int AST_cptrcmp(const AST_cptr* first,const AST_cptr* second)
{
	if(first==NULL&&second==NULL)return 0;
	AST_pair* firPair=NULL;
	AST_pair* secPair=NULL;
	AST_pair* tmpPair=(first->type==PAIR)?first->value:NULL;
	while(1)
	{
		if(first->type!=second->type)return 0;
		else if(first->type==PAIR)
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
				AST_atom* firAtm=first->value;
				AST_atom* secAtm=second->value;
				if(firAtm->type!=secAtm->type)return 0;
				if((firAtm->type==SYM||firAtm->type==STR)&&strcmp(firAtm->value.str,secAtm->value.str))return 0;
				else if(firAtm->type==IN32&&firAtm->value.num!=secAtm->value.num)return 0;
				else if(firAtm->type==DBL&&fabs(firAtm->value.dbl-secAtm->value.dbl)!=0)return 0;
				else if(firAtm->type==CHR&&firAtm->value.chr!=secAtm->value.chr)return 0;
				else if(firAtm->type==BYTS&&!bytsStrEq(&firAtm->value.byts,&secAtm->value.byts))return 0;
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
			AST_pair* firPrev=NULL;
			if(firPair->prev==NULL)break;
			while(firPair->prev!=NULL&&firPair!=tmpPair)
			{
				firPrev=firPair;
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

AST_cptr* nextCptr(const AST_cptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->cdr.type==PAIR)
		return &((AST_pair*)objCptr->outer->cdr.value)->car;
	return NULL;
}

AST_cptr* prevCptr(const AST_cptr* objCptr)
{
	if(objCptr->outer!=NULL&&objCptr->outer->prev!=NULL&&objCptr->outer->prev->cdr.value==objCptr->outer)
		return &objCptr->outer->prev->car;
	return NULL;
}

int isHexNum(const char* objStr)
{
	if(objStr!=NULL&&strlen(objStr)>2&&objStr[0]=='-'&&toupper(objStr[1])=='X')return 1;
	if(objStr!=NULL&&strlen(objStr)>1&&toupper(objStr[0])=='X')return 1;
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
		if(isHexNum(objStr))
		{
			objStr++;
			format="%x";
		}
		else if(isOctNum(objStr))format="%o";
		else format="%d";
		sscanf(objStr,format,&ch);
	}
	else
	{
		switch(toupper(*(objStr)))
		{
			case 'A':
				ch=0x07;
				break;
			case 'B':
				ch=0x08;
				break;
			case 'T':
				ch=0x09;
				break;
			case 'N':
				ch=0x0a;
				break;
			case 'V':
				ch=0x0b;
				break;
			case 'F':
				ch=0x0c;
				break;
			case 'R':
				ch=0x0d;
				break;
			default:
				ch=*(objStr);
				break;
		}
	}
	return ch;
}

int isNum(const char* objStr)
{
	if(isHexNum(objStr+(objStr[0]=='0')))return 1;
	if(!isdigit(*objStr)&&*objStr!='-'&&*objStr!='.')return 0;
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

void freeAtom(AST_atom* objAtm)
{
	if(objAtm->type==SYM||objAtm->type==STR)free(objAtm->value.str);
	else if(objAtm->type==BYTS)
	{
		objAtm->value.byts.size=0;
		free(objAtm->value.byts.str);
	}
	free(objAtm);
}

void printList(const AST_cptr* objCptr,FILE* out)
{
	if(objCptr==NULL)return;
	AST_pair* tmpPair=(objCptr->type==PAIR)?objCptr->value:NULL;
	AST_pair* objPair=tmpPair;
	while(objCptr!=NULL)
	{
		if(objCptr->type==PAIR)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr)
			{
				putc(' ',out);
				objPair=objPair->cdr.value;
				objCptr=&objPair->car;
			}
			else
			{
				putc('(',out);
				objPair=objCptr->value;
				objCptr=&objPair->car;
				continue;
			}
		}
		else if(objCptr->type==ATM||objCptr->type==NIL)
		{
			if(objPair!=NULL&&objCptr==&objPair->cdr&&objCptr->type==ATM)putc(',',out);
			if((objPair!=NULL&&objCptr==&objPair->car&&objCptr->type==NIL&&objPair->prev!=NULL)
			||(objCptr->outer==NULL&&objCptr->type==NIL))fputs("nil",out);
			if(objCptr->type!=NIL)
			{
				AST_atom* tmpAtm=objCptr->value;
				switch((int)tmpAtm->type)
				{
					case SYM:
						fprintf(out,"%s",tmpAtm->value.str);
						break;
					case STR:
						printRawString(tmpAtm->value.str,out);
						break;
					case IN32:
						fprintf(out,"%d",tmpAtm->value.num);
						break;
					case DBL:
						fprintf(out,"%lf",tmpAtm->value.dbl);
						break;
					case CHR:
						printRawChar(tmpAtm->value.chr,out);
						break;
					case BYTS:
						printByteStr(&tmpAtm->value.byts,out,1);
						break;
				}
			}
			if(objPair!=NULL&&objCptr==&objPair->car)
			{
				objCptr=&objPair->cdr;
				continue;
			}
		}
		if(objPair!=NULL&&objCptr==&objPair->cdr)
		{
			putc(')',out);
			AST_pair* prev=NULL;
			if(objPair->prev==NULL)break;
			while(objPair->prev!=NULL&&objPair!=tmpPair)
			{
				prev=objPair;
				objPair=objPair->prev;
				if(prev==objPair->car.value)break;
			}
			if(objPair!=NULL)objCptr=&objPair->cdr;
			if(objPair==tmpPair&&(prev==objPair->cdr.value||prev==NULL))break;
		}
		if(objPair==NULL)break;
	}
}

void exError(const AST_cptr* obj,int type,Intpr* inter)
{
	if(inter!=NULL)fprintf(stderr,"In file \"%s\",line %d\n",inter->filename,(obj==NULL)?inter->curline:obj->curline);
	if(obj!=NULL)printList(obj,stderr);
	switch(type)
	{
		case SYMUNDEFINE:
			fprintf(stderr,":Symbol is undefined.\n");
			break;
		case SYNTAXERROR:
			fprintf(stderr,":Syntax error.\n");
			break;
		case INVALIDEXPR:
			fprintf(stderr,":Invalid expression here.\n");
			break;
		case CIRCULARLOAD:
			fprintf(stderr,":Circular load file.\n");
			break;
		case INVALIDPATTERN:
			fprintf(stderr,":Invalid string match pattern.\n");
			break;
		case MACROEXPANDFAILED:
			fprintf(stderr,":Failed to expand macro.\n");
			break;
	}
}

void printRawChar(char chr,FILE* out)
{
	if(isgraph(chr))
	{
		if(chr=='\\')
			fprintf(out,"#\\\\\\");
		else
			fprintf(out,"#\\%c",chr);
	}
	else
	{
		uint8_t j=chr;
		fprintf(out,"#\\\\x");
		fprintf(out,"%X",j/16);
		fprintf(out,"%X",j%16);
	}
}

PreEnv* newEnv(PreEnv* prev)
{
	PreEnv* curEnv=NULL;
	if(!(curEnv=(PreEnv*)malloc(sizeof(PreEnv))))errors("newEnv",__FILE__,__LINE__);
	if(prev!=NULL)prev->next=curEnv;
	curEnv->prev=prev;
	curEnv->next=NULL;
	curEnv->symbols=NULL;
	return curEnv;
}

void destroyEnv(PreEnv* objEnv)
{
	if(objEnv==NULL)return;
	while(objEnv!=NULL)
	{
		PreDef* delsym=objEnv->symbols;
		while(delsym!=NULL)
		{
			free(delsym->symbol);
			deleteCptr(&delsym->obj);
			PreDef* prev=delsym;
			delsym=delsym->next;
			free(prev);
		}
		PreEnv* prev=objEnv;
		objEnv=objEnv->next;
		free(prev);
	}
}

Intpr* newIntpr(const char* filename,FILE* file,CompEnv* env,SymbolTable* table,LineNumberTable* lnt)
{
	Intpr* tmp=NULL;
	if(!(tmp=(Intpr*)malloc(sizeof(Intpr))))errors("newIntpr",__FILE__,__LINE__);
	tmp->filename=copyStr(filename);
	if(file!=stdin&&filename!=NULL)
	{
#ifdef _WIN32
		char* rp=_fullpath(NULL,filename,0);
#else
		char* rp=realpath(filename,0);
#endif
		if(rp==NULL)
		{
			perror(filename);
			exit(EXIT_FAILURE);
		}
		tmp->curDir=getDir(rp);
		free(rp);
	}
	else
		tmp->curDir=getcwd(NULL,0);
	tmp->file=file;
	tmp->curline=1;
	tmp->procs=NULL;
	tmp->prev=NULL;
	tmp->modules=NULL;
	tmp->head=NULL;
	tmp->tail=NULL;
	if(table)
		tmp->table=table;
	else
		tmp->table=newSymbolTable();
	if(lnt)
		tmp->lnt=lnt;
	else
		tmp->lnt=newLineNumTable();
	if(env)
	{
		tmp->glob=env;
		return tmp;
	}
	tmp->glob=newCompEnv(NULL);
	initCompEnv(tmp->glob,tmp->table);
	return tmp;
}

void freeIntpr(Intpr* inter)
{
	if(inter->filename)
		free(inter->filename);
	if(inter->file!=stdin)
		fclose(inter->file);
	free(inter->curDir);
	destroyCompEnv(inter->glob);
	deleteAllDll(inter->modules);
	freeModlist(inter->head);
	RawProc* tmp=inter->procs;
	while(tmp!=NULL)
	{
		RawProc* prev=tmp;
		tmp=tmp->next;
		freeByteCode(prev->proc);
		free(prev);
	}
	if(inter->table)freeSymbolTable(inter->table);
	if(inter->lnt)freeLineNumberTable(inter->lnt);
	free(inter);
}

CompEnv* newCompEnv(CompEnv* prev)
{
	CompEnv* tmp=(CompEnv*)malloc(sizeof(CompEnv));
	if(tmp==NULL)errors("newCompEnv",__FILE__,__LINE__);
	tmp->prev=prev;
	tmp->head=NULL;
	return tmp;
}

void destroyCompEnv(CompEnv* objEnv)
{
	if(objEnv==NULL)return;
	CompDef* tmpDef=objEnv->head;
	while(tmpDef!=NULL)
	{
		CompDef* prev=tmpDef;
		tmpDef=tmpDef->next;
		free(prev);
	}
	free(objEnv);
}

CompDef* findCompDef(const char* name,CompEnv* curEnv,SymbolTable* table)
{
	if(curEnv->head==NULL)return NULL;
	else
	{
		SymTabNode* node=findSymbol(name,table);
		if(node==NULL)
			return NULL;
		int32_t id=node->id;
		CompDef* curDef=curEnv->head;
		while(curDef&&id!=curDef->id)
			curDef=curDef->next;
		return curDef;
	}
}

CompDef* addCompDef(const char* name,CompEnv* curEnv,SymbolTable* table)
{
	if(curEnv->head==NULL)
	{
		SymTabNode* node=findSymbol(name,table);
		if(!node)
		{
			node=newSymTabNode(name);
			addSymTabNode(node,table);
		}
		if(!(curEnv->head=(CompDef*)malloc(sizeof(CompDef))))errors("addCompDef",__FILE__,__LINE__);
		curEnv->head->next=NULL;
		curEnv->head->id=node->id;
		return curEnv->head;
	}
	else
	{
		CompDef* curDef=findCompDef(name,curEnv,table);
		if(curDef==NULL)
		{
			SymTabNode* node=findSymbol(name,table);
			if(!node)
			{
				node=newSymTabNode(name);
				addSymTabNode(node,table);
			}
			if(!(curDef=(CompDef*)malloc(sizeof(CompDef))))errors("addCompDef",__FILE__,__LINE__);
			curDef->id=node->id;
			curDef->next=curEnv->head;
			curEnv->head=curDef;
		}
		return curDef;
	}
}

RawProc* newRawProc(int32_t count)
{
	RawProc* tmp=(RawProc*)malloc(sizeof(RawProc));
	if(tmp==NULL)errors("newRawProc",__FILE__,__LINE__);
	tmp->count=count;
	tmp->proc=NULL;
	tmp->prev=NULL;
	tmp->next=NULL;
	return tmp;
}

RawProc* addRawProc(ByteCode* proc,Intpr* inter)
{
	while(inter->prev!=NULL)inter=inter->prev;
	ByteCode* tmp=newByteCode(proc->size);
	memcpy(tmp->code,proc->code,proc->size);
	RawProc* tmpProc=newRawProc((inter->procs==NULL)?0:inter->procs->count+1);
	tmpProc->proc=tmp;
	tmpProc->next=inter->procs;
	if(inter->procs)
		inter->procs->prev=tmpProc;
	inter->procs=tmpProc;
	return tmpProc;
}

ByteCode* newByteCode(unsigned int size)
{
	ByteCode* tmp=NULL;
	if(!(tmp=(ByteCode*)malloc(sizeof(ByteCode))))errors("newByteCode",__FILE__,__LINE__);
	tmp->size=size;
	if(!(tmp->code=(char*)malloc(size*sizeof(char))))errors("newByteCode",__FILE__,__LINE__);
	int32_t i=0;
	for(;i<tmp->size;i++)tmp->code[i]=0;
	return tmp;
}

ByteCodelnt* newByteCodelnt(ByteCode* bc)
{
	ByteCodelnt* t=(ByteCodelnt*)malloc(sizeof(ByteCodelnt));
	if(!t)errors("newByteCodelnt",__FILE__,__LINE__);
	t->ls=0;
	t->l=NULL;
	t->bc=bc;
	return t;
}

void freeByteCodelnt(ByteCodelnt* t)
{
	freeByteCode(t->bc);
	if(t->l)
		free(t->l);
	free(t);
}

void freeByteCode(ByteCode* obj)
{
	free(obj->code);
	free(obj);
}

void codeCat(ByteCode* fir,const ByteCode* sec)
{
	int32_t size=fir->size;
	fir->size=sec->size+fir->size;
	fir->code=(char*)realloc(fir->code,sizeof(char)*fir->size);
	if(!fir->code)errors("codeCat",__FILE__,__LINE__);
	memcpy(fir->code+size,sec->code,sec->size);
}

void reCodeCat(const ByteCode* fir,ByteCode* sec)
{
	int32_t size=fir->size;
	char* tmp=(char*)malloc(sizeof(char)*(fir->size+sec->size));
	if(tmp==NULL)errors("reCodeCat",__FILE__,__LINE__);
	memcpy(tmp,fir->code,fir->size);
	memcpy(tmp+size,sec->code,sec->size);
	free(sec->code);
	sec->code=tmp;
	sec->size=fir->size+sec->size;
}

ByteCode* copyByteCode(const ByteCode* obj)
{
	ByteCode* tmp=newByteCode(obj->size);
	memcpy(tmp->code,obj->code,obj->size);
	return tmp;
}

void initCompEnv(CompEnv* curEnv,SymbolTable* table)
{
	int i=0;
	for(i=0;i<NUMOFBUILTINSYMBOL;i++)
		addCompDef(builtInSymbolList[i],curEnv,table);
}

char* copyStr(const char* str)
{
	if(str==NULL)return NULL;
	char* tmp=(char*)malloc(sizeof(char)*(strlen(str)+1));
	if(tmp==NULL)errors("copyStr",__FILE__,__LINE__);
	strcpy(tmp,str);
	return tmp;

}



int isscript(const char* filename)
{
	int i;
	int len=strlen(filename);
	for(i=len;i>=0;i--)if(filename[i]=='.')break;
	int lenOfExtension=strlen(filename+i);
	if(lenOfExtension!=4)return 0;
	else return !strcmp(filename+i,".fkl");
}

int iscode(const char* filename)
{
	int i;
	int len=strlen(filename);
	for(i=len;i>=0;i--)if(filename[i]=='.')break;
	int lenOfExtension=strlen(filename+i);
	if(lenOfExtension!=5)return 0;
	else return !strcmp(filename+i,".fklc");
}

AST_cptr* getLastCptr(const AST_cptr* objList)
{
	AST_pair* objPair=objList->value;
	AST_cptr* first=&objPair->car;
	for(;nextCptr(first)!=NULL;first=nextCptr(first));
	return first;
}

AST_cptr* getFirstCptr(const AST_cptr* objList)
{
	AST_pair* objPair=objList->value;
	AST_cptr* first=&objPair->car;
	return first;
}

void printByteCode(const ByteCode* tmpCode,FILE* fp)
{
	int32_t i=0;
	while(i<tmpCode->size)
	{
		int tmplen=0;
		fprintf(fp,"%d: %s ",i,codeName[(int)tmpCode->code[i]].codeName);
		switch(codeName[(int)tmpCode->code[i]].len)
		{
			case -3:
				fprintf(fp,"%d %d",*(int32_t*)(tmpCode->code+i+1),*(int32_t*)(tmpCode->code+i+1+sizeof(int32_t)));
				i+=9;
				break;
			case -2:
				fprintf(fp,"%d ",*(int32_t*)(tmpCode->code+i+1));
				printAsByteStr((uint8_t*)(tmpCode->code+i+5),*(int32_t*)(tmpCode->code+i+1),fp);
				i+=5+*(int32_t*)(tmpCode->code+i+1);
				break;
			case -1:
				tmplen=strlen(tmpCode->code+i+1);
				fprintf(fp,"%s",tmpCode->code+i+1);
				i+=tmplen+2;
				break;
			case 0:
				i+=1;
				break;
			case 1:
				printRawChar(tmpCode->code[i+1],fp);
				i+=2;
				break;
			case 4:
				fprintf(fp,"%d",*(int32_t*)(tmpCode->code+i+1));
				i+=5;
				break;
			case 8:
				fprintf(fp,"%lf",*(double*)(tmpCode->code+i+1));
				i+=9;
				break;
		}
		putc('\n',fp);
	}
}

uint8_t castCharInt(char ch)
{
	if(isdigit(ch))return ch-'0';
	else if(isxdigit(ch))
	{
		if(ch>='a'&&ch<='f')return 10+ch-'a';
		else if(ch>='A'&&ch<='F')return 10+ch-'A';
	}
	return 0;
}

uint8_t* castStrByteStr(const char* str)
{
	int len=strlen(str);
	int32_t size=(len%2)?(len/2+1):len/2;
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	if(tmp==NULL)errors("castStrByteStr",__FILE__,__LINE__);
	int32_t i=0;
	int k=0;
	for(;i<size;i++)
	{
		tmp[i]=castCharInt(str[k]);
		if(str[k+1]!='\0')tmp[i]+=16*castCharInt(str[k+1]);
		k+=2;
	}
	return tmp;
}

void printByteStr(const ByteString* obj,FILE* fp,int mode)
{
	if(mode)fputs("#b",fp);
	for(int i=0;i<obj->size;i++)
	{
		uint8_t j=obj->str[i];
		fprintf(fp,"%X",j%16);
		fprintf(fp,"%X",j/16);
	}
}

void printAsByteStr(const uint8_t* str,int32_t size,FILE* fp)
{
	fputs("#b",fp);
	for(int i=0;i<size;i++)
	{
		uint8_t j=str[i];
		fprintf(fp,"%X",j%16);
		fprintf(fp,"%X",j/16);
	}
}

void* copyMemory(void* pm,size_t size)
{
	void* tmp=(void*)malloc(size);
	if(tmp==NULL)errors("copyMemory",__FILE__,__LINE__);
	if(pm!=NULL)
		memcpy(tmp,pm,size);
	return tmp;
}

int hasLoadSameFile(const char* filename,const Intpr* inter)
{
	while(inter!=NULL)
	{
		if(!strcmp(inter->filename,filename))
			return 1;
		inter=inter->prev;
	}
	return 0;
}

RawProc* getHeadRawProc(const Intpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter->procs;
}

Intpr* getFirstIntpr(Intpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter;
}

ByteCode* newDllFuncProc(const char* name)
{
	ByteCode* callProc=newByteCode(sizeof(char)+strlen(name)+1);
	callProc->code[0]=FAKE_CALL_PROC;
	strcpy(callProc->code+sizeof(char),name);
	return callProc;
}

AST_cptr* getANSPairCar(const AST_cptr* obj)
{
	AST_pair* tmpPair=obj->value;
	return &tmpPair->car;
}

AST_cptr* getANSPairCdr(const AST_cptr* obj)
{
	AST_pair* tmpPair=obj->value;
	return &tmpPair->cdr;
}

Dlls* newDll(DllHandle handle)
{
	Dlls* tmp=(Dlls*)malloc(sizeof(Dlls));
	if(tmp==NULL)errors("newDll",__FILE__,__LINE__);
	tmp->handle=handle;
	return tmp;
}

Dlls* loadDll(const char* rpath,Dlls** Dhead,const char* modname,Modlist** tail)
{
#ifdef _WIN32
	DllHandle handle=LoadLibrary(rpath);
	if(!handle)
	{
		TCHAR szBuf[128];
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
				(LPTSTR) &lpMsgBuf,
				0, NULL );
		wsprintf(szBuf,
				_T("%s error Message (error code=%d): %s"),
				_T("CreateDirectory"), dw, lpMsgBuf);
		LocalFree(lpMsgBuf);
		fprintf(stderr,"%s\n",szBuf);
		exit(EXIT_FAILURE);
	}
#else
	DllHandle handle=dlopen(rpath,RTLD_LAZY);
	if(handle==NULL)
	{
		perror(dlerror());
		exit(EXIT_FAILURE);
	}
#endif
	Dlls* tmp=newDll(handle);
	tmp->count=(*Dhead)?(*Dhead)->count+1:0;
	tmp->next=*Dhead;
	*Dhead=tmp;
	if(modname!=NULL&&tail!=NULL)
	{
		Modlist* tmpL=newModList(modname);
		tmpL->count=(*tail)?(*tail)->count+1:0;
		tmpL->next=NULL;
		if(*tail)(*tail)->next=tmpL;
		*tail=tmpL;
	}
	return tmp;
}

void* getAddress(const char* funcname,Dlls* head)
{
	Dlls* cur=head;
	void* pfunc=NULL;
	while(cur!=NULL)
	{
#ifdef _WIN32
		pfunc=GetProcAddress(cur->handle,funcname);
#else
		pfunc=dlsym(cur->handle,funcname);
#endif
		if(pfunc!=NULL)return pfunc;
		cur=cur->next;
	}
	return NULL;
}

void deleteAllDll(Dlls* head)
{
	Dlls* cur=head;
	while(cur)
	{
		Dlls* prev=cur;
#ifdef _WIN32
		FreeLibrary(prev->handle);
#else
		dlclose(prev->handle);
#endif
		cur=cur->next;
		free(prev);
	}
}

Dlls** getHeadOfMods(Intpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return &inter->modules;
}

Modlist* newModList(const char* libname)
{
	Modlist* tmp=(Modlist*)malloc(sizeof(Modlist));
	if(tmp==NULL)errors("newModList",__FILE__,__LINE__);
	tmp->name=(char*)malloc(sizeof(char)*(strlen(libname)+1));
	if(tmp->name==NULL)errors("newModList",__FILE__,__LINE__);
	strcpy(tmp->name,libname);
	return tmp;
}

Dlls* loadAllModules(FILE* fp,Dlls** mods)
{
#ifdef _WIN32
	char* filetype=".dll";
#else
	char* filetype=".so";
#endif
	int32_t num=0;
	int i=0;
	fread(&num,sizeof(int32_t),1,fp);
	for(;i<num;i++)
	{
		char* modname=getStringFromFile(fp);
		char* realModname=(char*)malloc(sizeof(char)*(strlen(modname)+strlen(filetype)+1));
		if(realModname==NULL)errors("loadAllModules",__FILE__,__LINE__);
		strcpy(realModname,modname);
		strcat(realModname,filetype);
#ifdef _WIN32
		char* rpath=_fullpath(NULL,realModname,0);
#else
		char* rpath=realpath(realModname,0);
#endif
		if(rpath==NULL)
		{
			perror(rpath);
			exit(EXIT_FAILURE);
		}
		loadDll(rpath,mods,NULL,NULL);
		free(modname);
		free(rpath);
		free(realModname);
	}
	return *mods;
}

void changeWorkPath(const char* filename)
{
#ifdef _WIN32
	char* p=_fullpath(NULL,filename,0);
#else
	char* p=realpath(filename,NULL);
#endif
	char* wp=getDir(p);
	if(chdir(wp))
	{
		perror(wp);
		exit(EXIT_FAILURE);
	}
	free(p);
	free(wp);
}

char* getDir(const char* filename)
{
#ifdef _WIN32
	char dp='\\';
#else
	char dp='/';
#endif
	int i=strlen(filename)-1;
	for(;filename[i]!=dp;i--);
	char* tmp=(char*)malloc(sizeof(char)*(i+1));
	if(tmp==NULL)errors("getDir",__FILE__,__LINE__);
	tmp[i]='\0';
	memcpy(tmp,filename,i);
	return tmp;
}

char* getStringFromFile(FILE* file)
{
	char* tmp=(char*)malloc(sizeof(char));
	if(tmp==NULL)errors("getStringFromFile",__FILE__,__LINE__);
	tmp[0]='\0';
	char* before;
	int i=0;
	char ch;
	int j;
	while((ch=getc(file))!=EOF&&ch!='\0')
	{
		i++;
		j=i-1;
		before=tmp;
		if(!(tmp=(char*)malloc(sizeof(char)*(i+1))))errors("getStringFromFile",__FILE__,__LINE__);
		if(before!=NULL)
		{
			memcpy(tmp,before,j);
			free(before);
		}
		*(tmp+j)=ch;
	}
	if(tmp!=NULL)tmp[i]='\0';
	return tmp;
}

void writeAllDll(Intpr* inter,FILE* fp)
{
	int num=(inter->head==NULL)?0:inter->tail->count+1;
	fwrite(&num,sizeof(int32_t),1,fp);
	int i=0;
	Modlist* cur=inter->head;
	for(;i<num;i++)
	{
		fwrite(cur->name,strlen(cur->name)+1,1,fp);
		cur=cur->next;
	}
}

void freeAllRawProc(RawProc* cur)
{
	while(cur!=NULL)
	{
		freeByteCode(cur->proc);
		RawProc* prev=cur;
		cur=cur->next;
		free(prev);
	}
}

int bytsStrEq(ByteString* fir,ByteString* sec)
{
	if(fir->size!=sec->size)return 0;
	else return !memcmp(fir->str,sec->str,sec->size);
}

int ModHasLoad(const char* name,Modlist* head)
{
	while(head)
	{
		if(!strcmp(name,head->name))return 1;
		head=head->next;
	}
	return 0;
}

Dlls** getpDlls(Intpr* inter)
{
	while(inter->prev)
		inter=inter->prev;
	return &inter->modules;
}

Modlist** getpTail(Intpr* inter)
{
	while(inter->prev)
		inter=inter->prev;
	return &inter->tail;
}

Modlist** getpHead(Intpr* inter)
{
	while(inter->prev)
		inter=inter->prev;
	return &inter->head;
}

char* getLastWorkDir(Intpr* inter)
{
	while(inter->prev!=NULL)
		inter=inter->prev;
	return inter->curDir;
}

char* relpath(char* abs,char* relto)
{
#ifdef _WIN32
	char* divstr="\\";
	char* upperDir="..\\";
#else
	char* divstr="/";
	char* upperDir="../";
#endif
	char* cabs=copyStr(abs);
	char* crelto=copyStr(relto);
	int lengthOfAbs=0;
	int lengthOfRelto=0;
	char** absDirs=split(cabs,divstr,&lengthOfAbs);
	char** reltoDirs=split(crelto,divstr,&lengthOfRelto);
	int length=(lengthOfAbs<lengthOfRelto)?lengthOfAbs:lengthOfRelto;
	int lastCommonRoot=-1;
	int index;
	for(index=0;index<length;index++)
	{
		if(!strcmp(absDirs[index],reltoDirs[index]))
			lastCommonRoot=index;
		else break;
	}
#ifdef _WIN32
	if(lastCommonRoot==-1)
	{
		fprintf(stderr,"%s:Cant get relative path.\n",abs);
		exit(EXIT_FAILURE);
	}
#endif
	char rp[PATH_MAX]={0};
	for(index=lastCommonRoot+1;index<lengthOfAbs;index++)
		if(lengthOfAbs>0)
			strcat(rp,upperDir);
	for(index=lastCommonRoot+1;index<lengthOfRelto-1;index++)
	{
		strcat(rp,reltoDirs[index]);
#ifdef _WIN32
		strcat(rp,"\\");
#else
		strcat(rp,"/");
#endif
	}
	if(reltoDirs!=NULL)
		strcat(rp,reltoDirs[lengthOfRelto-1]);
	char* trp=copyStr(rp);
	free(cabs);
	free(crelto);
	free(absDirs);
	free(reltoDirs);
	return trp;
}

char** split(char* str,char* divstr,int* length)
{
	int count=0;
	char* pNext=NULL;
	char** strArry=(char**)malloc(0);
	if(strArry==NULL)errors("split",__FILE__,__LINE__);
	pNext=strtok(str,divstr);
	while(pNext!=NULL)
	{
		count++;
		strArry=(char**)realloc(strArry,sizeof(char*)*count);
		strArry[count-1]=pNext;
		pNext=strtok(NULL,divstr);
	}
	*length=count;
	return strArry;
}

void freeModlist(Modlist* cur)
{
	while(cur)
	{
		Modlist* prev=cur;
		cur=cur->next;
		free(prev->name);
		free(prev);
	}
}

char* castEscapeCharater(const char* str,char end,int32_t* len)
{
	int32_t strSize=0;
	int32_t memSize=MAX_STRING_SIZE;
	int32_t i=0;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	while(str[i]!=end)
	{
		int ch=0;
		if(str[i]=='\\')
		{
			if(isdigit(str[i+1]))
			{
				if(str[i+1]=='0')
				{
					if(isdigit(str[i+2]))
					{
						int len=0;
						while((isdigit(str[i+2+len])&&(str[i+2+len]<'8')&&len<4))len++;
						sscanf(str+i+1,"%4o",&ch);
						i+=len+2;
					}
				}
				else
				{
					int len=0;
					while(isdigit(str[i+1+len])&&len<4)len++;
					sscanf(str+i+1,"%4d",&ch);
					i+=len+1;
				}
			}
			else if(toupper(str[i+1])=='X')
			{
				int len=0;
				while(isxdigit(str[i+2+len])&&len<2)len++;
				sscanf(str+i+1,"%4x",&ch);
				i+=len+2;
			}
			else if(str[i+1]=='\n')
			{
				i+=2;
				continue;
			}
			else
			{
				switch(toupper(str[i+1]))
				{
					case 'A':
						ch=0x07;
						break;
					case 'B':
						ch=0x08;
						break;
					case 'T':
						ch=0x09;
						break;
					case 'N':
						ch=0x0a;
						break;
					case 'V':
						ch=0x0b;
						break;
					case 'F':
						ch=0x0c;
						break;
					case 'R':
						ch=0x0d;
						break;
					default:ch=str[i+1];break;
				}
				i+=2;
			}
		}
		else ch=str[i++];
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+MAX_STRING_SIZE));
			if(!tmp)errors("castKeyStringToNormalString",__FILE__,__LINE__);
			memSize+=MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
	}
	if(tmp)tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,memSize*sizeof(char));
	if(!tmp)errors("castKeyStringToNormalString",__FILE__,__LINE__);
	*len=i+1;
	return tmp;
}

Intpr* newTmpIntpr(const char* filename,FILE* fp)
{
	Intpr* tmp=NULL;
	if(!(tmp=(Intpr*)malloc(sizeof(Intpr))))errors("newTmpIntpr",__FILE__,__LINE__);
	tmp->filename=copyStr(filename);
	if(fp!=stdin&&filename)
	{
#ifdef _WIN32
		char* rp=_fullpath(NULL,filename,0);
#else
		char* rp=realpath(filename,0);
#endif
		if(rp==NULL)
		{
			perror(filename);
			exit(EXIT_FAILURE);
		}
		tmp->curDir=getDir(rp);
		free(rp);
	}
	else
		tmp->curDir=NULL;
	tmp->file=fp;
	tmp->curline=1;
	tmp->procs=NULL;
	tmp->prev=NULL;
	tmp->modules=NULL;
	tmp->head=NULL;
	tmp->tail=NULL;
	tmp->glob=NULL;
	tmp->table=NULL;
	tmp->lnt=NULL;
	return tmp;
}

ByteCode* castRawproc(ByteCode* prev,RawProc* procs)
{
	if(procs==NULL)return NULL;
	else
	{
		ByteCode* tmp=(ByteCode*)realloc(prev,sizeof(ByteCode)*(procs->count+1));
		if(tmp==NULL)
		{
			fprintf(stderr,"In file \"%s\",line %d\n",__FILE__,__LINE__);
			errors("castRawproc",__FILE__,__LINE__);
		}
		RawProc* curRawproc=procs;
		while(curRawproc!=NULL)
		{
			tmp[curRawproc->count]=*curRawproc->proc;
			curRawproc=curRawproc->next;
		}
		return tmp;
	}
}

void freeRawProc(ByteCode* l,int32_t num)
{
	int i=0;
	for(;i<num;i++)
		free(l[i].code);
	free(l);
}

int32_t countChar(const char* str,char c,int32_t len)
{
	int32_t num=0;
	int32_t i=0;
	if(len==-1)
	{
		for(;str[i]!='\0';i++)
			if(str[i]==c)
				num++;
	}
	else
	{
		for(;i<len;i++)
			if(str[i]==c)
				num++;
	}
	return num;
}

PreDef* findDefine(const char* name,const PreEnv* curEnv)
{
	if(curEnv->symbols==NULL)return NULL;
	else
	{
		PreDef* curDef=curEnv->symbols;
		while(curDef&&strcmp(name,curDef->symbol))
			curDef=curDef->next;
		return curDef;
	}
}

PreDef* addDefine(const char* symbol,const AST_cptr* objCptr,PreEnv* curEnv)
{
	if(curEnv->symbols==NULL)
	{
		curEnv->symbols=newDefines(symbol);
		replace(&curEnv->symbols->obj,objCptr);
		curEnv->symbols->next=NULL;
		return curEnv->symbols;
	}
	else
	{
		PreDef* curDef=findDefine(symbol,curEnv);
		if(curDef==NULL)
		{
			curDef=newDefines(symbol);
			curDef->next=curEnv->symbols;
			curEnv->symbols=curDef;
			replace(&curDef->obj,objCptr);
		}
		else
			replace(&curDef->obj,objCptr);
		return curDef;
	}
}

PreDef* newDefines(const char* name)
{
	PreDef* tmp=(PreDef*)malloc(sizeof(PreDef));
	if(tmp==NULL)errors("newDefines",__FILE__,__LINE__);
	tmp->symbol=(char*)malloc(sizeof(char)*(strlen(name)+1));
	if(tmp->symbol==NULL)errors("newDefines",__FILE__,__LINE__);
	strcpy(tmp->symbol,name);
	tmp->obj=(AST_cptr){NULL,0,NIL,NULL};
	tmp->next=NULL;
	return tmp;
}

SymbolTable* newSymbolTable()
{
	SymbolTable* tmp=(SymbolTable*)malloc(sizeof(SymbolTable));
	if(!tmp)
		errors("newSymbolTable",__FILE__,__LINE__);
	tmp->list=NULL;
	tmp->idl=NULL;
	tmp->size=0;
	return tmp;
}

SymTabNode* newSymTabNode(const char* symbol)
{
	SymTabNode* tmp=(SymTabNode*)malloc(sizeof(SymTabNode));
	if(!tmp)
		errors("newSymTabNode",__FILE__,__LINE__);
	tmp->id=0;
	tmp->symbol=copyStr(symbol);
	return tmp;
}

SymTabNode* addSymTabNode(SymTabNode* node,SymbolTable* table)
{
	if(!table->list)
	{
		table->size=1;
		node->id=table->size-1;
		table->list=(SymTabNode**)malloc(sizeof(SymTabNode*)*1);
		if(!table->list)errors("addSymTabNode",__FILE__,__LINE__);
		table->idl=(SymTabNode**)malloc(sizeof(SymTabNode*)*1);
		if(!table->idl)errors("addSymTabNode",__FILE__,__LINE__);
		table->list[0]=node;
		table->idl[0]=node;
	}
	else
	{
		int32_t l=0;
		int32_t h=table->size-1;
		int32_t mid;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			if(strcmp(table->list[mid]->symbol,node->symbol)>=0)
				h=mid-1;
			else
				l=mid+1;
		}
		if(strcmp(table->list[mid]->symbol,node->symbol)<=0)
			mid++;
		table->size+=1;
		int32_t i=table->size-1;
		table->list=(SymTabNode**)realloc(table->list,sizeof(SymTabNode*)*table->size);
		if(!table->list)errors("addSymTabNode",__FILE__,__LINE__);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->size-1;
		table->idl=(SymTabNode**)realloc(table->idl,sizeof(SymTabNode*)*table->size);
		if(!table->idl)errors("addSymTabNode",__FILE__,__LINE__);
		table->idl[table->size-1]=node;
	}
	return node;
}

void freeSymTabNode(SymTabNode* node)
{
	free(node->symbol);
	free(node);
}

void freeSymbolTable(SymbolTable* table)
{
	int32_t i=0;
	for(;i<table->size;i++)
		freeSymTabNode(table->list[i]);
	free(table->list);
	free(table->idl);
	free(table);
}

SymTabNode* findSymbol(const char* symbol,SymbolTable* table)
{
	if(!table->list)
		return NULL;
	int32_t l=0;
	int32_t h=table->size-1;
	int32_t mid;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int resultOfCmp=strcmp(table->list[mid]->symbol,symbol);
		if(resultOfCmp>0)
			h=mid-1;
		else if(resultOfCmp<0)
			l=mid+1;
		else
			return table->list[mid];
	}
	return NULL;
}

void printSymbolTable(SymbolTable* table,FILE* fp)
{
	int32_t i=0;
	for(;i<table->size;i++)
	{
		SymTabNode* cur=table->list[i];
		printf("symbol:%s id:%d\n",cur->symbol,cur->id);
	}
	printf("size:%d\n",table->size);
}

AST_cptr* baseCreateTree(const char* objStr,Intpr* inter)
{
	if(!objStr)return NULL;
	size_t i=0;
	int braketsNum=0;
	AST_cptr* root=NULL;
	AST_pair* objPair=NULL;
	AST_cptr* objCptr;
	while(*(objStr+i)!='\0')
	{
		if(*(objStr+i)=='(')
		{
			if(i!=0&&*(objStr+i-1)==')')
			{
				if(objPair!=NULL)
				{
					int curline=(inter)?inter->curline:0;
					AST_pair* tmp=newPair(curline,objPair);
					objPair->cdr.type=PAIR;
					objPair->cdr.value=(void*)tmp;
					objPair=tmp;
					objCptr=&objPair->car;
				}
			}
			i++;
			braketsNum++;
			if(root==NULL)
			{
				int curline=(inter)?inter->curline:0;
				root=newCptr(curline,objPair);
				root->type=PAIR;
				root->value=newPair(curline,NULL);
				objPair=root->value;
				objCptr=&objPair->car;
			}
			else
			{
				int curline=(inter)?inter->curline:0;
				objPair=newPair(curline,objPair);
				objCptr->type=PAIR;
				objCptr->value=(void*)objPair;
				objCptr=&objPair->car;
			}
		}
		else if(*(objStr+i)==')')
		{
			i++;
			braketsNum--;
			AST_pair* prev=NULL;
			if(objPair==NULL)break;
			while(objPair->prev!=NULL)
			{
				prev=objPair;
				objPair=objPair->prev;
				if(objPair->car.value==prev)break;
			}
			if(objPair->car.value==prev)
				objCptr=&objPair->car;
			else if(objPair->cdr.value==prev)
				objCptr=&objPair->cdr;
		}
		else if(*(objStr+i)==',')
		{
			if(objCptr==&objPair->cdr)
			{
				if(root)
					deleteCptr(root);
				free(root);
				return NULL;
			}
			i++;
			objCptr=&objPair->cdr;
		}
		else if(isspace(*(objStr+i)))
		{
			int j=0;
			char* tmpStr=(char*)objStr+i;
			while(j>(int)-i&&isspace(*(tmpStr+j)))j--;
			if(*(tmpStr+j)==','||*(tmpStr+j)=='(')
			{
				j=1;
				while(isspace(*(tmpStr+j)))j++;
				i+=j;
				continue;
			}
			j=0;
			while(isspace(*(tmpStr+j)))
			{
				if(*(tmpStr+j)=='\n')
				{
					if(inter)
						inter->curline+=1;
				}
				j++;
			}
			if(*(tmpStr+j)==','||*(tmpStr+j)==')')
			{
				i+=j;
				continue;
			}
			i+=j;
			if(objPair!=NULL)
			{
				int curline=(inter)?inter->curline:0;
				AST_pair* tmp=newPair(curline,objPair);
				objPair->cdr.type=PAIR;
				objPair->cdr.value=(void*)tmp;
				objPair=tmp;
				objCptr=&objPair->car;
			}
		}
		else if(*(objStr+i)=='\"')
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			int32_t len=0;
			char* tmpStr=castEscapeCharater(objStr+i+1,'\"',&len);
			inter->curline+=countChar(objStr+i,'\n',len);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(STR,tmpStr,objPair);
			i+=len+1;
			free(tmpStr);
		}
		else if(isdigit(*(objStr+i))||(*(objStr+i)=='-'&&(/**(objStr+i)&&*/isdigit(*(objStr+i+1)))))
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			char* tmp=getStringFromList(objStr+i);
			AST_atom* tmpAtm=NULL;
			if(!isNum(tmp))
				tmpAtm=newAtom(SYM,tmp,objPair);
			else if(isDouble(tmp))
			{
				tmpAtm=newAtom(DBL,NULL,objPair);
				double num=stringToDouble(tmp);
				tmpAtm->value.dbl=num;
			}
			else
			{
				tmpAtm=newAtom(IN32,NULL,objPair);
				int32_t num=stringToInt(tmp);
				tmpAtm->value.num=num;
			}
			objCptr->type=ATM;
			objCptr->value=tmpAtm;
			i+=strlen(tmp);
			free(tmp);
		}
		else if(*(objStr+i)=='#'&&(/**(objStr+i)&&*/*(objStr+1+i)=='\\'))
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			char* tmp=getStringAfterBackslash(objStr+i+2);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(CHR,NULL,objPair);
			AST_atom* tmpAtm=objCptr->value;
			if(tmp[0]!='\\')tmpAtm->value.chr=tmp[0];
			else tmpAtm->value.chr=stringToChar(tmp+1);
			i+=strlen(tmp)+2;
			free(tmp);
		}
		else if(*(objStr+i)=='#'&&(/**(objStr+i)&&*/*(objStr+1+i)=='b'))
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			char* tmp=getStringAfterBackslash(objStr+i+2);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(BYTS,NULL,objPair);
			AST_atom* tmpAtm=objCptr->value;
			int32_t size=strlen(tmp)/2+strlen(tmp)%2;
			tmpAtm->value.byts.refcount=0;
			tmpAtm->value.byts.size=size;
			tmpAtm->value.byts.str=castStrByteStr(tmp);
			i+=strlen(tmp)+2;
			free(tmp);
		}
		else
		{
			int curline=(inter)?inter->curline:0;
			if(root==NULL)objCptr=root=newCptr(curline,objPair);
			char* tmp=getStringFromList(objStr+i);
			objCptr->type=ATM;
			objCptr->value=(void*)newAtom(SYM,tmp,objPair);
			i+=strlen(tmp);
			free(tmp);
		}
		if(braketsNum<=0&&root!=NULL)break;
	}
	if(braketsNum)
	{
		if(root)
		{
			deleteCptr(root);
			free(root);
		}
		return NULL;
	}
	return root;
}

void writeSymbolTable(SymbolTable* table,FILE* fp)
{
	int32_t size=table->size;
	fwrite(&size,sizeof(size),1,fp);
	int32_t i=0;
	for(;i<size;i++)
		fwrite(table->idl[i]->symbol,strlen(table->idl[i]->symbol)+1,1,fp);
}

void writeLineNumberTable(LineNumberTable* lnt,FILE* fp)
{
	int32_t size=lnt->size;
	fwrite(&size,sizeof(size),1,fp);
	int32_t i=0;
	for(;i<size;i++)
	{
		LineNumTabId* idl=lnt->list[i];
		int32_t size=idl->size;
		int32_t j=0;
		fwrite(&idl->id,sizeof(idl->id),1,fp);
		fwrite(&size,sizeof(size),1,fp);
		for(;j<size;j++)
		{
			LineNumTabNode* n=idl->list[j];
			fwrite(&n->fid,sizeof(n->fid),1,fp);
			fwrite(&n->scp,sizeof(n->scp),1,fp);
			fwrite(&n->cpc,sizeof(n->cpc),1,fp);
			fwrite(&n->line,sizeof(n->line),1,fp);
		}
	}
}

int isAllSpace(const char* str)
{
	for(;*str;str++)
		if(!isspace(*str))
			return 0;
	return 1;
}

LineNumberTable* newLineNumTable()
{
	LineNumberTable* t=(LineNumberTable*)malloc(sizeof(LineNumberTable));
	if(!t)
		errors("newLineNumTable",__FILE__,__LINE__);
	t->size=0;
	t->list=NULL;
	return t;
}

LineNumTabId* newLineNumTabId(int32_t id)
{
	LineNumTabId* t=(LineNumTabId*)malloc(sizeof(LineNumTabId));
	if(!t)
		errors("newLineNumTabId",__FILE__,__LINE__);
	t->id=id;
	t->size=0;
	t->list=NULL;
	return t;
}

LineNumTabNode* newLineNumTabNode(int32_t fid,int32_t scp,int32_t cpc,int32_t line)
{
	LineNumTabNode* t=(LineNumTabNode*)malloc(sizeof(LineNumTabNode));
	if(!t)
		errors("newLineNumNode",__FILE__,__LINE__);
	t->fid=fid;
	t->scp=scp;
	t->cpc=cpc;
	t->line=line;
	return t;
}

LineNumTabId* addLineNumTabId(LineNumTabNode** list,int32_t size,int32_t id,LineNumberTable* table)
{
	if(!table->list)
	{
		table->size=1;
		table->list=(LineNumTabId**)malloc(sizeof(LineNumTabId*)*1);
		if(!table->list)
			errors("addLineNumTabId",__FILE__,__LINE__);
		LineNumTabId* i=newLineNumTabId(id);
		i->size=size;
		i->list=list;
		table->list[0]=i;
		return i;
	}
	else
	{
//		{
//			fprintf(stderr,"----\n");
//			int32_t i=0;
//			for(;i<table->size;i++)
//				fprintf(stderr,"%d\n",table->list[i]->id);
//		}
		int32_t l=0;
		int32_t h=table->size-1;
		int32_t mid;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			if(table->list[mid]->id>=id)
				h=mid-1;
			else
				l=mid+1;
		}
		if(table->list[mid]->id==id)
		{
			int32_t i=0;
			LineNumTabId* idl=table->list[mid];
			for(;i<idl->size;i++)
				freeLineNumTabNode(idl->list[i]);
			free(idl->list);
			idl->size=size;
			idl->list=list;
			return idl;
		}
		if(table->list[mid]->id<=id)
			mid++;
		table->size+=1;
		int32_t i=table->size-1;
		table->list=(LineNumTabId**)realloc(table->list,sizeof(LineNumTabId*)*table->size);
		if(!table->list)
			errors("addLineNumTabId",__FILE__,__LINE__);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		LineNumTabId* idl=newLineNumTabId(id);
		idl->size=size;
		idl->list=list;
		table->list[mid]=idl;
		return idl;
	}
}

void freeLineNumTabNode(LineNumTabNode* n)
{
	free(n);
}

void freeLineNumTabId(LineNumTabId* idt)
{
	int32_t i=0;
	for(;i<idt->size;i++)
		freeLineNumTabNode(idt->list[i]);
	free(idt->list);
	free(idt);
}

void freeLineNumberTable(LineNumberTable* t)
{
	int32_t i=0;
	for(;i<t->size;i++)
		freeLineNumTabId(t->list[i]);
	free(t->list);
	free(t);
}

LineNumTabNode* findLineNumTabNode(int32_t id,int32_t cp,LineNumberTable* t)
{
	int32_t i=0;
	LineNumTabId* idt=t->list[id];
	for(;i<idt->size;i++)
	{
		if(idt->list[i]->scp<=cp&&(idt->list[i]->scp+idt->list[i]->cpc)>=cp)
			return idt->list[i];
	}
	return NULL;
}

void increaseScpOfByteCodelnt(ByteCodelnt* o,int32_t size)
{
	int32_t i=0;
	for(;i<o->ls;i++)
		o->l[i]->scp+=size;
}

void codelntCat(ByteCodelnt* f,ByteCodelnt* s)
{
	if(!f->l)
	{
		f->ls=s->ls;
		f->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*s->ls);
		if(!f->l)
			errors("codelntCat",__FILE__,__LINE__);
		s->l[0]->cpc+=f->bc->size;
		INCREASE_ALL_SCP(s->l+1,s->ls-1,f->bc->size);
		memcpy(f->l,s->l,(s->ls)*sizeof(LineNumTabNode*));
	}
	else
	{
		INCREASE_ALL_SCP(s->l,s->ls,f->bc->size);
		if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
		{
			f->l[f->ls-1]->cpc+=s->l[0]->cpc;
			f->l=(LineNumTabNode**)realloc(f->l,sizeof(LineNumTabNode*)*(f->ls+s->ls-1));
			if(!f->l)
				errors("codelntCat",__FILE__,__LINE__);
			memcpy(f->l+f->ls,s->l+1,(s->ls-1)*sizeof(LineNumTabNode*));
			f->ls+=s->ls-1;
			freeLineNumTabNode(s->l[0]);
		}
		else
		{
			f->l=(LineNumTabNode**)realloc(f->l,sizeof(LineNumTabNode*)*(f->ls+s->ls));
			if(!f->l)
				errors("codelntCat",__FILE__,__LINE__);
			memcpy(f->l+f->ls,s->l,(s->ls)*sizeof(LineNumTabNode*));
			f->ls+=s->ls;
		}
	}
	codeCat(f->bc,s->bc);
}

void reCodelntCat(ByteCodelnt* f,ByteCodelnt* s)
{
	if(!s->l)
	{
		s->ls=f->ls;
		s->l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*f->ls);
		if(!s->l)
			errors("reCodelntCat",__FILE__,__LINE__);
		f->l[f->ls-1]->cpc+=s->bc->size;
		memcpy(s->l,f->l,(f->ls)*sizeof(LineNumTabNode*));
	}
	else
	{
		INCREASE_ALL_SCP(s->l,s->ls,f->bc->size);
		if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
		{
			LineNumTabNode** l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*(f->ls+s->ls-1));
			if(!l)
				errors("reCodeCat",__FILE__,__LINE__);
			f->l[f->ls-1]->cpc+=s->l[0]->cpc;
			freeLineNumTabNode(s->l[0]);
			memcpy(l,f->l,(f->ls)*sizeof(LineNumTabNode*));
			memcpy(l+f->ls,s->l+1,(s->ls-1)*sizeof(LineNumTabNode*));
			free(s->l);
			s->l=l;
			s->ls+=f->ls-1;
		}
		else
		{
			LineNumTabNode** l=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*(f->ls+s->ls));
			if(!l)
				errors("reCodeCat",__FILE__,__LINE__);
			memcpy(l,f->l,(f->ls)*sizeof(LineNumTabNode*));
			memcpy(l+f->ls,s->l,(s->ls)*sizeof(LineNumTabNode*));
			free(s->l);
			s->l=l;
			s->ls+=f->ls;
		}
	}
	reCodeCat(f->bc,s->bc);
}

void printByteCodelnt(ByteCodelnt* obj,SymbolTable* table,FILE* fp)
{
	ByteCode* tmpCode=obj->bc;
	int32_t i=0;
	int32_t j=0;
	int32_t fid=0;
	int32_t line=0;
	while(i<tmpCode->size)
	{
		int tmplen=0;
		fprintf(fp,"%d: %s ",i,codeName[(int)tmpCode->code[i]].codeName);
		switch(codeName[(int)tmpCode->code[i]].len)
		{
			case -3:
				fprintf(fp,"%d %d",*(int32_t*)(tmpCode->code+i+1),*(int32_t*)(tmpCode->code+i+1+sizeof(int32_t)));
				i+=9;
				break;
			case -2:
				fprintf(fp,"%d ",*(int32_t*)(tmpCode->code+i+1));
				printAsByteStr((uint8_t*)(tmpCode->code+i+5),*(int32_t*)(tmpCode->code+i+1),fp);
				i+=5+*(int32_t*)(tmpCode->code+i+1);
				break;
			case -1:
				tmplen=strlen(tmpCode->code+i+1);
				fprintf(fp,"%s",tmpCode->code+i+1);
				i+=tmplen+2;
				break;
			case 0:
				i+=1;
				break;
			case 1:
				printRawChar(tmpCode->code[i+1],fp);
				i+=2;
				break;
			case 4:
				fprintf(fp,"%d",*(int32_t*)(tmpCode->code+i+1));
				i+=5;
				break;
			case 8:
				fprintf(fp,"%lf",*(double*)(tmpCode->code+i+1));
				i+=9;
				break;
		}
		if(obj->l[j]->scp+obj->l[j]->cpc<i)
			j++;
		putc('\t',fp);
		if(obj->l[j]->fid!=fid||obj->l[j]->line!=line)
		{
			fid=obj->l[j]->fid;
			line=obj->l[j]->line;
			fprintf(fp,"%s:%d:%d",table->idl[obj->l[j]->fid]->symbol,obj->l[j]->line,obj->l[j]->cpc);
		}
		putc('\n',fp);
	}
}

ByteCodeLabel* newByteCodeLable(int32_t place,const char* label)
{
	ByteCodeLabel* tmp=(ByteCodeLabel*)malloc(sizeof(ByteCodeLabel));
	if(!tmp)
		errors("newByteCodeLable",__FILE__,__LINE__);
	tmp->place=place;
	tmp->label=copyStr(label);
	tmp->next=NULL;
	return tmp;
}

ByteCodeLabel* findByteCodeLable(const char* label,ByteCodeLabel* head)
{
	while(head)
	{
		if(!strcmp(label,head->label))
			return head;
		head=head->next;
	}
	return NULL;
}

void addByteCodeLabel(ByteCodeLabel* obj,ByteCodeLabel** head)
{
	obj->next=*head;
	*head=obj;
}

void freeByteCodeLabel(ByteCodeLabel* obj)
{
	free(obj->label);
	free(obj);
}

void destroyByteCodeLabelChain(ByteCodeLabel* head)
{
	while(head)
	{
		ByteCodeLabel* obj=head;
		head=head->next;
		freeByteCodeLabel(obj);
	}
}
