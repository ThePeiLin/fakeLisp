#include"reader.h"
#include"tool.h"
#include<string.h>
#include<stdlib.h>

StringMatchPattern* newStringPattern(const char** parts,int32_t size)
{
	StringMatchPattern* tmp=(StringMatchPattern*)malloc(sizeof(StringMatchPattern));
	if(!tmp)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->size=size;
	char** tmParts=(char**)malloc(sizeof(char*)*size);
	if(!tmParts)errors(OUTOFMEMORY,__FILE__,__LINE__);
	int i=0;
	for(;i<size;i++)
		tmParts[i]=copyStr(parts[i]);
	tmp->parts=tmParts;
	return tmp;
}

void freeStringPattern(StringMatchPattern* o)
{
	int i=0;
	int32_t size=o->size;
	for(;i<size;i++)
		free(o->parts[i]);
	free(o->parts);
	free(o);
}

ReaderMacro* newReaderMacro(StringMatchPattern* pattern,AST_cptr* expression)
{
	ReaderMacro* tmp=(ReaderMacro*)malloc(sizeof(ReaderMacro));
	if(!tmp)errors(OUTOFMEMORY,__FILE__,__LINE__);
	tmp->pattern=StringMatchPattern;
	tmp->expression=expression;
	tmp->next=NULL;
	return tmp;
}

void freeReaderMacro(ReaderMacro* tmp)
{
	freeStringPattern(tmp->pattern);
	deleteCptr(tmp->expression);
	free(tmp);
}
