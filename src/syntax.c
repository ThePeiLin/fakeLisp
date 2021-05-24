#include"syntax.h"
#include"common.h"
#include<stdlib.h>
#include<string.h>

int isPreprocess(const AST_cptr* objCptr)
{
	if(isDefmacroExpression(objCptr))return 1;
	return 0;
}

int isDefmacroExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||nextCptr(objCptr)->type==NIL)return 0;
		else
		{
			AST_atom* tmpAtm=objCptr->value;
			if(tmpAtm->type!=SYM||strcmp(tmpAtm->value.str,"defmacro"))return 0;
		}
		return 1;
	}
	return 0;
}

int isDefExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||nextCptr(objCptr)->type!=ATM)return 0;
		else
		{
			AST_atom* first=objCptr->value;
			AST_atom* second=nextCptr(objCptr)->value;
			if(first->type!=SYM||second->type!=SYM||strcmp(first->value.str,"define"))return 0;
		}
		return 1;
	}
	else return 0;
}

int isSetqExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM||nextCptr(objCptr)->type!=ATM)return 0;
		else
		{
			AST_atom* first=objCptr->value;
			AST_atom* second=nextCptr(objCptr)->value;
			if(first->type!=SYM||second->type!=SYM||strcmp(first->value.str,"setq"))return 0;
		}
		return 1;
	}
	else return 0;
}

int isSetfExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL&&nextCptr(objCptr)!=NULL&&nextCptr(nextCptr(objCptr))!=NULL)
	{
		if(objCptr->type!=ATM)return 0;
		else
		{
			AST_atom* first=objCptr->value;
			if(first->type!=SYM||strcmp(first->value.str,"setf"))return 0;
		}
		return 1;
	}
	else return 0;
}

int isCondExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		if(objCptr->type==ATM)
		{
			AST_atom* tmpAtm=objCptr->value;
			if(tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"cond"))
			{
				for(objCptr=nextCptr(objCptr);objCptr!=NULL;objCptr=nextCptr(objCptr))
					if(!isValid(objCptr)||objCptr->type!=PAIR)return 0;
				return 1;
			}
		}
	}
	return 0;
}

int isSymbol(const AST_cptr* objCptr)
{
	AST_atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm==NULL||tmpAtm->type!=SYM)return 0;
	return 1;
}

int isNil(const AST_cptr* objCptr)
{
	if(objCptr->type==NIL)return 1;
	else if(objCptr->type==PAIR)
	{
		AST_pair* tmpPair=objCptr->value;
		if(tmpPair->car.type==NIL&&tmpPair->cdr.type==NIL)return 1;
	}
	return 0;
}

int isConst(const AST_cptr* objCptr)
{
	if(isNil(objCptr))return 1;
	AST_atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type!=SYM)return 1;
	if(objCptr->type==PAIR&&(isQuoteExpression(objCptr)||(((AST_pair*)objCptr->value)->car.type==NIL&&((AST_pair*)objCptr->value)->car.type==NIL&&isValid(objCptr))))return 1;
	return 0;
}

int isAndExpression(const AST_cptr* objCptr)
{
	if(!isValid(objCptr))return 0;
	objCptr=(objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		AST_atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"and"))return 1;
	}
	return 0;
}

int isOrExpression(const AST_cptr* objCptr)
{
	if(!isValid(objCptr))return 0;
	objCptr=(objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	if(objCptr!=NULL)
	{
		AST_atom* tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
		if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"or"))return 1;
	}
	return 0;
}

int isQuoteExpression(const AST_cptr* objCptr)
{
	objCptr=(isValid(objCptr)&&objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"quote"))return 1;
	return 0;
}

int isUnquoteExpression(const AST_cptr* objCptr)
{
	objCptr=(isValid(objCptr)&&objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"unquote"))return 1;
	return 0;
}

int isQsquoteExpression(const AST_cptr* objCptr)
{
	objCptr=(isValid(objCptr)&&objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"qsquote"))return 1;
	return 0;
}

int isUnqtespExpression(const AST_cptr* objCptr)
{
	objCptr=(isValid(objCptr)&&objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"unqtesp"))return 1;
	return 0;
}

int isLambdaExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"lambda"))return 1;
	return 0;
}

int isBeginExpression(const AST_cptr* objCptr)
{
	objCptr=(objCptr->type==PAIR)?&((AST_pair*)objCptr->value)->car:NULL;
	AST_atom* tmpAtm=(objCptr!=NULL&&objCptr->type==ATM)?objCptr->value:NULL;
	if(tmpAtm!=NULL&&tmpAtm->type==SYM&&!strcmp(tmpAtm->value.str,"begin"))return 1;
	return 0;
}

int isFuncCall(const AST_cptr* objCptr,CompEnv* curEnv)
{
	if(objCptr->type==PAIR&&isValid(objCptr)&&!hasKeyWord(objCptr,curEnv)&&!isNil(objCptr))return 1;
	return 0;
}

int isValid(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		objCptr=&((AST_pair*)objCptr->value)->car;
		const AST_cptr* prevCptr=NULL;
		while(objCptr!=NULL)
		{
			prevCptr=objCptr;
			objCptr=nextCptr(objCptr);
		}
		if(prevCptr->outer->cdr.type!=NIL)return 0;
	}
	return 1;
}

int isLoadExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=&((AST_pair*)objCptr->value)->car;
		AST_cptr* sec=nextCptr(fir);
		if(sec==NULL)return 0;
		if(fir->type!=ATM||sec->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->value;
		AST_atom* secAtm=sec->value;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"load")&&(secAtm->type==SYM||secAtm->type==STR))
			return 1;
	}
	return 0;
}

int isImportExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=&((AST_pair*)objCptr->value)->car;
		AST_cptr* sec=nextCptr(fir);
		if(sec==NULL)return 0;
		if(fir->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->value;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"import"))
			return 1;
	}
	return 0;
}

int isLibraryExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=&((AST_pair*)objCptr->value)->car;
		if(fir->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->value;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"library"))
			return 1;
	}
	return 0;
}

int isExportExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=&((AST_pair*)objCptr->value)->car;
		if(fir->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->value;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"export"))
			return 1;
	}
	return 0;
}

int isProcExpression(const AST_cptr* objCptr)
{
	if(objCptr->type==PAIR)
	{
		AST_cptr* fir=getFirstCptr(objCptr);
		if(fir->type!=ATM)
			return 0;
		AST_atom* firAtm=fir->value;
		if(firAtm->type==SYM&&!strcmp(firAtm->value.str,"proc"))
			return 1;
	}
	return 0;
}

KeyWord* hasKeyWord(const AST_cptr* objCptr,CompEnv* curEnv)
{
	AST_atom* tmpAtm=NULL;
	if(objCptr->type==ATM&&(tmpAtm=objCptr->value)->type==SYM)
	{
		for(;curEnv;curEnv=curEnv->prev)
		{
			KeyWord* tmp=curEnv->keyWords;
			while(tmp!=NULL&&strcmp(tmpAtm->value.str,tmp->word))
				tmp=tmp->next;
			return tmp;
		}
	}
	else if(objCptr->type==PAIR)
	{
		KeyWord* tmp=NULL;
		for(objCptr=&((AST_pair*)objCptr->value)->car;objCptr!=NULL;objCptr=nextCptr(objCptr))
		{
			CompEnv* tmpEnv=curEnv;
			tmpAtm=(objCptr->type==ATM)?objCptr->value:NULL;
			AST_atom* cdrAtm=(objCptr->outer->cdr.type==ATM)?objCptr->outer->cdr.value:NULL;
			if((tmpAtm==NULL||tmpAtm->type!=SYM)&&(cdrAtm==NULL||cdrAtm->type!=SYM))
			{
				tmp=NULL;
				continue;
			}
			for(;tmpEnv;tmpEnv=tmpEnv->prev)
			{
				tmp=tmpEnv->keyWords;
				while(tmp!=NULL&&tmpAtm!=NULL&&strcmp(tmpAtm->value.str,tmp->word)&&(cdrAtm==NULL||cdrAtm->type!=SYM||(cdrAtm!=NULL&&strcmp(cdrAtm->value.str,tmp->word))))
					tmp=tmp->next;
				if(tmp!=NULL)break;
			}
		}
		return tmp;
	}
	return NULL;
}

void printAllKeyWord(KeyWord* head)
{
	KeyWord* tmp=head;
	while(tmp!=NULL)
	{
		puts(tmp->word);
		tmp=tmp->next;
	}
}

void addKeyWord(const char* objStr,CompEnv* curEnv)
{
	if(objStr!=NULL)
	{
		KeyWord* current=curEnv->keyWords;
		KeyWord* prev=NULL;
		while(current!=NULL&&strcmp(current->word,objStr)){prev=current;current=current->next;}
		if(current==NULL)
		{
			current=(KeyWord*)malloc(sizeof(KeyWord));
			current->word=(char*)malloc(sizeof(char)*(strlen(objStr)+1));
			if(current==NULL||current->word==NULL)errors("addKeyWord",__FILE__,__LINE__);
			strcpy(current->word,objStr);
			if(prev!=NULL)prev->next=current;
			else
				curEnv->keyWords=current;
			current->next=NULL;
		}
	}
}

int isKeyWord(const char* str,CompEnv* curEnv)
{
	for(;curEnv;curEnv=curEnv->prev)
	{
		KeyWord* cur=curEnv->keyWords;
		for(;cur;cur=cur->next)
			if(!strcmp(str,cur->word))
				return 1;
	}
	return 0;
}
