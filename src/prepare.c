static macro* Head=NULL
int macroMatch(cptr*)
{
}

macro* createMacro(cptr* format,cptr* express)
{
	macro* tmp=NULL;
	if(!(tmp=(macro*)malloc(sizeof(macro))))
		errors(OUTOFMEMORY);
	tmp->format=format;
	tmp->express=express;
	tmp->next=NULL;
	return tmp;
}

int addMacro(macro* obj)
{
	macro* current=(Head==NULL)?NULL:Head;
	while(current->next!=NULL)current=current->next;
	current->next=obj;
	Head=(Head==NULL)?current:Head;
}
