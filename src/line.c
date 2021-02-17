#include"line.h"
#include"common.h"
#include"fakedef.h"
#include<stdlib.h>

static int LineNumTabNodeCmp(LineNumTabNode* f,LineNumTabNode* s)
{
	if(f->id!=s->id)
		return (f->id)-(s->id);
	else if(f->fid!=s->fid)
		return (f->fid)-(s->fid);
	else if(f->line!=s->line)
		return (f->line)-(f->line);
	return (f->scp)-(s->scp);
}

static int isInSameLine(LineNumTabNode* f,LineNumTabNode* s)
{
	if(f->id!=s->id)
		return (f->id)-(s->id);
	else if(f->fid!=s->fid)
		return (f->fid)->(s->fid);
	else
		return (f->line)-(s->line);
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

LineNumTabNode* newLineNumNode(int32_t id,int32_t fid,int32_t scp,int32_t cpc,int32_t line)
{
	LineNumTabNode* t=(LineNumTabNode*)malloc(sizeof(LineNumTabNode));
	if(!t)
		errors("newLineNumNode",__FILE__,__LINE__);
	t->id=id;
	t->fid=fid;
	t->scp=scp;
	t->cpc=cpc;
	t->line=line;
	return t;
}

LineNumTabNode* addLineNumNode(LineNumTabNode* node,LineNumberTable* table)
{
	if(!table->list)
	{
		table->size=1;
		table->list=(LineNumTabNode**)malloc(sizeof(LineNumTabNode*)*1);
		if(!table->list)
			errors("addLineNumNode",__FILE__,__LINE__);
		table->list[0]=node;
	}
	else
	{
		int32_t l=0;
		int32_t h=table->size-1;
		int32_t mid;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			if(LineNumTabNodeCmp(table->list[mid],node)>=0)
				h=mid-1;
			else
				l=mid+1;
		}
		if(isInSameLine(table->list[mid],node)==0)
		{
			LineNumTabNodeAppd(table->list[mid],node);
			return table->list[mid];
		}
		else if(LineNumTabNodeCmp(table->list[mid],node)<0)
			mid++;
		table->size+=1;
		int32_t i=table->size-1;
		table->list=(LineNumTabNode**)realloc(table->list,sizeof(LineNumTabNode*)*table->size);
		if(!table->list)
			errors("addLineNumNode",__FILE__,__LINE__);
		for(;i>mid;i++)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
	}
	return node;
}

void LineNumTabNodeAppd(LineNumTabNode* f,LineNumTabNode* s)
{
	f->scp=(f->scp<s->scp)?f->scp:s->scp;
	f->cpc=f->cpc+s->cpc;
}

void freeLineNumTabNode(LineNumTabNode* n)
{
	free(n);
}

void freeLineNumberTable(LineNumberTable* t)
{
	int32_t i=0;
	for(;i<t->size;i++)
		freeLineNumTabNode(t->list[i]);
	free(t->list);
	free(t);
}

LineNumTabNode* findLineNumTabNode(int32_t id,int32_t cp,LineNumberTable* t)
{
	int32_t i=0;
	for(;i<t->size;i++)
	{
		if(t->list[i]->id==id&&t->list[i]->scp=<cp&&(t->list[i]->scp+t->list[i]->cpc)>=cp)
			return t->list[i];
	}
	return NULL;
}
