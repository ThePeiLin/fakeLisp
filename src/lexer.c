#include "fakeLisp/base.h"
#include "fakeLisp/symbol.h"
#include<fakeLisp/lexer.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/reader.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/parser.h>
#include<ctype.h>
#include <stdlib.h>
#include<string.h>

static inline int isCompleteStringBuf(const char* buf,size_t size,char ch,size_t* len)
{
	int mark=0;
	int markN=0;
	size_t i=0;
	for(;i<size&&markN<2
			;i+=(buf[i]=='\\')+1)
		if(buf[i]==ch)
		{
			mark=~mark;
			markN+=1;
		}
	if(len)
		*len=mark?0:i;
	return !mark;
}

static inline FklNastNode* getPartFromState(FklStringMatchState* state,size_t index)
{
	return state->pattern->parts->u.vec->base[index];
}

static inline FklNastNode* getPartFromPattern(FklStringMatchPattern* pattern,size_t index)
{
	return pattern->parts->u.vec->base[index];
}

static inline size_t getSizeFromPattern(FklStringMatchPattern* p)
{
	return p->parts->u.vec->size;
}

static FklStringMatchState* createStringMatchState(FklStringMatchPattern* pattern
		,size_t index
		,FklStringMatchState* next)
{
	FklStringMatchState* r=(FklStringMatchState*)malloc(sizeof(FklStringMatchState));
	FKL_ASSERT(r);
	r->next=next;
	r->index=index;
	r->pattern=pattern;
	return r;
}

static void addStateIntoSyms(FklStringMatchState** syms,FklStringMatchState* state)
{
	state->next=*syms;
	*syms=state;
}

static void addStateIntoStrs(FklStringMatchState** strs,FklStringMatchState* state)
{
	size_t len=getPartFromState(state,state->index)->u.str->size;
	for(;*strs;strs=&(*strs)->next)
	{
		size_t curlen=getPartFromState((*strs),(*strs)->index)->u.str->size;
		if(len>=curlen)
		{
			state->next=*strs;
			*strs=state;
			break;
		}
	}
	if(!*strs)
	{
		state->next=*strs;
		*strs=state;
	}
}

static void addStateIntoBoxes(FklStringMatchState** boxes,FklStringMatchState* state)
{
	size_t len=getPartFromState(state,state->index+1)->u.str->size;
	for(;*boxes;boxes=&(*boxes)->next)
	{
		size_t curlen=getPartFromState((*boxes),(*boxes)->index+1)->u.str->size;
		if(len>=curlen)
		{
			state->next=*boxes;
			*boxes=state;
			break;
		}
	}
	if(!*boxes)
	{
		state->next=*boxes;
		*boxes=state;
	}
}

static void addStateIntoSet(FklStringMatchState* state
		,FklStringMatchState** strs
		,FklStringMatchState** boxes
		,FklStringMatchState** syms)
{
	FklNastNode* part=getPartFromState(state,state->index);
	switch(part->type)
	{
		case FKL_NAST_STR:
			addStateIntoStrs(strs,state);
			break;
		case FKL_NAST_BOX:
			addStateIntoBoxes(boxes,state);
			break;
		case FKL_NAST_SYM:
			addStateIntoSyms(syms,state);
			break;
		default:
			FKL_ASSERT(0);
	}
}

static FklStringMatchSet* createStringMatchSet(FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchState* syms
		,FklStringMatchSet* prev)
{
	FklStringMatchSet* r=(FklStringMatchSet*)malloc(sizeof(FklStringMatchSet));
	FKL_ASSERT(r);
	r->str=strs;
	r->box=boxes;
	r->sym=syms;
	r->prev=prev;
	return r;
}

static inline int charbufEqual(const char* buf,size_t n,const char* s,size_t l)
{
	return n>=l&&!memcmp(s,buf,l);
}

static inline int simpleMatch(const char* buf,size_t n,FklString* s)
{
	return n>=s->size&&!memcmp(s->str,buf,s->size);
}

static inline int matchWithStr(size_t pn,const char* buf,size_t n,FklString* s)
{
	return (pn==0||pn==s->size)&&simpleMatch(buf,n,s);
}

static int matchStrInBoxes(const char* buf
		,size_t n
		,FklStringMatchState* boxes)
{
	for(;boxes;boxes=boxes->next)
	{
		FklString* s=getPartFromState(boxes,boxes->index+1)->u.str;
		if(simpleMatch(buf,n,s))
			return 1;
	}
	return 0;
}

static int matchStrInStrs(const char* buf
		,size_t n
		,FklStringMatchState* strs)
{
	for(;strs;strs=strs->next)
	{
		FklString* s=getPartFromState(strs,strs->index)->u.str;
		if(simpleMatch(buf,n,s))
			return 1;
	}
	return 0;
}

static int matchStrInPatterns(const char* buf
		,size_t n
		,FklStringMatchPattern* patterns)
{
	for(;patterns;patterns=patterns->next)
	{
		FklString* s=getPartFromPattern(patterns,0)->u.str;
		if(simpleMatch(buf,n,s))
			return 1;
	}
	return 0;
}

#define DEFAULT_TOKEN_HEADER_STR       (0)
#define DEFAULT_TOKEN_HEADER_SYM       (1)
#define DEFAULT_TOKEN_HEADER_COMMENT_0 (2)
#define DEFAULT_TOKEN_HEADER_COMMENT_1 (3)
#define DEFAULT_TOKEN_HEADER_CHR       (4)

static const struct DefaultTokenHeader
{
	size_t l;
	const char* s;
}
DefaultTokenHeaders[]=
{
	{1,"\"",},
	{1,"|",},
	{1,";",},
	{2,"#!",},
	{2,"#\\",},
	{0,NULL,},
};

static inline int isComment(const char* buf,size_t n)
{
	return charbufEqual(buf,n
			,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_0].s
			,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_0].l)
		||charbufEqual(buf,n
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_1].s
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_COMMENT_1].l);
}

static inline int matchStrInDefaultHeaders(const char* buf,size_t n)
{
	for(const struct DefaultTokenHeader* cur=&DefaultTokenHeaders[0];cur->l;cur++)
	{
		size_t l=cur->l;
		const char* s=cur->s;
		if(charbufEqual(buf,n,s,l))
			return 1;
	}
	return 0;
}

static size_t getDefaultSymbolLen(const char* buf
		,size_t n
		,FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchPattern* patterns
		,int* p)
{
	int hasStrs=0;
	size_t i=0;
	for(;i<n
			&&!isspace(buf[i])
			&&!(hasStrs=matchStrInStrs(&buf[i],n-i,strs))
			&&!matchStrInBoxes(&buf[i],n-i,boxes)
			&&!matchStrInPatterns(&buf[i],n-i,patterns)
			&&!matchStrInDefaultHeaders(&buf[i],n-i)
			;i++);
	if(p)
		*p=hasStrs;
	return i;
}

static FklToken* createDefaultToken(const char* buf
		,size_t n
		,FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchPattern* patterns
		,size_t line
		,size_t* jInc
		,int* err)
{
	if(charbufEqual(buf,n
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_CHR].s
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_CHR].l))
	{
		size_t len=getDefaultSymbolLen(&buf[3],n-3,strs,boxes,patterns,NULL)+3;
		*jInc=len;
		return fklCreateToken(FKL_TOKEN_CHAR,fklCreateString(len,buf),line);
	}
	else if(charbufEqual(buf,n
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_STR].s
				,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_STR].l))
	{
		size_t len=0;
		int complete=isCompleteStringBuf(buf,n,'"',&len);
		if(complete)
		{
			*jInc=len;
			size_t size=0;
			char* s=fklCastEscapeCharBuf(&buf[1],len-2,&size);
			FklString* str=fklCreateString(size,s);
			free(s);
			return fklCreateToken(FKL_TOKEN_STRING,str,line);
		}
		else
			return FKL_INCOMPLETED_TOKEN;
	}
	else
	{
		int shouldBeSymbol=0;
		size_t i=0;
		FklUintStack lenStack=FKL_STACK_INIT;
		fklInitUintStack(&lenStack,2,4);
		int hasStrs=0;
		while(i<n)
		{
			size_t len=0;
			if(charbufEqual(&buf[i],n-i
						,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].s
						,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].l))
			{
				shouldBeSymbol=1;
				int complete=isCompleteStringBuf(&buf[i],n-i,'|',&len);
				if(!complete)
				{
					fklUninitUintStack(&lenStack);
					return FKL_INCOMPLETED_TOKEN;
				}
			}
			else
				len=getDefaultSymbolLen(&buf[i],n-i,strs,boxes,patterns,&hasStrs);
			*err=hasStrs&&!len&&!lenStack.top;
			if(!len)
				break;
			i+=len;
			fklPushUintStack(len,&lenStack);
		}
		FklToken* t=NULL;
		if(i)
		{
			i=0;
			FklString* str=fklCreateEmptyString();
			while(!fklIsUintStackEmpty(&lenStack))
			{
				uint64_t len=fklPopUintStack(&lenStack);
				if(charbufEqual(&buf[i],n-i
							,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].s
							,DefaultTokenHeaders[DEFAULT_TOKEN_HEADER_SYM].l))
				{
					size_t size=0;
					char* s=fklCastEscapeCharBuf(&buf[i+1],len-2,&size);
					fklStringCharBufCat(&str,s,size);
					free(s);
				}
				else
					fklStringCharBufCat(&str,&buf[i],len);
				i+=len;
			}
			*jInc=i;
			t=fklCreateToken(FKL_TOKEN_SYMBOL,str,line);
			if(!shouldBeSymbol&&fklIsNumberString(t->value))
				t->type=FKL_TOKEN_NUM;
		}
		fklUninitUintStack(&lenStack);
		return t;
	}
}

static FklStringMatchSet* matchInUniversSet(const char* buf
		,size_t n
		,FklStringMatchPattern* pattern
		,FklToken** pt
		,size_t line
		,FklStringMatchSet* prev
		,size_t* jInc
		,int* err)
{
	FklStringMatchSet* r=NULL;
	size_t pn=0;
	FklStringMatchState* strs=NULL;
	FklStringMatchState* boxes=NULL;
	FklStringMatchState* syms=NULL;
	for(FklStringMatchPattern* p=pattern;p;p=p->next)
	{
		FklString* s=p->parts->u.vec->base[0]->u.str;
		if(pn!=0&&pn>s->size)
			break;
		if(matchWithStr(pn,buf,n,s))
		{
			pn=s->size;
			if(!*pt)
				*pt=fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(s),line);
			FklStringMatchState* state=createStringMatchState(p,1,NULL);
			addStateIntoSet(state,&strs,&boxes,&syms);
		}
	}
	if(strs||boxes||syms)
		r=createStringMatchSet(strs,boxes,syms,prev);
	if(!*pt)
	{
		*pt=createDefaultToken(buf
				,n
				,prev?prev->str:NULL
				,prev?prev->box:NULL
				,pattern
				,line
				,jInc
				,err);
	}
	else
		*jInc=(*pt)->value->size;
	return r;
}

static int updateStrState(const char* buf
		,size_t n
		,FklStringMatchState** strs
		,FklStringMatchSet* prev
		,FklToken** pt
		,size_t line
		,FklStringMatchRouteNode* route
		,size_t top)
{
	int r=0;
	size_t pn=0;
	for(FklStringMatchState** pstate=strs;*pstate;)
	{
		FklStringMatchState* cur=*pstate;
		FklString* s=getPartFromState(cur,cur->index)->u.str;
		if(pn!=0&&pn>s->size)
			break;
		if(matchWithStr(pn,buf,n,s))
		{
			pn=s->size;
			if(!*pt)
				*pt=fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(s),line);
			cur->index++;
			r=1;
			if(cur->index<getSizeFromPattern(cur->pattern))
			{
				*pstate=cur->next;
				addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
				continue;
			}
		}
		pstate=&(*pstate)->next;
	}
	if(!r)
	{
		prev->str=*strs;
		*strs=NULL;
	}
	return r;
}

static int updateBoxState(const char* buf
		,size_t n
		,FklStringMatchState** boxes
		,FklStringMatchSet* prev
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line
		,FklStringMatchRouteNode* route
		,size_t top)
{
	int r=0;
	size_t pn=0;
	for(FklStringMatchState** pstate=boxes;*pstate;)
	{
		FklStringMatchState* cur=*pstate;
		FklString* s=getPartFromState(cur,cur->index+1)->u.str;
		if(pn!=0&&pn>s->size)
			break;
		if(matchWithStr(pn,buf,n,s))
		{
			pn=s->size;
			if(!*pt)
				*pt=fklCreateToken(FKL_TOKEN_RESERVE_STR,fklCopyString(s),line);
			cur->index+=2;
			r=1;
			if(cur->index<getSizeFromPattern(cur->pattern))
			{
				*pstate=cur->next;
				addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
				continue;
			}
		}
		pstate=&(*pstate)->next;
	}
	if(!r)
	{
		prev->box=*boxes;
		*boxes=NULL;
	}
	return r;
}

static void updateSymState(const char* buf
		,size_t n
		,FklStringMatchState** syms
		,FklStringMatchSet* prev
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line)
{
	for(FklStringMatchState** pstate=syms;*pstate;)
	{
		FklStringMatchState* cur=*pstate;
		cur->index++;
		if(cur->index<getSizeFromPattern(cur->pattern))
		{
			*pstate=cur->next;
			addStateIntoSet(cur,&prev->str,&prev->box,&prev->sym);
		}
		else
			pstate=&(*pstate)->next;
	}
}

static FklStringMatchState* getRollBack(FklStringMatchState** pcur,FklStringMatchState* prev)
{
	FklStringMatchState* r=NULL;
	while(*pcur!=prev)
	{
		FklStringMatchState* cur=*pcur;
		*pcur=cur->next;
		cur->next=r;
		cur->index--;
		r=cur;
	}
	return r;
}

static void rollBack(FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchState** syms
		,FklStringMatchSet* oset)
{
	FklStringMatchState* ostrs=getRollBack(&oset->str,strs);
	FklStringMatchState* oboxes=getRollBack(&oset->box,boxes);
	for(FklStringMatchState* cur=*syms;cur;cur=cur->next)
		cur->index--;
	FklStringMatchState** psym=&oset->sym;
	for(;*psym;psym=&(*psym)->next)
		(*psym)->index--;
	*psym=*syms;
	*syms=NULL;
	for(FklStringMatchState* cur=ostrs;cur;)
	{
		FklStringMatchState* t=cur->next;
		addStateIntoSet(cur,&oset->str,&oset->box,&oset->sym);
		cur=t;
	}
	for(FklStringMatchState* cur=oboxes;cur;)
	{
		FklStringMatchState* t=cur->next;
		addStateIntoSet(cur,&oset->str,&oset->box,&oset->sym);
		cur=t;
	}
}

static inline int isValidToken(FklToken* t)
{
	return t&&t!=FKL_INCOMPLETED_TOKEN;
}

static inline FklStringMatchPattern* getCompleteMatchPattern(FklStringMatchState* strs
		,FklStringMatchState* boxes
		,FklStringMatchState* syms)
{
	for(FklStringMatchState* cur=strs;cur;cur=cur->next)
		if(cur->index>=getSizeFromPattern(cur->pattern))
			return cur->pattern;
	for(FklStringMatchState* cur=boxes;cur;cur=cur->next)
		if(cur->index>=getSizeFromPattern(cur->pattern))
			return cur->pattern;
	for(FklStringMatchState* cur=syms;cur;cur=cur->next)
		if(cur->index>=getSizeFromPattern(cur->pattern))
			return cur->pattern;
	return NULL;
}

static FklStringMatchSet* updatePreviusSet(FklStringMatchSet* set
		,const char* buf
		,size_t n
		,FklStringMatchPattern* patterns
		,FklToken** pt
		,size_t line
		,size_t* jInc
		,FklStringMatchRouteNode** proute
		,size_t top
		,int* err)
{
	FklStringMatchRouteNode* route=*proute;
	if(isComment(buf,n))
	{
		size_t len=0;
		for(;len<n&&buf[len]!='\n';len++);
		*pt=fklCreateToken(FKL_TOKEN_COMMENT,fklCreateString(len,buf),line);
		*jInc=len;
		return set;
	}
	FklStringMatchSet* r=NULL;
	if(set==FKL_STRING_PATTERN_UNIVERSAL_SET)
	{
		r=matchInUniversSet(buf
				,n
				,patterns
				,pt
				,line
				,NULL
				,jInc
				,err);
		if(!isValidToken(*pt))
		{
			route->start=top;
			route->end=top;
			return set;
		}
	}
	else
	{
		FklStringMatchState* strs=set->str;
		FklStringMatchState* boxes=set->box;
		FklStringMatchState* syms=set->sym;
		FklStringMatchState* ostrs=set->str;
		FklStringMatchState* oboxes=set->box;
		set->str=NULL;
		set->box=NULL;
		set->sym=NULL;
		int b=!updateStrState(buf,n,&strs,set,pt,line,route,top)
			&&!updateBoxState(buf,n,&boxes,set,patterns,pt,line,route,top);
		if(b)
			updateSymState(buf,n,&syms,set,patterns,pt,line);
		FklStringMatchSet* nset=NULL;
		FklStringMatchSet* oset=set;
		while(set&&set->str==NULL&&set->box==NULL&&set->sym==NULL)
			set=set->prev;
		if(b)
		{
			nset=matchInUniversSet(buf,n
					,patterns
					,pt
					,line
					,set
					,jInc
					,err);
			if(isValidToken(*pt))
			{
				FklStringMatchRouteNode* node=fklCreateStringMatchRouteNode(NULL
						,top
						,top
						,NULL
						,NULL
						,NULL);
				fklInsertMatchRouteNodeAsFirstChild(route,node);
				if(nset)
				{
					*proute=node;
					nset->prev=oset;
				}
			}
			else
			{
				rollBack(ostrs,oboxes,&syms,oset);
				set=oset;
			}
		}
		else
			*jInc=(*pt)->value->size;
		route->pattern=getCompleteMatchPattern(strs,boxes,syms);
		if(!nset&&isValidToken(*pt))
		{
			set=oset;
			while(set&&set->str==NULL&&set->box==NULL&&set->sym==NULL)
			{
				route->end=top;
				route=route->parent;
				FklStringMatchSet* oset=set;
				set=set->prev;
				free(oset);
			}
			*proute=route;
		}
		fklDestroyStringMatchState(strs);
		fklDestroyStringMatchState(boxes);
		fklDestroyStringMatchState(syms);
		r=(nset==NULL)?set:nset;
	}
	return r;
}

FklStringMatchSet* fklSplitStringIntoTokenWithPattern(const char* buf
		,size_t size
		,size_t line
		,size_t* pline
		,size_t j
		,size_t* pj
		,FklPtrStack* retvalStack
		,FklStringMatchSet* matchSet
		,FklStringMatchPattern* patterns
		,FklStringMatchRouteNode* route
		,FklStringMatchRouteNode** proute
		,int* err)
{
	while(j<size&&matchSet)
	{
		FklToken* token=NULL;
		size_t jInc=0;
		matchSet=updatePreviusSet(matchSet
				,&buf[j]
				,size-j
				,patterns
				,&token
				,line
				,&jInc
				,&route
				,retvalStack->top
				,err);
		if(*err)
			return matchSet;
		if(token==FKL_INCOMPLETED_TOKEN)
			break;
		else if(token==NULL)
		{
			if(buf[j]=='\n')
				line++;
			j++;
		}
		else
		{
			fklPushPtrStack(token,retvalStack);
			line+=fklCountCharInBuf(&buf[j],jInc,'\n');
			j+=jInc;
		}
	}
	*pline=line;
	*pj=j;
	*proute=route;
	return matchSet;
}

FklToken* fklCreateTokenCopyStr(FklTokenType type,const FklString* str,size_t line)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token);
	token->type=type;
	token->line=line;
	token->value=fklCopyString(str);
	return token;
}

FklToken* fklCreateToken(FklTokenType type,FklString* str,size_t line)
{
	FklToken* token=(FklToken*)malloc(sizeof(FklToken));
	FKL_ASSERT(token);
	token->type=type;
	token->line=line;
	token->value=str;
	return token;
}

void fklDestroyToken(FklToken* token)
{
	free(token->value);
	free(token);
}

void fklPrintToken(FklPtrStack* tokenStack,FILE* fp)
{
	static const char* tokenTypeName[]=
	{
		"reserve-str",
		"char",
		"num",
		"string",
		"symbol",
		"comment",
	};
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		fprintf(fp,"%lu,%s:",token->line,tokenTypeName[token->type]);
		fklPrintRawString(token->value,fp);
		fputc('\n',fp);
	}
}

int fklIsAllComment(FklPtrStack* tokenStack)
{
	for(uint32_t i=0;i<tokenStack->top;i++)
	{
		FklToken* token=tokenStack->base[i];
		if(token->type!=FKL_TOKEN_COMMENT)
			return 0;
	}
	return 1;
}


// lalr1
// =====

typedef struct
{
	FklSid_t left;
	FklGrammerProduction* prods;
}ProdHashItem;

static void prod_hash_set_key(void* k0,void* k1)
{
	*(FklSid_t*)k0=*(FklSid_t*)k1;
}

static uintptr_t prod_hash_func(void* k)
{
	return *(FklSid_t*)k;
}

static int prod_hash_equal(void* k0,void* k1)
{
	return *(FklSid_t*)k0==*(FklSid_t*)k1;
}

static void prod_hash_set_val(void* d0,void* d1)
{
	*(ProdHashItem*)d0=*(ProdHashItem*)d1;
}

static inline void destroy_prod(FklGrammerProduction* h)
{

	// if(!h->isBuiltin)
	// 	fklDestroyByteCodelnt(h->action.proc);
	free(h->units);
	free(h);
}

static void prod_hash_uninit(void* d)
{
	ProdHashItem* prod=(ProdHashItem*)d;
	FklGrammerProduction* h=prod->prods;
	while(h)
	{
		FklGrammerProduction* next=h->next;
		destroy_prod(h);
		h=next;
	}
	prod->prods=NULL;
}

static const FklHashTableMetaTable ProdHashMetaTable=
{
	.size=sizeof(ProdHashItem),
	.__setKey=prod_hash_set_key,
	.__setVal=prod_hash_set_val,
	.__hashFunc=prod_hash_func,
	.__uninitItem=prod_hash_uninit,
	.__keyEqual=prod_hash_equal,
	.__getKey=fklHashDefaultGetKey,
};

static inline FklGrammerProduction* create_empty_production(FklSid_t left,size_t len)
{
	FklGrammerProduction* r=(FklGrammerProduction*)calloc(1,sizeof(FklGrammerProduction));
	r->left=left;
	r->len=len;
	r->units=(FklGrammerProductionUnit*)calloc(len,sizeof(FklGrammerProductionUnit));
	return r;
}

static inline FklGrammerProduction* create_grammer_prod_from_cstr(const char* str,FklSymbolTable* symbolTable,FklSymbolTable* termTable)
{
	const char* ss;
	size_t len;
	for(ss=str;isspace(*ss);ss++);
	for(len=0;!isspace(ss[len]);len++);
	FklSid_t left=fklAddSymbolCharBuf(ss,len,symbolTable)->id;
	ss+=len;
	len=0;
	FklPtrStack st=FKL_STACK_INIT;
	fklInitPtrStack(&st,8,8);
	while(*ss)
	{
		for(;isspace(*ss);ss++);
		for(len=0;ss[len]&&!isspace(ss[len]);len++);
		fklPushPtrStack(fklCreateString(len,ss),&st);
		ss+=len;
	}
	size_t prod_len=st.top;
	FklGrammerProduction* prod=create_empty_production(left,prod_len);
	for(uint32_t i=0;i<st.top;i++)
	{
		FklGrammerProductionUnit* u=&prod->units[i];
		FklString* s=st.base[i];
		switch(*(s->str))
		{
			case '#':
				u->term=1;
				u->nt=fklAddSymbolCharBuf(&s->str[1],s->size-1,termTable)->id;
				break;
			case '&':
				u->term=0;
				u->nt=fklAddSymbolCharBuf(&s->str[1],s->size-1,symbolTable)->id;
				break;
		}
		free(s);
	}
	fklUninitPtrStack(&st);
	return prod;
}

static inline FklHashTable* create_prod_hash_table()
{
	return fklCreateHashTable(&ProdHashMetaTable);
}

static inline int prod_unit_equal(const FklGrammerProductionUnit* u0,const FklGrammerProductionUnit* u1)
{
	return u0->term==u1->term
		&&u0->nt==u1->nt
		&&u0->nodel==u1->nodel
		&&u0->space==u1->space
		&&u0->repeat==u1->repeat;
}

static inline int prod_equal(const FklGrammerProduction* prod0,const FklGrammerProduction* prod1)
{
	if(prod0->len!=prod1->len)
		return 0;
	size_t len=prod0->len;
	FklGrammerProductionUnit* u0=prod0->units;
	FklGrammerProductionUnit* u1=prod1->units;
	for(size_t i=0;i<len;i++)
		if(!prod_unit_equal(&u0[i],&u1[i]))
			return 0;
	return 1;
}

static inline FklGrammerProduction* create_extra_production(FklSid_t start)
{
	FklGrammerProduction* prod=create_empty_production(0,1);
	FklGrammerProductionUnit* u=&prod->units[0];
	u->term=0;
	u->nt=start;
	return prod;
}

static inline void add_prod_to_grammer(FklLalr1Grammer* grammer,FklGrammerProduction* prod)
{
	FklSid_t left=prod->left;
	FklHashTable* productions=grammer->productions;
	ProdHashItem* item=fklGetHashItem(&left,productions);
	if(item)
	{
		FklGrammerProduction** pp=&item->prods;
		FklGrammerProduction* cur=NULL;
		for(;*pp;pp=&((*pp)->next),cur=*pp)
		{
			if(prod_equal(*pp,prod))
				break;
		}
		if(cur)
		{
			// prod->next=cur->next;
			// *pp=prod;
			// destroy_prod(cur);
			destroy_prod(prod);
		}
		else
		{
			prod->next=NULL;
			*pp=prod;
		}
	}
	else
	{
		if(!grammer->start)
		{
			grammer->start=left;
			FklGrammerProduction* extra_prod=create_extra_production(left);
			fklPutHashItem(&extra_prod->left,grammer->nonterminals);
			extra_prod->next=NULL;
			item=fklPutHashItem(&extra_prod->left,productions);
			item->prods=extra_prod;
		}
		fklPutHashItem(&left,grammer->nonterminals);
		prod->next=NULL;
		item=fklPutHashItem(&left,productions);
		item->prods=prod;
	}
}

static inline FklLalr1Grammer* create_empty_lalr1_grammer()
{
	FklLalr1Grammer* r=(FklLalr1Grammer*)calloc(1,sizeof(FklLalr1Grammer));
	r->terminals=fklCreateSymbolTable();
	r->nonterminals=fklCreateSidSet();
	r->productions=create_prod_hash_table();
	return r;
}

FklLalr1Grammer* fklCreateLalr1GrammerFromCstr(const char* str[],FklSymbolTable* st)
{
	FklLalr1Grammer* grammer=create_empty_lalr1_grammer();
	FklSymbolTable* tt=grammer->terminals;
	for(;*str;str++)
	{
		FklGrammerProduction* prod=create_grammer_prod_from_cstr(*str,st,tt);
		add_prod_to_grammer(grammer,prod);
	}
	return grammer;
}

static inline void print_prod_unit(FILE* fp,const FklGrammerProductionUnit* u,const FklSymbolTable* st,const FklSymbolTable* tt)
{
	if(u->term)
	{
		fputc('#',fp);
		fklPrintString(fklGetSymbolWithId(u->nt,tt)->symbol,fp);
	}
	else
	{
		fputc('&',fp);
		fklPrintString(fklGetSymbolWithId(u->nt,st)->symbol,fp);
	}
}

void fklPrintGrammerProduction(FILE* fp,const FklGrammerProduction* prod,const FklSymbolTable* st,const FklSymbolTable* tt)
{
	if(prod->left)
		fklPrintString(fklGetSymbolWithId(prod->left,st)->symbol,fp);
	else
		fputs("S'",fp);
	fputs(" ->",fp);
	size_t len=prod->len;
	FklGrammerProductionUnit* units=prod->units;
	for(size_t i=0;i<len;i++)
	{
		fputc(' ',fp);
		print_prod_unit(fp,&units[i],st,tt);
	}
	fputc('\n',fp);
}

void fklPrintLalr1Grammer(FILE* fp,const FklLalr1Grammer* grammer,FklSymbolTable* st)
{
	FklSymbolTable* tt=grammer->terminals;
	for(FklHashTableNodeList* list=grammer->productions->list;list;list=list->next)
	{
		FklGrammerProduction* prods=((ProdHashItem*)list->node->data)->prods;
		for(;prods;prods=prods->next)
			fklPrintGrammerProduction(fp,prods,st,tt);
	}
}

