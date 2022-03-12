#include<fakeLisp/bytecode.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/deftype.h>
#include<fakeLisp/symbol.h>
#include<string.h>
#include<stdlib.h>
FklByteCode* fklNewByteCode(unsigned int size)
{
	FklByteCode* tmp=NULL;
	FKL_ASSERT((tmp=(FklByteCode*)malloc(sizeof(FklByteCode))),"fklNewByteCode",__FILE__,__LINE__);
	tmp->size=size;
	FKL_ASSERT((tmp->code=(uint8_t*)malloc(size*sizeof(uint8_t))),"fklNewByteCode",__FILE__,__LINE__);
	int32_t i=0;
	for(;i<tmp->size;i++)tmp->code[i]=0;
	return tmp;
}

FklByteCodelnt* fklNewByteCodelnt(FklByteCode* bc)
{
	FklByteCodelnt* t=(FklByteCodelnt*)malloc(sizeof(FklByteCodelnt));
	FKL_ASSERT(t,"fklNewByteCode",__FILE__,__LINE__);
	t->ls=0;
	t->l=NULL;
	t->bc=bc;
	return t;
}

void fklFreeByteCodeAndLnt(FklByteCodelnt* t)
{
	fklFreeByteCode(t->bc);
	if(t->l)
	{
		for(uint32_t i=0;i<(t->ls);i++)
			fklFreeLineNumTabNode((t->l)[i]);
		free(t->l);
	}
	free(t);
}

void fklFreeByteCodelnt(FklByteCodelnt* t)
{
	fklFreeByteCode(t->bc);
	if(t->l)
		free(t->l);
	free(t);
}

void fklFreeByteCode(FklByteCode* obj)
{
	free(obj->code);
	free(obj);
}

void fklCodeCat(FklByteCode* fir,const FklByteCode* sec)
{
	int32_t size=fir->size;
	fir->size=sec->size+fir->size;
	fir->code=(uint8_t*)realloc(fir->code,sizeof(uint8_t)*fir->size);
	FKL_ASSERT(fir->code||!fir->size,"fklCodeCat",__FILE__,__LINE__);
	memcpy(fir->code+size,sec->code,sec->size);
}

void fklReCodeCat(const FklByteCode* fir,FklByteCode* sec)
{
	int32_t size=fir->size;
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*(fir->size+sec->size));
	FKL_ASSERT(tmp,"fklReCodeCat",__FILE__,__LINE__);
	memcpy(tmp,fir->code,fir->size);
	memcpy(tmp+size,sec->code,sec->size);
	free(sec->code);
	sec->code=tmp;
	sec->size=fir->size+sec->size;
}

FklByteCode* fklCopyByteCode(const FklByteCode* obj)
{
	FklByteCode* tmp=fklNewByteCode(obj->size);
	memcpy(tmp->code,obj->code,obj->size);
	return tmp;
}

FklByteCodelnt* fklCopyByteCodelnt(const FklByteCodelnt* obj)
{
	FklByteCodelnt* tmp=fklNewByteCodelnt(fklCopyByteCode(obj->bc));
	tmp->ls=obj->ls;
	tmp->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*obj->ls);
	FKL_ASSERT(tmp->l,"fklCopyByteCodelnt",__FILE__,__LINE__);
	int32_t i=0;
	for(;i<obj->ls;i++)
	{
		FklLineNumTabNode* t=obj->l[i];
		FklLineNumTabNode* node=fklNewLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
		tmp->l[i]=node;
	}
	return tmp;
}

void fklPrintByteCode(const FklByteCode* tmpCode,FILE* fp)
{
	int32_t i=0;
	while(i<tmpCode->size)
	{
		int tmplen=0;
		fprintf(fp,"%d: %s ",i,fklGetOpcodeName((FklOpcode)(tmpCode->code[i])));
		switch(fklGetOpcodeArgLen((FklOpcode)(tmpCode->code[i])))
		{
			case -4:
				{
					fprintf(fp,"%s ",fklFindSymbolInGlob((char*)(tmpCode->code+(++i)))->symbol);
					i+=sizeof(FklSid_t);
					int32_t handlerNum=*(int32_t*)(tmpCode->code+i);
					fprintf(fp,"%d\n",handlerNum);
					i+=sizeof(int32_t);
					int j=0;
					for(;j<handlerNum;j++)
					{
						FklSid_t type=*(FklSid_t*)(tmpCode->code+i);
						i+=sizeof(FklSid_t);
						uint32_t pCpc=*(uint32_t*)(tmpCode->code+i);
						i+=sizeof(uint32_t);
						fprintf(fp,"%s %d ",fklGetGlobSymbolWithId(type)->symbol,pCpc);
						fklPrintAsByteStr(tmpCode->code+i,pCpc,fp);
						i+=pCpc;
					}
				}
				break;
			case -3:
				fprintf(fp,"%d %d",*(int32_t*)(tmpCode->code+i+1),*(int32_t*)(tmpCode->code+i+1+sizeof(int32_t)));
				i+=9;
				break;
			case -2:
				fprintf(fp,"%d ",*(int32_t*)(tmpCode->code+i+1));
				fklPrintAsByteStr((uint8_t*)(tmpCode->code+i+5),*(int32_t*)(tmpCode->code+i+1),fp);
				i+=5+*(int32_t*)(tmpCode->code+i+1);
				break;
			case -1:
				tmplen=strlen((char*)tmpCode->code+i+1);
				fprintf(fp,"%s",tmpCode->code+i+1);
				i+=tmplen+2;
				break;
			case 0:
				i+=1;
				break;
			case 1:
				fklPrintRawChar(tmpCode->code[i+1],fp);
				i+=2;
				break;
			case 4:
				if(tmpCode->code[i]==FKL_PUSH_SYM)
					fprintf(fp,"%s",fklGetGlobSymbolWithId(*(FklSid_t*)(tmpCode->code+i+1))->symbol);
				else
					fprintf(fp,"%d",*(int32_t*)(tmpCode->code+i+1));
				i+=5;
				break;
			case 8:
				switch(tmpCode->code[i])
				{
					case FKL_PUSH_DBL:
						fprintf(fp,"%lf",*(double*)(tmpCode->code+i+1));
						break;
					case FKL_PUSH_I64:
						fprintf(fp,"%ld",*(int64_t*)(tmpCode->code+i+1));
						break;
					case FKL_PUSH_IND_REF:
						fprintf(fp,"%d %u",*(FklTypeId_t*)(tmpCode->code+i+1),*(uint32_t*)(tmpCode->code+i+1+sizeof(FklTypeId_t)));
						break;
				}
				i+=9;
				break;
			case 12:
				fprintf(fp,"%ld %d",*(ssize_t*)(tmpCode->code+i+1),*(FklTypeId_t*)(tmpCode->code+i+1+sizeof(ssize_t)));
				i+=13;
				break;
		}
		putc('\n',fp);
	}
}

void fklPrintByteCodelnt(FklByteCodelnt* obj,FILE* fp)
{
	FklByteCode* tmpCode=obj->bc;
	int32_t i=0;
	int32_t j=0;
	int32_t fid=0;
	int32_t line=0;
	while(i<tmpCode->size)
	{
		int tmplen=0;
		fprintf(fp,"%d: %s ",i,fklGetOpcodeName((FklOpcode)(tmpCode->code[i])));
		switch(fklGetOpcodeArgLen((FklOpcode)(tmpCode->code[i])))
		{
			case -4:
				{
					fprintf(fp,"%s ",fklFindSymbolInGlob((char*)(tmpCode->code+(++i)))->symbol);
					i+=sizeof(FklSid_t);
					int32_t handlerNum=*(int32_t*)(tmpCode->code+i);
					fprintf(fp,"%d\n",handlerNum);
					i+=sizeof(int32_t);
					int j=0;
					for(;j<handlerNum;j++)
					{
						FklSid_t type=*(FklSid_t*)(tmpCode->code+i);
						i+=sizeof(FklSid_t);
						uint32_t pCpc=*(uint32_t*)(tmpCode->code+i);
						i+=sizeof(uint32_t);
						fprintf(fp,"%s %d ",fklGetGlobSymbolWithId(type)->symbol,pCpc);
						fklPrintAsByteStr(tmpCode->code+i,pCpc,fp);
						i+=pCpc;
					}
				}
			case -3:
				fprintf(fp,"%d %d",*(int32_t*)(tmpCode->code+i+1),*(int32_t*)(tmpCode->code+i+1+sizeof(int32_t)));
				i+=9;
				break;
			case -2:
				fprintf(fp,"%d ",*(int32_t*)(tmpCode->code+i+1));
				fklPrintAsByteStr((uint8_t*)(tmpCode->code+i+5),*(int32_t*)(tmpCode->code+i+1),fp);
				i+=5+*(int32_t*)(tmpCode->code+i+1);
				break;
			case -1:
				tmplen=strlen((char*)tmpCode->code+i+1);
				fprintf(fp,"%s",tmpCode->code+i+1);
				i+=tmplen+2;
				break;
			case 0:
				i+=1;
				break;
			case 1:
				fklPrintRawChar(tmpCode->code[i+1],fp);
				i+=2;
				break;
			case 4:
				if(tmpCode->code[i]==FKL_PUSH_SYM)
					fprintf(fp,"%s",fklGetGlobSymbolWithId(*(FklSid_t*)(tmpCode->code+i+1))->symbol);
				else
					fprintf(fp,"%d",*(int32_t*)(tmpCode->code+i+1));
				i+=5;
				break;
			case 8:
				switch(tmpCode->code[i])
				{
					case FKL_PUSH_DBL:
						fprintf(fp,"%lf",*(double*)(tmpCode->code+i+1));
						break;
					case FKL_PUSH_I64:
						fprintf(fp,"%ld",*(int64_t*)(tmpCode->code+i+1));
						break;
					case FKL_PUSH_IND_REF:
						fprintf(fp,"%d %u",*(FklTypeId_t*)(tmpCode->code+i+1),*(uint32_t*)(tmpCode->code+i+1+sizeof(FklTypeId_t)));
						break;
					case FKL_PUSH_FPROC:
						fprintf(fp,"%d %u",*(FklTypeId_t*)(tmpCode->code+i+1),*(FklSid_t*)(tmpCode->code+i+1+sizeof(FklTypeId_t)));
						break;
				}
				i+=9;
				break;
			case 12:
				fprintf(fp,"%ld %d",*(ssize_t*)(tmpCode->code+i+1),*(FklTypeId_t*)(tmpCode->code+i+1+sizeof(ssize_t)));
				i+=13;
				break;
		}
		if(obj->l[j]->scp+obj->l[j]->cpc<i)
			j++;
		putc('\t',fp);
		if(obj->l[j]->fid!=fid||obj->l[j]->line!=line)
		{
			fid=obj->l[j]->fid;
			line=obj->l[j]->line;
			fprintf(fp,"%s:%d:%d",fklGetGlobSymbolWithId(obj->l[j]->fid)->symbol,obj->l[j]->line,obj->l[j]->cpc);
		}
		putc('\n',fp);
	}
}

void fklIncreaseScpOfByteCodelnt(FklByteCodelnt* o,int32_t size)
{
	int32_t i=0;
	for(;i<o->ls;i++)
		o->l[i]->scp+=size;
}

void fklCodeLntCat(FklByteCodelnt* f,FklByteCodelnt* s)
{
	if(!f->l)
	{
		f->ls=s->ls;
		f->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*s->ls);
		FKL_ASSERT(f->l,"fklCodefklLntCat",__FILE__,__LINE__);
		s->l[0]->cpc+=f->bc->size;
		FKL_INCREASE_ALL_SCP(s->l+1,s->ls-1,f->bc->size);
		memcpy(f->l,s->l,(s->ls)*sizeof(FklLineNumTabNode*));
	}
	else
	{
		FKL_INCREASE_ALL_SCP(s->l,s->ls,f->bc->size);
		if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
		{
			f->l[f->ls-1]->cpc+=s->l[0]->cpc;
			f->l=(FklLineNumTabNode**)realloc(f->l,sizeof(FklLineNumTabNode*)*(f->ls+s->ls-1));
			FKL_ASSERT(f->l,"fklCodefklLntCat",__FILE__,__LINE__);
			memcpy(f->l+f->ls,s->l+1,(s->ls-1)*sizeof(FklLineNumTabNode*));
			f->ls+=s->ls-1;
			fklFreeLineNumTabNode(s->l[0]);
		}
		else
		{
			f->l=(FklLineNumTabNode**)realloc(f->l,sizeof(FklLineNumTabNode*)*(f->ls+s->ls));
			FKL_ASSERT(f->l,"fklCodefklLntCat",__FILE__,__LINE__);
			memcpy(f->l+f->ls,s->l,(s->ls)*sizeof(FklLineNumTabNode*));
			f->ls+=s->ls;
		}
	}
	fklCodeCat(f->bc,s->bc);
}

void fklCodelntCopyCat(FklByteCodelnt* f,const FklByteCodelnt* s)
{
	if(s->ls)
	{
		if(!f->l)
		{
			f->ls=s->ls;
			f->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*s->ls);
			FKL_ASSERT(f->l,"fklCodelntCopyCat",__FILE__,__LINE__);
			uint32_t i=0;
			for(;i<f->ls;i++)
			{
				FklLineNumTabNode* t=s->l[i];
				f->l[i]=fklNewLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
			}
			f->l[0]->cpc+=f->bc->size;
			FKL_INCREASE_ALL_SCP(f->l+1,f->ls-1,f->bc->size);
		}
		else
		{
			uint32_t i=0;
			FklLineNumTabNode** tl=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*s->ls);
			FKL_ASSERT(tl,"fklCodelntCopyCat",__FILE__,__LINE__);
			for(;i<s->ls;i++)
			{
				FklLineNumTabNode* t=s->l[i];
				tl[i]=fklNewLineNumTabNode(t->fid,t->scp,t->cpc,t->line);
			}
			FKL_INCREASE_ALL_SCP(tl,s->ls,f->bc->size);
			if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
			{
				f->l[f->ls-1]->cpc+=s->l[0]->cpc;
				f->l=(FklLineNumTabNode**)realloc(f->l,sizeof(FklLineNumTabNode*)*(f->ls+s->ls-1));
				FKL_ASSERT(f->l,"fklCodelntCopyCat",__FILE__,__LINE__);
				memcpy(f->l+f->ls,tl+1,(s->ls-1)*sizeof(FklLineNumTabNode*));
				f->ls+=s->ls-1;
				fklFreeLineNumTabNode(tl[0]);
			}
			else
			{
				f->l=(FklLineNumTabNode**)realloc(f->l,sizeof(FklLineNumTabNode*)*(f->ls+s->ls));
				FKL_ASSERT(f->l,"fklCodelntCopyCat",__FILE__,__LINE__);
				memcpy(f->l+f->ls,tl,(s->ls)*sizeof(FklLineNumTabNode*));
				f->ls+=s->ls;
			}
			free(tl);
		}
	}
	fklCodeCat(f->bc,s->bc);
}

void fklReCodeLntCat(FklByteCodelnt* f,FklByteCodelnt* s)
{
	if(!s->l)
	{
		s->ls=f->ls;
		s->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*f->ls);
		FKL_ASSERT(s->l,"fklReCodeLntCat",__FILE__,__LINE__);
		f->l[f->ls-1]->cpc+=s->bc->size;
		memcpy(s->l,f->l,(f->ls)*sizeof(FklLineNumTabNode*));
	}
	else
	{
		FKL_INCREASE_ALL_SCP(s->l,s->ls,f->bc->size);
		if(f->l[f->ls-1]->line==s->l[0]->line&&f->l[f->ls-1]->fid==s->l[0]->fid)
		{
			FklLineNumTabNode** l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*(f->ls+s->ls-1));
			FKL_ASSERT(l,"fklReCodeLntCat",__FILE__,__LINE__);
			f->l[f->ls-1]->cpc+=s->l[0]->cpc;
			fklFreeLineNumTabNode(s->l[0]);
			memcpy(l,f->l,(f->ls)*sizeof(FklLineNumTabNode*));
			memcpy(l+f->ls,s->l+1,(s->ls-1)*sizeof(FklLineNumTabNode*));
			free(s->l);
			s->l=l;
			s->ls+=f->ls-1;
		}
		else
		{
			FklLineNumTabNode** l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*(f->ls+s->ls));
			FKL_ASSERT(l,"fklReCodeCat",__FILE__,__LINE__);
			memcpy(l,f->l,(f->ls)*sizeof(FklLineNumTabNode*));
			memcpy(l+f->ls,s->l,(s->ls)*sizeof(FklLineNumTabNode*));
			free(s->l);
			s->l=l;
			s->ls+=f->ls;
		}
	}
	fklReCodeCat(f->bc,s->bc);
}

FklByteCodeLabel* fklNewByteCodeLable(int32_t place,const char* label)
{
	FklByteCodeLabel* tmp=(FklByteCodeLabel*)malloc(sizeof(FklByteCodeLabel));
	FKL_ASSERT(tmp,"fklNewByteCodeLable",__FILE__,__LINE__);
	tmp->place=place;
	tmp->label=fklCopyStr(label);
	return tmp;
}

FklByteCodeLabel* fklFindByteCodeLabel(const char* label,FklPtrStack* s)
{
	int32_t l=0;
	int32_t h=s->top-1;
	int32_t mid;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int cmp=strcmp(((FklByteCodeLabel*)s->base[mid])->label,label);
		if(cmp>0)
			h=mid-1;
		else if(cmp<0)
			l=mid+1;
		else
			return (FklByteCodeLabel*)s->base[mid];
	}
	return NULL;
}


void fklFreeByteCodeLabel(FklByteCodeLabel* obj)
{
	free(obj->label);
	free(obj);
}

FklLineNumberTable* fklNewLineNumTable()
{
	FklLineNumberTable* t=(FklLineNumberTable*)malloc(sizeof(FklLineNumberTable));
	FKL_ASSERT(t,"fklNewLineNumTable",__FILE__,__LINE__);
	t->num=0;
	t->list=NULL;
	return t;
}

FklLineNumTabNode* fklNewLineNumTabNode(FklSid_t fid,int32_t scp,int32_t cpc,int32_t line)
{
	FklLineNumTabNode* t=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode));
	FKL_ASSERT(t,"fklNewLineNumTable",__FILE__,__LINE__);
	t->fid=fid;
	t->scp=scp;
	t->cpc=cpc;
	t->line=line;
	return t;
}

void fklFreeLineNumTabNode(FklLineNumTabNode* n)
{
	free(n);
}

void fklFreeLineNumberTable(FklLineNumberTable* t)
{
	int32_t i=0;
	for(;i<t->num;i++)
		fklFreeLineNumTabNode(t->list[i]);
	free(t->list);
	free(t);
}

FklLineNumTabNode* fklFindLineNumTabNode(uint32_t cp,FklLineNumberTable* t)
{
	int32_t i=0;
	uint32_t size=t->num;
	FklLineNumTabNode** list=t->list;
	for(;i<size;i++)
	{
		if(list[i]->scp<=cp&&(list[i]->scp+list[i]->cpc)>=cp)
			return list[i];
	}
	return NULL;
}

void fklLntCat(FklLineNumberTable* t,int32_t bs,FklLineNumTabNode** l2,int32_t s2)
{
	if(!t->list)
	{
		t->num=s2;
		t->list=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*s2);
		FKL_ASSERT(t->list,"fklLntCat",__FILE__,__LINE__);
		l2[0]->cpc+=bs;
		FKL_INCREASE_ALL_SCP(l2+1,s2-1,bs);
		memcpy(t->list,l2,(s2)*sizeof(FklLineNumTabNode*));
	}
	else
	{
		FKL_INCREASE_ALL_SCP(l2,s2,bs);
		if(t->list[t->num-1]->line==l2[0]->line&&t->list[t->num-1]->fid==l2[0]->fid)
		{
			t->list[t->num-1]->cpc+=l2[0]->cpc;
			t->list=(FklLineNumTabNode**)realloc(t->list,sizeof(FklLineNumTabNode*)*(t->num+s2-1));
			FKL_ASSERT(t->list,"fklLntCat",__FILE__,__LINE__);
			memcpy(t->list+t->num,l2+1,(s2-1)*sizeof(FklLineNumTabNode*));
			t->num+=s2-1;
			fklFreeLineNumTabNode(l2[0]);
		}
		else
		{
			t->list=(FklLineNumTabNode**)realloc(t->list,sizeof(FklLineNumTabNode*)*(t->num+s2));
			FKL_ASSERT(t->list,"fklLntCat",__FILE__,__LINE__);
			memcpy(t->list+t->num,l2,(s2)*sizeof(FklLineNumTabNode*));
			t->num+=s2;
		}
	}
}

void fklWriteLineNumberTable(FklLineNumberTable* lnt,FILE* fp)
{
	int32_t size=lnt->num;
	fwrite(&size,sizeof(size),1,fp);
	int32_t i=0;
	for(;i<size;i++)
	{
		FklLineNumTabNode* n=lnt->list[i];
		fwrite(&n->fid,sizeof(n->fid),1,fp);
		fwrite(&n->scp,sizeof(n->scp),1,fp);
		fwrite(&n->cpc,sizeof(n->cpc),1,fp);
		fwrite(&n->line,sizeof(n->line),1,fp);
	}
}

void fklDBG_printByteCode(uint8_t* code,uint32_t s,uint32_t c,FILE* fp)
{
	FklByteCode t={c,code+s};
	fklPrintByteCode(&t,fp);
}
